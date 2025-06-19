#pragma once
#include "n00b.h"

#ifdef N00B_USE_INTERNAL_API
extern _Atomic int32_t          n00b_live_threads;
extern _Atomic(n00b_thread_t *) n00b_global_thread_list[N00B_THREADS_MAX];
extern _Atomic int32_t          n00b_next_thread_slot;

static inline int
n00b_thread_run_count(void)
{
    return atomic_read(&n00b_live_threads);
}
#endif

extern n00b_tsi_t *_n00b_init_self_tsi(int);
#define n00b_init_self_tsi(void) _n00b_init_self_tsi(0)

// Static thread-specific info, because __thread is doing WEIRD things
// on MacOS and might as well group everything and minimize calls to
// pthread_setspecific() for static fields.
struct n00b_tsi_t {
    n00b_thread_t          self_data;
    n00b_exception_stack_t exception_stack;
    // Stuff to support the locking subsystem:

    // These two are used to ensure we can clean up lock state to
    // be consistent, when a thread exits while holding locks.
    n00b_lock_base_t             *exclusive_locks;
    n00b_lock_log_t              *read_locks;
    n00b_lock_log_t              *log_alloc_cache;
    // This supports the Global Interpreter Lock.
    n00b_futex_t                  self_lock;
    // State associated with any condition variable we are waiting on.
    n00b_condition_thread_state_t cv_info;
    n00b_lock_base_t             *lock_wait_target;
#if defined(__linux__)
    pthread_attr_t attrs;
#endif
#ifdef N00B_DEBUG
    char *lock_wait_file;
    int   lock_wait_line;
    char *lock_wait_trace;
#endif
    void        *thread_runtime; // really n00b_vmthread_t
    // Used by libbacktrace.
    char        *bt_utf8_result;
    void        *trace_table; // really n00b_table_t
    // Threads are allowed to change their heap; we aren't currently
    // ever taking advantage of this, but the allocator does respect it.
    n00b_heap_t *thread_heap;
    int64_t      thread_id;
    int          kargs_next_entry;
    uint8_t      dlogging;
    // C keyword args have fixed-size, thread-specific storage above.
    // We use this bit to indicate whether it's been set up or not;
    // doing so happens the first time a keyword enabled function is
    // called.
    unsigned int init_kargs            : 1;
    unsigned int thread_sleeping       : 1;
    // Used to mark private threads like the IO event dispatcher
    // and the IO callback thread.
    unsigned int system_thread         : 1;
    // Used for debugging, to tell n00b_new() to add a guard
    // page after the next in-thread call.
    unsigned int add_guard             : 1;
    // This flag is used if debugging watches are enabled, to prevent
    // recursion. See utils/watch.c for more.
    //
    // Currently watch checks have been removed from allocs; will put
    // them back down the road when I need them again.
    unsigned int watch_recursion_guard : 1;
    // This flag is used to prevent recursion when getting an
    // unformatted stack trace from libbacktrace().
    unsigned int u8_backtracing        : 1;
    // Prevent log recursion.
#if defined(N00B_ENABLE_ALLOC_DEBUG)
    int show_alloc_locations : 1;
#endif
    n00b_static_karg_t kcache;
};

extern pthread_key_t n00b_static_tsi_key;

static inline n00b_tsi_t *
n00b_get_tsi_ptr(void)
{
    n00b_tsi_t *result;

    n00b_barrier();

    if ((result = pthread_getspecific(n00b_static_tsi_key)) == NULL) {
        result = n00b_init_self_tsi();
    }
    return result;
}

static inline n00b_thread_t *
n00b_thread_self(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return &tsi->self_data;
}

static inline void * /* n00b_vmthread_t * */
n00b_thread_runtime_acquire()
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return tsi->thread_runtime;
}

static inline int64_t
n00b_thread_id(void)
{
    return n00b_get_tsi_ptr()->thread_id;
}

static inline n00b_heap_t *
n00b_thread_heap(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return tsi->thread_heap;
}

static inline void
n00b_set_thread_heap(n00b_heap_t *h)
{
    n00b_get_tsi_ptr()->thread_heap = h;
}

static inline n00b_heap_t *
n00b_current_heap(n00b_heap_t *h)
{
    if (h) {
        return h;
    }
    h = n00b_thread_heap();

    if (h) {
        return h;
    }
    return n00b_default_heap;
}

#define n00b_push_heap(heap)
#define n00b_pop_heap()

#ifdef N00B_MPROTECT_GUARD_ALLOCS
static inline void
n00b_alloc_guard_next_new(void)
{
    n00b_get_tsi_ptr()->add_guard = true;
}
#else
#define n00b_alloc_guard_next_new()
#endif
