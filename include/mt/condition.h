#pragma once
#include "n00b.h"

// Traditional condition variables have a crap interface. Condition
// variables don't imply a mutex, you have to pass mutexes around (and
// can oddly use one CV with multiple mutexes). And, managing that
// mutex is often a pain, especially when the condition variable can
// be waited on or signaled without any need for holding onto a lock
// while running (on the side of the operation).
//
// Similarly, while it's usually right to want to gate the start of a
// wait operation by locking it, sometimes I just want to wait for the
// 'next' event, even if one has passed.
//
// Also, the notion of a 'condition' is left pretty amorphous. It's
// nice to be able explicitly pass a value sometimes, especially in
// two cases:
//
// 1. When producers want to wake a single consumer and pass back a
//    single value (a notify_one())
//
// 2. When the condition has multiple states, and you want to wake a
//    class of listener, you either have to do a notify_all() and then
//    have people put themselves back to sleep, or you need to run
//    multiple condition variables.
//
// I'd like to be able to atomically pass values to anyone waiting,
// and I'd love to have a decent portable interface to only wake
// some people. I'd also love to do custom predicates.
//
// Thus, this implementation. It automatically handles acquiring a
// lock on 'notify' if you don't already have it, and has convenience
// parameters to automatically acquire on wait() or to automatically
// drop after a wait() or a notify().
//
// It can also pass back a 'result', and allows selectively waking via
// callback or basic state #'s.

typedef bool (*n00b_condition_predicate_fn)(uint64_t, // actual pred value
                                            uint64_t, // thread pred param
                                            void *,   // The 'output'
                                            void *,   // cv-wide user parameter
                                            void *);  // Thread-provided parameter.

typedef struct {
    N00B_COMMON_LOCK_BASE;
    n00b_futex_t                mutex;
    _Atomic uint32_t            should_wake;
    void                       *ovalue;   // 'Output' from a notification.
    uint64_t                    pvalue;   // Predicate value.
    n00b_condition_predicate_fn predicate;
    void                       *cv_param; // Used only w/ predicate
    n00b_futex_t                notify_done;
    int32_t                     num_to_process;
    int32_t                     num_to_wake;
    n00b_futex_t                wait_queue;
} n00b_condition_t;

typedef struct {
    n00b_condition_t *current_cv;
    uint64_t          wait_predicate;
    void             *thread_param;
#if defined(N00B_DEBUG)
    char *wait_file;
    int   wait_line;
#endif
} n00b_condition_thread_state_t;

extern void N00B_DBG_DECL(n00b_condition_init, n00b_condition_t *);
extern int  N00B_DBG_DECL(n00b_condition_lock, n00b_condition_t *);
extern bool N00B_DBG_DECL(n00b_condition_unlock, n00b_condition_t *);
extern void n00b_condition_set_callback(n00b_condition_t *,
                                        n00b_condition_predicate_fn,
                                        void *);
#define n00b_condition_init(x)   N00B_DBG_CALL(n00b_condition_init, x)
#define n00b_condition_lock(x)   N00B_DBG_CALL(n00b_condition_lock, x)
#define n00b_condition_unlock(x) N00B_DBG_CALL(n00b_condition_unlock, x)

#if defined(N00B_DEBUG)
extern int32_t _n00b_condition_notify(n00b_condition_t *, char *, int, ...);
extern void   *_n00b_condition_wait(n00b_condition_t *, char *, int, ...);
#define n00b_condition_notify(cv, ...) \
    _n00b_condition_notify(cv, __FILE__, __LINE__, N00B_VA(__VA_ARGS__))
#define n00b_condition_wait(cv, ...) \
    _n00b_condition_wait(cv, __FILE__, __LINE__, N00B_VA(__VA_ARGS__))
#else
extern int32_t _n00b_condition_notify(n00b_condition_t *, ...);
extern void   *_n00b_condition_wait(n00b_condition_t *, ...);
#define n00b_condition_notify(cv, ...) \
    _n00b_condition_notify(cv, N00B_VA(__VA_ARGS__))
#define n00b_condition_wait(cv, ...) \
    _n00b_condition_wait(cv, N00B_VA(__VA_ARGS__))

#endif

#define n00b_condition_notify_one(cv) n00b_condition_notify(cv)
#define n00b_condition_notify_all(cv) \
    n00b_condition_notify(cv, n00b_header_kargs("all", (int64_t) true))

#if defined(N00B_USE_INTERNAL_API)
#define N00B_CV_NOTIFY_IN_PROGRESS 0x40000000
#define N00B_CV_ANY                (~0ULL)
#endif

static inline bool
n00b_condition_has_waiters(n00b_condition_t *t)
{
    return atomic_read(&t->wait_queue) != 0;
}
