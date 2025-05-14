#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "util/plock.h"

bool        n00b_abort_signal     = false;
uint64_t    gti_lock              = 0;
int64_t     n00b_world_is_stopped = 0;
_Atomic int r_holders             = 0;

const struct timespec gts_poll_interval = {
    .tv_sec  = 0,
    .tv_nsec = N00B_GTS_POLL_DURATION_NS,
};

void
n00b_initialize_global_thread_info(void)
{
    n00b_set_thread_state(n00b_gts_go);
}

void
_n00b_gts_resume(char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi->gti_w) {
        return;
    }

    if (!tsi->gti_r++) {
        tsi->gti_file = file;
        tsi->gti_line = line;
        n00b_set_thread_state(n00b_gts_go);
        pl_take_r(&gti_lock);
        atomic_fetch_add(&r_holders, 1);
    }
}

void
n00b_gts_reacquire(void)
{
    // Give an opening for people to grab the write lock if they've
    // been waiting.
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi->gti_r && !tsi->gti_w) {
        pl_drop_r(&gti_lock);
        n00b_set_thread_state(n00b_gts_go);
        pl_take_r(&gti_lock);
    }
}

void
_n00b_gts_suspend(char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi->gti_w) {
        return;
    }
    if (!--tsi->gti_r) {
        pl_drop_r(&gti_lock);
        tsi->gti_file = file;
        tsi->gti_line = line;
        atomic_fetch_add(&r_holders, -1);
    }
    n00b_set_thread_state(n00b_gts_go);
    n00b_thread_stack_region(n00b_thread_self());
}

void
_n00b_gts_might_stop(char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (n00b_abort_signal) {
        n00b_thread_exit(NULL);
    }

    if (tsi->gti_w) {
        return;
    }

    tsi->gti_file = file;
    tsi->gti_line = line;
    tsi->gti_s    = true;

    if (tsi->gti_r) {
        while (!pl_try_rtos(&gti_lock)) {
            n00b_gts_reacquire();
        }
    }
    else {
        pl_take_s(&gti_lock);
    }
    tsi->gti_s = true;
}

void
_n00b_gts_wont_stop(char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    assert(tsi->gti_s);

    if (tsi->gti_r) {
        pl_stor(&gti_lock);
    }
    else {
        pl_drop_s(&gti_lock);
    }
    tsi->gti_file = file;
    tsi->gti_line = line;
    tsi->gti_s    = false;
}

void
_n00b_gts_stop_the_world(char *file, int line)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (n00b_abort_signal) {
        n00b_thread_exit(NULL);
    }
    if (tsi->gti_w) {
        n00b_world_is_stopped++;
        return;
    }

    if (tsi->gti_s) {
        pl_stow(&gti_lock);
        tsi->gti_s = false;
    }
    else {
        pl_take_w(&gti_lock);
    }

    tsi->gti_file = file;
    tsi->gti_line = line;
    tsi->gti_w    = true;
    n00b_world_is_stopped++;
    n00b_set_thread_state(n00b_gts_go);
}

void
_n00b_gts_restart_the_world(char *file, int line)
{
    if (--n00b_world_is_stopped) {
        return;
    }

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    tsi->gti_file = file;
    tsi->gti_line = line;

    if (tsi->gti_r) {
        pl_wtor(&gti_lock);
    }
    else {
        pl_wtos(&gti_lock);
        pl_drop_s(&gti_lock);
    }

    tsi->gti_w = false;
    tsi->gti_s = false;
}

void
_n00b_gts_start(char *file, int line)
{
    _n00b_gts_resume(file, line);
}

void
n00b_gts_notify_abort(void)
{
    n00b_abort_signal = true;
}

void
n00b_gts_quit(n00b_tsi_t *tsi)
{
    if (tsi->gti_r) {
        pl_drop_r(&gti_lock);
    }
    if (tsi->gti_s) {
        pl_drop_s(&gti_lock);
    }
    if (tsi->gti_w) {
        pl_drop_w(&gti_lock);
    }
}

void
n00b_dump_gti(void)
{
    printf("Lock state: %p\n", (void *)gti_lock);

    for (int i = 0; i < HATRACK_THREADS_MAX; i++) {
        n00b_thread_t *t = atomic_read(&n00b_global_thread_list[i]);

        if (!t) {
            continue;
        }

        n00b_tsi_t *tsi = t->tsi;

        char state = 0;
        if (tsi->gti_w) {
            state = 'w';
            assert(!tsi->gti_s);
        }
        else {
            if (tsi->gti_s) {
                state = 's';
            }
        }

        if (!state) {
            if (tsi->gti_r) {
                state = 'r';
            }
            else {
                state = '-';
            }
        }

        printf("%p: %c %d\n", t, state, tsi->gti_r);
    }
}
