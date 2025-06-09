// The global interpreter lock only needs to run at times where it's
// important that all threads are stopped / synchronized.
//
// This is mainly (but not exclusively) when we need to do garbage
// collection; we don't want threads to be mucking w/ memory while we
// are copying data.
//
// We generally would like to avoid contention and cache
// synchronization, so instead of one global lock, the GIL works like this:
//
// There is one 'stop the world' mutex that must be acquired before
// you can actually begin the process. Then, when you have the mutex,
// you iterate through the global list of threads, and set the GIL
// bit in their self-lock futex. Every thread that checks in or.

// 'self-lock' futex to the value 0xffffffff.
//
// Every time a thread is about to call a function that will suspend
// it, any time it uses memory management routines, and every loop
// through the interpreter, the thread will store ~0 into
// tsi->wait_futex to indicate it's in the 'self-futex'. If the
// sentinel has been added to the futex by the GIL, then that thread
// will block until the GIL finishes the stop the world process.
//
// After the GIL runs through all threads once, it effectively
// busy-waits.  It keeps iterating through all threads, looking to see
// if they're suspended on their own futex, or otherwise waiting.
//
// For waiting threads, they also ensure wait on their self-lock when
// they resume, so once the GIL thread has placed the lock, as long as
// the thread is still in a wait state, all is good.
//
// At that point, the GIL thread proceeds, until it is time to restart
// the world.
//
// When it comes time to do that, it goes back through and resets
// the self-lock for each thread to its expected value.
//
// For any thread that was blocked on their self-lock, they then
// ensure that the GIL is unlocked before proceeding.
//
// We could use a RW lock for the GIL instead; that may end up just as
// good; yes, there'd be a lot more reading of the same memory
// address, but it wouldn't often be modified (on stop-the-world,
// suspend and resume), so probably not much of an issue.
//
// Still, this is simple enough, and doesn't allow nesting of suspends
// / resumes, which I prefer. I might not change it.

#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_futex_t n00b_gil    = N00B_NO_OWNER;
static int          stw_nesting = 0;

static void mheap_abandon(void);

bool
n00b_world_is_stopped(void)
{
    return atomic_read(&n00b_gil) != (uint32_t)N00B_NO_OWNER;
}

void
N00B_DBG_DECL(n00b_stop_the_world)
{
    n00b_thread_suspend();

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        tsi = n00b_init_self_tsi();
    }
    int32_t tid   = (int32_t)tsi->thread_id;
    int32_t owner = atomic_read(&n00b_gil);

    if (owner == tid) {
        n00b_dlog_gil2("Nested stw (#%d); file = %s; line = %d",
                       stw_nesting,
                       __file,
                       __line);
        stw_nesting++;
        return;
    }

    int32_t expected = N00B_NO_OWNER;

    do {
        int32_t cur = (int32_t)atomic_read(&n00b_gil);

        while (cur != N00B_NO_OWNER) {
            n00b_dlog_gil("stw: contention: current owner: %d", cur);
            cur = atomic_read(&n00b_gil);
            n00b_futex_wait(&n00b_gil, cur, N00B_NO_OWNER);
        }
    } while (!CAS(&n00b_gil, (uint32_t *)&expected, tid));

    n00b_dlog_gil1("stw(start) #%d; file = %s; line = %d",
                   stw_nesting,
                   __file,
                   __line);

    assert(atomic_read(&n00b_gil) == (uint32_t)tid);

    int n = HATRACK_THREADS_MAX;

    // Loop through every single thread and add the GIL bit to their
    // state, which will alert them to go wait on the GIL.

    n00b_thread_t *thread;
    n00b_tsi_t    *t;

    while (n--) {
        thread = atomic_read(&n00b_global_thread_list[n]);
        if (!thread || !thread->tsi) {
            continue;
        }

        t = thread->tsi;

        if (t->thread_id == tid) {
            continue;
        }

        atomic_fetch_or(&t->self_lock, N00B_GIL);
    }

    stw_nesting = 1;

    n00b_dlog_gil2("op: stw(end); level: %d->%d; file = %s; line = %d",
                   stw_nesting - 1,
                   stw_nesting,
                   __file,
                   __line);

    n00b_thread_resume();
}

