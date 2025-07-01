#pragma once
#include "n00b.h"

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

#ifdef N00B_MPROTECT_GUARD_ALLOCS
static inline void
n00b_alloc_guard_next_new(void)
{
    n00b_get_tsi_ptr()->add_guard = true;
}
#else
#define n00b_alloc_guard_next_new()
#endif

#ifdef N00B_USE_INTERNAL_API
static inline int
n00b_thread_run_count(void)
{
    return atomic_read(&n00b_live_threads);
}
#endif
