#pragma once
#include "n00b.h"

// Static thread-specific info, because __thread is doing WEIRD things
// on MacOS and might as well group everything and minimize calls to
// pthread_setspecific() for static fields.
typedef struct {
    n00b_thread_t              self_data;
    n00b_exception_stack_t     exception_stack;
    void                      *thread_runtime; // really n00b_vmthread_t
    char                      *bt_utf8_result;
    // Used by libbacktrace.
    void                      *trace_table; // really n00b_table_t
    n00b_heap_t               *thread_heap;
    n00b_heap_t               *string_heap;
    void                      *locks; // n00b_generic_lock_t       *
    // When we call into system functions that we know are not
    // recursive, we can stash the user's heap here. For instance,
    // this is done when calling into the format module, which does a
    // lot of little allocations that we clean up all at once.
    n00b_heap_t               *stashed_heap;
    int                        saved_lock_level;
    n00b_lock_record_t         saved_lock_records[N00B_LOCK_DEBUG_RING];
    n00b_lock_record_t         lock_wait_location;
    n00b_lock_t               *lock_wait_target;
    int64_t                    thread_id;
    int                        kargs_next_entry;
    n00b_global_thread_state_t thread_state;
    // C keyword args have fixed-size, thread-specific storage above.
    // We use this bit to indicate whether it's been set up or not;
    // doing so happens the first time a keyword enabled function is
    // called.
    int                        init_kargs            : 1;
    int                        thread_sleeping       : 1;
    // Used to mark private threads like the IO event dispatcher
    // and the IO callback thread.
    int                        system_thread         : 1;
    // Used for debugging, to tell n00b_new() to add a guard
    // page after the next in-thread call.
    int                        add_guard             : 1;
    // This flag is used if debugging watches are enabled, to prevent
    // recursion. See utils/watch.c for more.
    //
    // Currently watch checks have been removed from allocs; will put
    // them back down the road when I need them again.
    int                        watch_recursion_guard : 1;
    // This flag is used to prevent recursion when getting an
    // unformatted stack trace from libbacktrace().
    int                        u8_backtracing        : 1;
    // This flag is used for critical sections with private
    // data structures to suspend locking.
    int                        suspend_locking       : 1;
#if defined(N00B_ENABLE_ALLOC_DEBUG)
    int show_alloc_locations : 1;
#endif
#if defined(N00B_FLOG_DEBUG)
    FILE *flog_file;
#endif

    n00b_static_karg_t kcache[N00B_MAX_KARGS_NESTING_DEPTH];
} n00b_tsi_t;

extern pthread_key_t n00b_static_tsi_key;

static inline n00b_tsi_t *
n00b_get_tsi_ptr(void)
{
    return pthread_getspecific(n00b_static_tsi_key);
}

static inline void
n00b_thread_resume_locking(void)
{
    n00b_tsi_t *tsi      = n00b_get_tsi_ptr();
    tsi->suspend_locking = false;
}

static inline void
n00b_thread_suspend_locking(void)
{
    n00b_tsi_t *tsi      = n00b_get_tsi_ptr();
    tsi->suspend_locking = true;
}

static inline void * // n00b_generic_lock_t *
n00b_get_thread_locks(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    if (!tsi) {
        return NULL;
    }

    return tsi->locks;
}

static inline bool
n00b_is_system_thread(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return false;
    }

    return tsi->system_thread;
}

static inline void
n00b_set_system_thread(void)
{
    n00b_get_tsi_ptr()->system_thread = true;
}

static inline void
n00b_set_thread_locks(void *locks) // n00b_generic_lock_t *
{
    n00b_get_tsi_ptr()->locks = locks;
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

static inline n00b_thread_t *
n00b_thread_self(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return &tsi->self_data;
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
n00b_string_heap(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return tsi->string_heap;
}

static inline void
n00b_set_string_heap(n00b_heap_t *h)
{
    n00b_get_tsi_ptr()->string_heap = h;
}

static inline n00b_heap_t *
n00b_stashed_heap(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (!tsi) {
        return NULL;
    }

    return tsi->stashed_heap;
}

static inline void
n00b_stash_thread_heap(void)
{
    n00b_tsi_t *tsi   = n00b_get_tsi_ptr();
    tsi->stashed_heap = tsi->thread_heap;
}

static inline void
n00b_restore_stashed_heap(void)
{
    n00b_tsi_t *tsi   = n00b_get_tsi_ptr();
    tsi->thread_heap  = tsi->stashed_heap;
    tsi->stashed_heap = NULL;
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

static inline n00b_global_thread_state_t
n00b_get_thread_state(void)
{
    return n00b_get_tsi_ptr()->thread_state;
}

static inline void
n00b_set_thread_state(n00b_global_thread_state_t s)
{
    n00b_get_tsi_ptr()->thread_state = s;
}

#ifdef N00B_MPROTECT_GUARD_ALLOCS
static inline void
n00b_alloc_guard_next_new(void)
{
    n00b_get_tsi_ptr()->add_guard = true;
}
#else
#define n00b_alloc_guard_next_new()
#endif

// Use to stash on the stack.
#define n00b_push_heap(heap)                             \
    n00b_heap_t *__n00b_saved_heap = n00b_thread_heap(); \
    n00b_set_thread_heap(heap)

#define n00b_pop_heap() \
    n00b_set_thread_heap(__n00b_saved_heap)

#ifdef N00B_USE_INTERNAL_API
extern void
            n00b_set_system_thread(void);
extern bool n00b_is_system_thread(void);
extern void n00b_initialize_thread_specific_info(void);
extern void n00b_finish_main_thread_initialization(void);

extern _Atomic int              n00b_live_threads;
extern _Atomic(n00b_thread_t *) n00b_global_thread_list[HATRACK_THREADS_MAX];
extern _Atomic int              n00b_next_thread_slot;
extern n00b_tsi_t              *n00b_init_self_tsi(void);
extern void                     n00b_gts_quit(n00b_tsi_t *);

static inline int
n00b_thread_run_count(void)
{
    return atomic_read(&n00b_live_threads);
}

#endif
