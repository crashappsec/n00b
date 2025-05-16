#pragma once
#include "n00b.h"

typedef struct n00b_thread_t n00b_thread_t;

struct n00b_thread_t {
    // This reserves space for MMM's use; the n00b_thread_t * is
    // transparently cast to it, so this must be first.
    mmm_thread_t mmm_info;
    void        *tsi;
    void        *base; // base pointer of stack
    void        *cur;  // current stack 'top'.
    pthread_t    pthread_id;
};

// Use the first one on thread first start-up only.
extern void n00b_gts_start(void);
extern void n00b_gts_suspend(void);
extern void n00b_gts_resume(void);
extern void n00b_gts_might_stop(void);
extern void _n00b_gts_stop_the_world(char *, int);
extern void n00b_gts_wont_stop(void);
extern void _n00b_gts_restart_the_world(char *, int);
extern void n00b_gts_checkin(void);
extern void n00b_gts_notify_abort(void);

#define n00b_gts_stop_the_world() \
    _n00b_gts_stop_the_world(__FILE__, __LINE__)
#define n00b_gts_restart_the_world() \
    _n00b_gts_restart_the_world(__FILE__, __LINE__)

extern void           n00b_thread_stack_region(n00b_thread_t *);
extern n00b_thread_t *n00b_thread_register(void);
extern void           n00b_thread_unregister(void *);
extern n00b_thread_t *n00b_thread_spawn(void *(*)(void *), void *);
extern void           n00b_thread_cancel_other_threads(void);

static inline void
n00b_thread_prevent_cancelation(void)
{
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

static inline void
n00b_thread_async_cancelable(void)
{
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

typedef struct {
    void *(*true_cb)(void *);
    void *true_arg;
} n00b_tbundle_t;

// 1/1000th of a second
#define N00B_GTS_POLL_DURATION_NS 100000

#ifdef N00B_USE_INTERNAL_API
extern void           n00b_threading_setup(void);
extern n00b_thread_t *n00b_thread_list_acquire(void);
extern void           n00b_thread_list_release(void);
extern bool           n00b_current_process_is_exiting(void);
extern void           n00b_initialize_global_thread_info(void);
extern n00b_thread_t *n00b_thread_find_by_pthread_id(pthread_t);
extern bool           n00b_world_is_stopped;
#endif