void
N00B_DBG_DECL(n00b_restart_the_world)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    int32_t tid   = (int32_t)tsi->thread_id;
    int32_t owner = atomic_read(&n00b_gil);

    if (owner != tid) {
        fprintf(stderr, "System error: thread w/o GIL is running.\n");
        abort();
    }

    assert(stw_nesting > 0);

    if (--stw_nesting) {
        return;
    }

#if 0
    n00b_dlog_gil2("rtw(start); level: %d->%d; file = %s; line = %d",
                   stw_nesting,
                   stw_nesting - 1,
                   __file,
                   __line);
#endif

    int n = HATRACK_THREADS_MAX;

    n00b_thread_t *thread;
    n00b_tsi_t    *t;

    while (n--) {
        thread = atomic_read(&n00b_global_thread_list[n]);
        if (!thread || !thread->tsi) {
            continue;
        }

        t = thread->tsi;

        if (t->thread_id == tid) {
            continue;
        }

        atomic_fetch_and(&t->self_lock, ~(N00B_GIL | N00B_BLOCKING));
    }

    atomic_store(&n00b_gil, N00B_NO_OWNER);

    n00b_dlog_gil2("rtw(end); level: %d->%d; file = %s; line = %d",
                   stw_nesting,
                   stw_nesting - 1,
                   __file,
                   __line);
    mheap_abandon();
    n00b_futex_wake(&n00b_gil, true);
}

const struct timespec gil_check_timeout = {
    .tv_sec  = 0,
    .tv_nsec = 10000,
};

static inline void
wait_for_gil_release(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    n00b_thread_stack_region(n00b_thread_self());
    int32_t cur = atomic_read(&n00b_gil);

    if (cur == tsi->thread_id) {
        return;
    }

    atomic_fetch_or(&tsi->self_lock, N00B_BLOCKING);

    while (cur != N00B_NO_OWNER) {
        // Use the version that doesn't check the GIL when it's
        // signaled!
        n00b_futex_wait_timespec(&n00b_gil, cur, (void *)&gil_check_timeout);
        cur = atomic_read(&n00b_gil);
    }

    atomic_fetch_and(&tsi->self_lock, ~N00B_BLOCKING);
}

void
n00b_thread_checkin(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        exit(-1);
    }

    int val = atomic_read(&tsi->self_lock);

    if (!val) {
        return;
    }

    if (atomic_read(&tsi->self_lock) & N00B_GIL) {
        atomic_fetch_or(&tsi->self_lock, N00B_BLOCKING);
        wait_for_gil_release();
    }
}

// 'suspend' means we are about to go into some kind of state where we
// might not check in (at least not soon). We actually can still keep
// running, but we promise that we are not doing ANYTHING in scope for
// a GIL. So we cannot be using heap memory or locks, etc.
//
// The intent here is we call this, then call sleep(), poll() or
// any other potentially blocking call we need to make, then call
// n00b_thread_resume() immediately on getting woken.
void
N00B_DBG_DECL(n00b_thread_suspend)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (!tsi) {
        exit(-1);
    }

    int val = atomic_read(&n00b_gil);

    if (val == tsi->thread_id) {
#if 0
        n00b_dlog_gil2("ignored op: suspend; file = %s; line = %d",
                       __file,
                       __line);
#endif
        return;
    }

#if 0
    n00b_dlog_gil2("op: suspend; file = %s; line = %d",
                   __file,
                   __line);
#endif

#if defined(N00B_DLOG_GIL)
    if (atomic_fetch_or(&tsi->self_lock, N00B_SUSPEND) & N00B_SUSPEND) {
        n00b_dlog_gil3("Nested suspend (try to avoid)");
    }
#endif
    n00b_thread_stack_region(n00b_thread_self());
}

