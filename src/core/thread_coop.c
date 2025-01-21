#define N00B_USE_INTERNAL_API

#include "n00b.h"

// This file mainly helps ensure that we can assign a single thread
// the ability to perform tasks without any risk of contention
// (particularly intended for garbage collection).
//
// Threads at NO risk of contention (due to sleep, IO blocking, or
// whatever) can leave the 'run' state, and then return to it when
// they're ready.
//
//
// This also helps us do things like run I/O callbacks without
// worrying about blocking-- basically, we actually allow the block,
// but update the I/O callback thread.

typedef struct gti_t {
    uint32_t running; // Includes 'waiting' threads, but not self-blocked.
    uint32_t waiting; // Waiting for the world to stop and / or resume.
    uint64_t requestor;
} gti_t;

static _Atomic gti_t    global_thread_info;
static n00b_condition_t world_is_stopped;
static n00b_condition_t world_is_restarting;
static bool             is_world_stopped = true;

// These are designed to minimize contention by only requiring locks
// when someone is actually trying to stop the world.
void
n00b_init_thread_accounting(void)
{
    n00b_raw_condition_init(&world_is_stopped);
    n00b_raw_condition_init(&world_is_restarting);
    gti_t init = {0, 0, 0};
    atomic_store(&global_thread_info, init);
}

bool
n00b_is_world_stopped(void)
{
    return is_world_stopped;
}

static inline void
capture_stack_bounds(void)
{
    n00b_thread_stack_region(n00b_thread_self());
}

static inline void
defer_to_requestor(void)
{
    // Here, we have to acquire the world_is_restarting lock and also
    // the count_modification lock. If we make the thread count equal
    // to the wait count, then we signal the requestor before we go to
    // sleep.

    if (is_world_stopped) {
        return;
    }

    //    printf("dtr...\n");
    n00b_condition_lock_acquire_raw(&world_is_restarting);

    gti_t expected = atomic_read(&global_thread_info);
    gti_t tinfo;

    /*    cprintf("defer start: %p: %d vs %d\n",
                n00b_thread_self(),
                expected.waiting,
                expected.running);
    */

    n00b_condition_lock_acquire_raw(&world_is_stopped);

    do {
        tinfo = expected;
        tinfo.waiting++;
    } while (!CAS(&global_thread_info, &expected, tinfo));

    /*    cprintf("tinfo: %p: %d vs %d\n",
                n00b_thread_self(),
                tinfo.waiting,
                tinfo.running);
    */

    if (tinfo.waiting == tinfo.running) {
        //        printf("NOTIFY.\n");
        n00b_condition_notify_all(&world_is_stopped);
        //        printf("RELEASE.\n");
    }

    n00b_condition_lock_release_all(&world_is_stopped);
    capture_stack_bounds();
    //    printf("Wait for restart.\n");
    n00b_condition_wait_raw(&world_is_restarting);
    n00b_condition_lock_release_all(&world_is_restarting);
    //    printf("World restarted.");
}

void
n00b_thread_enter_run_state(void)
{
    gti_t tinfo;
    gti_t expected;

    expected = atomic_read(&global_thread_info);

    do {
        while (expected.requestor) {
            n00b_condition_lock_acquire_raw(&world_is_restarting);
            tinfo = atomic_read(&global_thread_info);
            if (tinfo.requestor == expected.requestor) {
                capture_stack_bounds();
                n00b_condition_wait_raw(&world_is_restarting);
                expected = atomic_read(&global_thread_info);
            }
            else {
                expected = tinfo;
            }
            n00b_condition_lock_release_all(&world_is_restarting);
        }
        tinfo = expected;
        tinfo.running += 1;
    } while (!CAS(&global_thread_info, &expected, tinfo));
}

extern void n00b_thread_unlock_all(void);

void
n00b_thread_leave_run_state(void)
{
    gti_t tinfo;
    gti_t expected;

    // Just in case something needs to GC scan while we're waiting.
    if (n00b_thread_self()) {
        capture_stack_bounds();
    }

    // We're about to go to sleep; don't block other callbacks from
    // running if we don't have to.
    n00b_ioqueue_dont_block_callbacks();

    expected = atomic_read(&global_thread_info);

    do {
        while (expected.requestor) {
            defer_to_requestor();
            expected = atomic_read(&global_thread_info);
        }

        tinfo = expected;
        tinfo.running -= 1;
    } while (!CAS(&global_thread_info, &expected, tinfo));
    n00b_thread_unlock_all();
}

void
n00b_thread_pause_if_stop_requested(void)
{
    gti_t expected = atomic_read(&global_thread_info);

    if (expected.requestor
        && expected.requestor != (uint64_t)n00b_thread_self()) {
        // Temp pin of the main heap to avoid recursion if a collection
        // is needed inside.

        N00B_USING_HEAP(n00b_thread_master_heap, defer_to_requestor());
    }
}

void
n00b_stop_the_world(void)
{
    gti_t          tinfo;
    gti_t          expected;
    n00b_thread_t *self = n00b_thread_self();

    expected = atomic_read(&global_thread_info);

    if (is_world_stopped) {
        return;
    }

    //    cprintf("stw: %p: %d\n", n00b_thread_self(), expected.waiting);
    //    cprintf("time to stw!\n");
    // We're the only thread that can now acquire this until the
    // counts match, so grab it before we change the count to
    // avoid potential deadlock.
    n00b_condition_lock_acquire_raw(&world_is_stopped);

    do {
        while (expected.requestor && expected.requestor != (uint64_t)self) {
            defer_to_requestor();
            expected = atomic_read(&global_thread_info);
        }
        tinfo.requestor = (uint64_t)self;
        tinfo.waiting   = 1;
        tinfo.running   = expected.running;
    } while (!CAS(&global_thread_info, &expected, tinfo));

    /*
      cprintf("stw: %p: %d (wait for stop)\n",
                n00b_thread_self(),
                tinfo.waiting);
    */
    expected = atomic_read(&global_thread_info);

    //    printf("stw: %d vs %d\n", expected.waiting, expected.running);
    if (expected.waiting != expected.running) {
        n00b_condition_wait_raw(&world_is_stopped);
    }
    //    cprintf("World is my oyster.\n");

    is_world_stopped = true;
    n00b_condition_lock_release_all(&world_is_stopped);

    tinfo.requestor = 0;
    atomic_store(&global_thread_info, tinfo);

    expected = atomic_read(&global_thread_info);
    // cprintf("post stw: %p: %d\n", n00b_thread_self(), tinfo.waiting);
    // cprintf("post stw: running: %d\n", tinfo.running);
}

void
n00b_restart_the_world(void)
{
    n00b_condition_lock_acquire_raw(&world_is_restarting);
    // This is just to block any new starters.
    gti_t expected = atomic_read(&global_thread_info);
    gti_t tinfo;

    // Waking threads could be trying to update this.
    do {
        tinfo.requestor = 0;
        tinfo.waiting   = 0;
        tinfo.running   = expected.running;
    } while (!CAS(&global_thread_info, &expected, tinfo));

    is_world_stopped = false;
    // cprintf("About to restart w/ %d processes.\n", tinfo.running);
    n00b_condition_notify_all(&world_is_restarting);
    n00b_condition_lock_release_all(&world_is_restarting);
}
