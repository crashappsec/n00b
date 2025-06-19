#pragma once
#include "n00b.h"

typedef struct n00b_thread_t n00b_thread_t;

struct n00b_thread_t {
    void     *tsi;
    void     *base; // base pointer of stack
    void     *cur;  // current stack 'top'.
    pthread_t pthread_id;
    bool      cancel;
};

extern void           n00b_thread_stack_region(n00b_thread_t *);
extern n00b_thread_t *n00b_thread_register(void);
extern void           n00b_thread_unregister(void *);
extern n00b_thread_t *n00b_thread_spawn(void *(*)(void *), void *);
extern void           n00b_thread_cancel_other_threads(void);
extern n00b_thread_t *n00b_thread_by_id(int32_t);
extern void           n00b_thread_cancel(n00b_thread_t *);

typedef struct {
    void *(*true_cb)(void *);
    void   *true_arg;
    int32_t tid;
} n00b_tbundle_t;

// 1/1000th of a second
#define N00B_GTS_POLL_DURATION_NS 100000

#ifdef N00B_USE_INTERNAL_API
extern /*once*/ void  n00b_threading_setup(void);
extern n00b_thread_t *n00b_thread_list_acquire(void);
extern void           n00b_thread_list_release(void);
extern bool           n00b_current_process_is_exiting(void);
extern void           n00b_initialize_global_thread_info(void);
extern n00b_thread_t *n00b_thread_find_by_pthread_id(pthread_t);
extern void          *n00b_post_thread_cleanup(n00b_tsi_t *tsi);
#endif
