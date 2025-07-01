#pragma once
#include "n00b.h"

extern _Atomic int32_t n00b_live_threads;

#ifdef N00B_USE_INTERNAL_API
extern _Atomic(n00b_thread_t *) n00b_global_thread_list[N00B_THREADS_MAX];
extern _Atomic int32_t          n00b_next_thread_slot;
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
