#define N00B_USE_INTERNAL_API
#include "n00b.h"

// The eopch really is simply to avoid ABA problems here; we don't
// really care about the ordering (random values would do too), we
// just care that two threads won't be using the same epoch at the
// same time.
static _Atomic uint32_t                    coop_epoch = 0;
static _Atomic n00b_global_thread_info_t   gti;
static __thread n00b_global_thread_state_t thread_state = n00b_gts_go;

static _Atomic int gti_nesting_level = 0;

#define only_if_world_runs()                                                \
    if (thread_state == n00b_gts_stop || thread_state == n00b_gts_exited) { \
        return;                                                             \
    }

n00b_global_thread_state_t
n00b_get_thread_state(void)
{
    return thread_state;
}

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
    thread_state = n00b_gts_go;

    // Setup the main thread.
    n00b_thread_bootstrap_initialization();
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
    only_if_world_runs();
    n00b_assert(thread_state != n00b_gts_go);

    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);
    n00b_global_thread_info_t desired;

    do {
        expected = gts_wait_for_go_state(expected);
        n00b_assert(expected.state == n00b_gts_go);
        n00b_assert(!expected.leader);

        desired.leader  = NULL;
        desired.epoch   = epoch;
        desired.running = expected.running + 1;
        desired.state   = n00b_gts_go;
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    thread_state = n00b_gts_go;
}

void
n00b_gts_suspend(void)
{
    only_if_world_runs();
    if (thread_state == n00b_gts_go) {
        capture_stack_bounds();
    }
    else {
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

    thread_state = n00b_gts_yield;
}

void
n00b_gts_checkin(void)
{
    only_if_world_runs();
    n00b_assert(thread_state == n00b_gts_go);

    n00b_global_thread_info_t expected = atomic_read(&gti);

    if (expected.state != n00b_gts_go) {
        n00b_assert(expected.state == n00b_gts_yield);
        n00b_gts_suspend();
        n00b_gts_resume();
    }
}

static inline void
gts_begin_stw(void)
{
    n00b_global_thread_info_t desired;
    n00b_global_thread_info_t expected = atomic_read(&gti);
    uint32_t                  epoch    = atomic_fetch_add(&coop_epoch, 1);
    n00b_thread_t            *self     = n00b_thread_self();

    n00b_assert(self);

    if (thread_state == n00b_gts_stop) {
        n00b_assert(expected.state == n00b_gts_stop);
        n00b_assert(expected.leader == self);
        atomic_fetch_add(&gti_nesting_level, 1);
        return;
    }

    n00b_assert(expected.state != n00b_gts_stop);

    do {
        n00b_gts_checkin();
        desired.leader  = self;
        desired.epoch   = epoch;
        desired.running = expected.running;
        desired.state   = n00b_gts_yield;
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    expected = gts_wait_for_all_stops(desired);
    epoch    = atomic_fetch_add(&coop_epoch, 1);
    atomic_store(&gti_nesting_level, 0);

    do {
        nanosleep(&gts_poll_interval, NULL);
        desired       = atomic_read(&gti);
        desired.state = n00b_gts_stop;
        desired.epoch = epoch;
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    capture_stack_bounds();
    atomic_store(&gti_nesting_level, 1);
    thread_state = n00b_gts_stop;
}

static inline void
gts_end_stw(void)
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
    } while (!atomic_compare_exchange_strong(&gti, &expected, desired));

    thread_state = n00b_gts_go;
}

void
n00b_gts_start(void)
{
    // The initial thread comes pre-started, too bulky to special case it,
    // but any internal state discrepencies are checked via assert.
    if (thread_state == n00b_gts_go) {
        return;
    }
    n00b_gts_resume();
}

void
n00b_gts_quit(void)
{
    thread_state = n00b_gts_exited;
    n00b_gts_suspend();
}

void
n00b_gts_stop_the_world(void)
{
    gts_begin_stw();
}

void
n00b_gts_restart_the_world(void)
{
    gts_end_stw();
}

bool
n00b_is_world_stopped(void)
{
    return atomic_read(&gti_nesting_level) != 0;
}
