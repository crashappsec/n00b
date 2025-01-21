#pragma once
#include "n00b.h"

typedef struct n00b_thread_t n00b_thread_t;

struct n00b_thread_t {
    mmm_thread_t mmm_info;
    pthread_t    pthread_id;
    void        *base; // base pointer of stack
    void        *cur;  // current stack 'top'.
};

extern void           n00b_thread_stack_region(n00b_thread_t *);
extern n00b_thread_t *n00b_thread_self(void);
extern n00b_thread_t *n00b_thread_register(void);
extern void           n00b_thread_unregister(void *);
extern int            n00b_thread_spawn(void *(*)(void *), void *);
extern void           n00b_thread_enter_run_state(void);
extern void           n00b_thread_leave_run_state(void);
extern void           n00b_thread_pause_if_stop_requested(void);

extern void n00b_stop_the_world(void);
extern void n00b_restart_the_world(void);

#define N00B_WITH_WORLD_STOPPED(stuff_to_do) \
    n00b_stop_the_world();                   \
    stuff_to_do;                             \
    n00b_restart_the_world()

typedef struct {
    void *(*true_cb)(void *);
    void *true_arg;
} n00b_tbundle_t;

extern void n00b_static_c_backtrace();

static inline int
n00b_nanosleep(n00b_duration_t *rqtp, n00b_duration_t *rmtp)
{
    int result;
    n00b_thread_leave_run_state();
    result = nanosleep(rqtp, rmtp);
    n00b_thread_enter_run_state();

    return result;
}

#ifdef N00B_USE_INTERNAL_API
extern n00b_thread_t            *n00b_thread_list_acquire(void);
extern void                      n00b_thread_list_release(void);
extern void                      n00b_lock_register(n00b_lock_t *);
extern void                      n00b_lock_unregister(n00b_lock_t *);
extern void                      n00b_thread_unlock_all(void);
extern bool                      n00b_current_process_is_exiting(void);
extern _Atomic(n00b_thread_t *) *n00b_global_thread_list;
extern __thread mmm_thread_t    *n00b_mmm_thread_ref;
#endif
