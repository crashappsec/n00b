#define N00B_USE_INTERNAL_API
#include "n00b.h"

// The eopch really is simply to avoid ABA problems here; we don't
// really care about the ordering (random values would do too), we
// just care that two threads won't be using the same epoch at the
// same time.
static _Atomic uint32_t                  coop_epoch = 0;
static _Atomic n00b_global_thread_info_t gti;
static _Atomic int                       gti_nesting_level = 0;
bool                                     n00b_abort_signal = false;

const struct timespec gts_poll_interval = {
    .tv_sec  = 0,
    .tv_nsec = N00B_GTS_POLL_DURATION_NS,
};

static inline void
capture_stack_bounds(void)
{
    n00b_thread_stack_region(n00b_thread_self());
}

void
n00b_initialize_global_thread_info(void)
{
    const n00b_global_thread_info_t tinit = {
        .leader  = NULL,
        .epoch   = 0,
        .running = 0,
        .state   = n00b_gts_go,
    };
    atomic_store(&gti, tinit);
    n00b_set_thread_state(n00b_gts_go);
}

static inline n00b_global_thread_info_t
gts_wait_for_go_state(n00b_global_thread_info_t info)
{
    while (info.state != n00b_gts_go) {
        nanosleep(&gts_poll_interval, NULL);
        info = atomic_read(&gti);
    }
    return info;
}

static inline n00b_global_thread_info_t
gts_wait_for_all_stops(n00b_global_thread_info_t info)
{
    n00b_assert(info.state == n00b_gts_yield);

    while (info.running > 1) {
        nanosleep(&gts_poll_interval, NULL);
        info = atomic_read(&gti);
        n00b_assert(info.state == n00b_gts_yield);
    }

    return info;
}

void
n00b_gts_resume(void)
{
    if (__n00b_collector_running) {
        return;
    }
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (tsi->thread_state == n00b_gts_exited) {
        abort();
        return;
    }

    int count_delta = 1;

    // This makes adding calls that will make nested calls to this API
    // simpler than having everything check 4 states; we only check in
    // suspend / resume and let other thigns set the state to 'go'.
    //
    // thread_sleeping is how suspend / resume determines if you're
    // REALLY asleep.
    if (!tsi->thread_sleeping) {
        count_delta = 0;
    }

    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);
    n00b_global_thread_info_t desired;

    do {
        expected = gts_wait_for_go_state(expected);
        n00b_assert(expected.state == n00b_gts_go);
        n00b_assert(!expected.leader);

        desired.leader  = NULL;
        desired.epoch   = epoch;
        desired.running = expected.running + count_delta;
        desired.state   = n00b_gts_go;
        assert(desired.running >= 0);
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    tsi->thread_state    = n00b_gts_go;
    tsi->thread_sleeping = false;
}

void
n00b_gts_suspend(void)
{
    if (__n00b_collector_running) {
        return;
    }

    int count_delta = -1;

    n00b_tsi_t                *tsi = n00b_get_tsi_ptr();
    n00b_global_thread_state_t ts  = tsi->thread_state;

    // Is this a nested suspend?  If so, we won't remove ourselves
    // from the running list again.
    if (tsi->thread_sleeping) {
        count_delta = 0;
    }
    else {
        tsi->thread_sleeping = true;
    }

    if (ts != n00b_gts_go) {
        return;
    }
    n00b_assert(ts != n00b_gts_exited);

    capture_stack_bounds();

    n00b_global_thread_info_t desired;
    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);

    do {
        desired         = expected;
        desired.epoch   = epoch;
        desired.running = desired.running + count_delta;
        assert(desired.running >= 0);
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    tsi->thread_state = n00b_gts_yield;
}

void
n00b_gts_checkin(void)
{
    if (__n00b_collector_running) {
        return;
    }

    if (n00b_abort_signal) {
        n00b_thread_exit(NULL);
    }

    n00b_global_thread_state_t ts = n00b_get_thread_state();

    if (ts == n00b_gts_stop || ts == n00b_gts_exited) {
        return;
    }

    n00b_assert(ts == n00b_gts_go);

    n00b_global_thread_info_t expected = atomic_read(&gti);

    if (expected.state != n00b_gts_go) {
        n00b_assert(expected.state == n00b_gts_yield);
        n00b_gts_suspend();
        n00b_gts_resume();
    }
}