void
N00B_DBG_DECL(n00b_thread_resume)
{
    // Threads can try to resume during a GIL. We should Go ahead and
    // turn of 'SUSPEND', but then we will hit the GIL when we do the
    // check-in at the end.
    //
    // However, the GIL thread should quick-return; it shouldn't have
    // any contention, unless it is trying to access a resource that's
    // already locked at the time of the GIL, which should be a no-no.
    //
    // If that happens, the system will deadlock.

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (!tsi) {
        exit(-1);
    }

    int val = atomic_read(&n00b_gil);
    if (val == tsi->thread_id) {
        return;
    }

    if (&tsi->self_data == n00b_thread_to_cancel) {
        n00b_dlog_thread("Exiting thread due to cancelation");
        n00b_thread_exit(N00B_CANCEL_EXIT);
    }
#if 0
    n00b_dlog_gil2("op: resume; file = %s; line = %d",
                   __file,
                   __line);
#endif

#if defined(N00B_DLOG_GIL)
    if (!(atomic_fetch_and(&tsi->self_lock, ~N00B_SUSPEND)
          & N00B_SUSPEND)) {
        n00b_dlog_gil1("WARNING: resume from non-suspended thread");
    }
#endif
    n00b_thread_checkin();
}

inline void
n00b_thread_start(void)
{
    wait_for_gil_release();
}

// Temporary allocation API for when the GIL is active, or during
// startup. The created pages are freed the next time a STW happens.
//
// We get to access all of this stuff without contention.
//
// This interface doesn't end up getting logged, to avoid super spam
// due to recursive stuff.
//
// If you happen to not be in the GIL, we go ahead and use the default
// heap.
//
// Currently non-object only.

typedef struct mheap_pageset_t {
    char                   *pageset_end;
    struct mheap_pageset_t *next_pageset;
    char                   *next_alloc;
    char                    heap_start[];
} mheap_pageset_t;

_Atomic(mheap_pageset_t *) stw_heap   = NULL;
static _Atomic int32_t     mheap_lock = 0;

void
mheap_add_pages(int size)
{
    while (atomic_fetch_or(&mheap_lock, 1))
        ;

    mheap_pageset_t *ps;

    size += sizeof(mheap_pageset_t);
    size = n00b_max(size, N00B_DEFAULT_STW_HEAP_SIZE);
    size = n00b_round_up_to_given_power_of_2(n00b_page_bytes, size);
    ps   = mmap(NULL,
              size,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANON,
              -1,
              0);

    if (ps == MAP_FAILED) {
        fprintf(stderr, "Out of memory.");
        abort();
    }

    ps->pageset_end  = ((char *)ps) + size;
    ps->next_pageset = atomic_read(&stw_heap);
    ps->next_alloc   = ps->heap_start;
    atomic_store(&stw_heap, ps);
    atomic_store(&mheap_lock, 0);
#if 0
    n00b_dlog_gil("STW scratch heap @%p: %p-%p (next @%p)",
		  ps, ps->next_alloc, ps->pageset_end, ps->next_pageset);
#endif
}

void
mheap_abandon(void)
{
    if (!n00b_startup_complete) {
        return;
    }

    while (atomic_fetch_or(&mheap_lock, 1))
        ;

    mheap_pageset_t *cur = atomic_read(&stw_heap);
    mheap_pageset_t *next;

    atomic_store(&stw_heap, NULL);

    while (cur) {
        int len = cur->pageset_end - (char *)cur;
        next    = cur->next_pageset;
#if defined(__APPLE__)
        madvise(cur, len, MADV_ZERO);
#endif
        mprotect(cur, len, PROT_NONE);
        munmap(cur, len);
        cur = next;
    }

    assert(!atomic_read(&stw_heap));
    atomic_store(&mheap_lock, 0);
}

static inline bool
use_gil_alloc(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return true;
    }

    if (tsi->dlogging) {
        return true;
    }

    if (!n00b_startup_complete || !n00b_world_is_stopped()) {
        return false;
    }

    return true;
}

void *
N00B_DBG_DECL(n00b_gil_alloc_len, int n)
{
    if (!use_gil_alloc()) {
        return n00b_gc_raw_alloc(n, N00B_GC_SCAN_ALL);
    }

    char *result;

    int sz = n00b_round_up_to_given_power_of_2(N00B_FORCED_ALIGNMENT,
                                               n);

    mheap_pageset_t *ps = atomic_read(&stw_heap);

    if (!ps) {
        mheap_add_pages(sz);
        ps = atomic_read(&stw_heap);
    }

    if (ps->next_alloc + sz > ps->pageset_end) {
        mheap_add_pages(sz);
        ps = atomic_read(&stw_heap);
    }
    result = ps->next_alloc;
    ps->next_alloc += sz;

    return result;
}
