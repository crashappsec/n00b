#define N00B_USE_INTERNAL_API
#define ONLY_READERS -1

#include "n00b.h"

typedef struct gts_atomic_t {
    int16_t requesting;
    int16_t world_stopped;
    int32_t readers;
    int64_t write_lock;
} gts_state_t;

_Atomic gts_state_t gts_state;
bool                n00b_abort_signal     = false;
bool                n00b_world_is_stopped = false;

const struct timespec gts_poll_interval = {
    .tv_sec  = 0,
    .tv_nsec = N00B_GTS_POLL_DURATION_NS,
};

pthread_rwlock_t runlock          = PTHREAD_RWLOCK_INITIALIZER;
_Atomic int      gts_read_request = 0;

void
n00b_dump_gts(void)
{
}

void
n00b_initialize_global_thread_info(void)
{
    printf("world is stopped = %d\n", n00b_world_is_stopped);
}

void
n00b_gts_might_stop(void)
{
}

void
n00b_gts_suspend(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi->gts_reader) {
        pthread_rwlock_unlock(&runlock);
        tsi->gts_reader = false;
    }
}

void
n00b_gts_resume(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    assert(!tsi->gts_reader);
    pthread_rwlock_rdlock(&runlock);
    tsi->gts_reader = true;
}

void
n00b_gts_checkin(void)
{
    // Give an opening for people to grab the write lock if they've
    // been waiting for the gloval lock.

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        // Should only be during an abort.
        return;
    }

    if (tsi->gts_reader) {
        pthread_rwlock_unlock(&runlock);
        sched_yield();
        pthread_rwlock_rdlock(&runlock);
    }
}

void
n00b_gts_wont_stop(void)
{
}

void
_n00b_gts_stop_the_world(char *f, int l)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (tsi->gts_reader) {
        pthread_rwlock_unlock(&runlock);
        tsi->gts_reader = false;
    }

    int x;
    if ((x = (tsi->gts_nest++)) == 3) {
        // Nesting too deep.
        abort();
    }
    pthread_rwlock_wrlock(&runlock);
    n00b_world_is_stopped = true;
}

void
_n00b_gts_restart_the_world(char *f, int l)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    assert(!tsi->gts_reader);
    assert(tsi->gts_nest);

    if (--tsi->gts_nest) {
        return;
    }

    n00b_world_is_stopped = false;
    pthread_rwlock_unlock(&runlock);
    sched_yield();
    pthread_rwlock_rdlock(&runlock);
    tsi->gts_reader = true;
}

void
n00b_gts_start(void)
{
    n00b_gts_resume();
}

void
n00b_gts_notify_abort(void)
{
    n00b_abort_signal = true;
}

void
n00b_gts_quit(n00b_tsi_t *tsi)
{
    if (tsi->gts_reader || tsi->gts_nest) {
        pthread_rwlock_unlock(&runlock);
    }

    /*
    gts_state_t read = atomic_read(&gts_state);
    n00b_barrier();

    if (tsi->thread_id == read.write_lock) {
        assert(tsi->thread_id != ONLY_READERS);
        if (read.requesting) {
            n00b_gts_wont_stop();
        }
        else {
            n00b_gts_restart_the_world();
        }
    }

    if (tsi->gts_reader) {
        deregister_runner(tsi);
        }*/
}