void
n00b_gts_stop_the_world(void)
{
    if (n00b_abort_signal) {
        n00b_thread_exit(NULL);
    }

    n00b_tsi_t *tsi         = n00b_get_tsi_ptr();
    int         count_delta = 0;

    n00b_assert(tsi->thread_state != n00b_gts_exited);

    n00b_global_thread_info_t desired;
    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);
    n00b_thread_t            *self     = n00b_thread_self();

    n00b_assert(self);

    if (tsi->thread_state == n00b_gts_stop) {
        n00b_assert(expected.state == n00b_gts_stop);
        // Can happen during an abort.
        if (expected.leader != self) {
            _exit(0);
        }
        atomic_fetch_add(&gti_nesting_level, 1);
        return;
    }

    // Wake us up if we're asleep.
    if (tsi->thread_sleeping) {
        count_delta          = 1;
        tsi->thread_sleeping = false;
    }

    do {
        n00b_gts_checkin();

        desired.leader  = self;
        desired.epoch   = epoch;
        desired.running = expected.running + count_delta;
        desired.state   = n00b_gts_yield;
        assert(desired.running > 0);
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    expected = gts_wait_for_all_stops(desired);
    epoch    = atomic_fetch_add(&coop_epoch, 1);
    atomic_store(&gti_nesting_level, 0);

    do {
        nanosleep(&gts_poll_interval, NULL);
        desired       = atomic_read(&gti);
        desired.state = n00b_gts_stop;
        desired.epoch = epoch;
        assert(desired.running == 1);
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    capture_stack_bounds();
    atomic_store(&gti_nesting_level, 1);
    tsi->thread_state = n00b_gts_stop;
}

void
n00b_gts_restart_the_world(void)
{
    n00b_global_thread_info_t desired;
    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);

    // The nesting level returned is the nesting level we're closing
    // out.  So if it's time to restart, we'll get returned 1, even
    // though the op will change it to be 0.
    if (atomic_fetch_add(&gti_nesting_level, -1) > 1) {
        n00b_assert(expected.state == n00b_gts_stop);
        n00b_assert(expected.leader == n00b_thread_self());
        return;
    }
    do {
        n00b_assert(expected.state == n00b_gts_stop);
        n00b_assert(expected.leader == n00b_thread_self());
        n00b_assert(atomic_read(&gti_nesting_level) == 0);

        desired.leader  = NULL;
        desired.epoch   = epoch;
        desired.running = expected.running;
        desired.state   = n00b_gts_go;
        assert(desired.running >= 0);
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    n00b_get_tsi_ptr()->thread_state = n00b_gts_go;

    if (n00b_abort_signal) {
        n00b_thread_exit(NULL);
    }
}

void
n00b_gts_start(void)
{
    n00b_get_tsi_ptr()->thread_state = n00b_gts_stop;
    n00b_gts_resume();
}

void
n00b_gts_notify_abort(void)
{
    n00b_gts_stop_the_world();
    n00b_abort_signal = true;
    n00b_gts_restart_the_world();
}

void
n00b_gts_quit(n00b_tsi_t *tsi)
{
    if (tsi->thread_state != n00b_gts_exited) {
        if (tsi->thread_sleeping) {
            tsi->thread_state = n00b_gts_exited;
            return;
        }

        n00b_global_thread_info_t desired;
        n00b_global_thread_info_t expected = atomic_read(&gti);
        uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);

        do {
            desired         = expected;
            desired.epoch   = epoch;
            desired.running = desired.running - 1;
        } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

        tsi->thread_state = n00b_gts_exited;
    }
}

bool
n00b_is_world_stopped(void)
{
    return atomic_read(&gti_nesting_level) != 0;
}
