#pragma once

#include "n00b/base.h"

extern bool n00b_is_world_stopped(void);

typedef struct n00b_lock_t {
    pthread_mutex_t lock;
    void           *thread;
    unsigned int    level;
    int             slot;
} n00b_lock_t;

typedef struct n00b_condition_t {
    n00b_lock_t    lock;
    pthread_cond_t cv;
    void          *aux;
    void          *extra; // User-defined.
    void (*cb)(void *);   // Callback called before signaling.
} n00b_condition_t;

typedef struct {
    int pipe[2];
} n00b_notifier_t;

extern n00b_notifier_t *n00b_new_notifier(void);
extern int64_t          n00b_wait(n00b_notifier_t *, int);

static inline void
n00b_notify(n00b_notifier_t *n, uint64_t value)
{
top:
    switch (write(n->pipe[1], &value, sizeof(uint64_t))) {
    case -1:
        if (errno == EINTR || errno == EAGAIN) {
            goto top;
        }
        abort(); // should be unreachable
    case sizeof(uint64_t):
        return;
    default:
        abort(); // should be unreachable.
    }
}

typedef struct n00b_rw_lock_t {
    pthread_rwlock_t lock;
    void            *thread; // Thread holding the write lock.
    unsigned int     level;
} n00b_rw_lock_t;

extern void n00b_thread_enter_run_state(void);
extern void n00b_thread_leave_run_state(void);

extern void n00b_raw_lock_init(n00b_lock_t *);
extern void n00b_raw_condition_init(n00b_condition_t *c);
extern bool n00b_lock_acquire_if_unlocked(n00b_lock_t *l);
extern void n00b_lock_acquire_raw(n00b_lock_t *l);
extern void n00b_lock_acquire(n00b_lock_t *l);
extern void n00b_lock_release(n00b_lock_t *l);
extern void n00b_lock_release_all(n00b_lock_t *l);

static inline void
n00b_raw_rw_lock_init(n00b_rw_lock_t *l)
{
    pthread_rwlock_init(&l->lock, NULL);
    l->level = 0;
}

#define n00b_rw_lock_init(x) n00b_raw_rw_lock_init(x)

static inline bool
n00b_rw_lock_acquire_for_write_if_unlocked(n00b_rw_lock_t *l)
{
    if (n00b_is_world_stopped()) {
        return true;
    }

    switch (pthread_rwlock_trywrlock(&l->lock)) {
    case EDEADLK:
    case EBUSY:
        if (l->thread == pthread_self()) {
            l->level++;
            return true;
        }
        return false;
    case 0:
        l->thread = pthread_self();
        return true;
    default:
        return false;
    }
}

static inline void
n00b_rw_lock_acquire_for_write(n00b_rw_lock_t *l)
{
    if (n00b_is_world_stopped()) {
        return;
    }

    if (!n00b_rw_lock_acquire_for_write_if_unlocked(l)) {
        n00b_thread_leave_run_state();
        pthread_rwlock_wrlock(&l->lock);
        n00b_thread_enter_run_state();
        l->thread = pthread_self();
    }
}

static inline void
n00b_rw_lock_release(n00b_rw_lock_t *l)
{
    if (n00b_is_world_stopped()) {
        return;
    }

    if (l->level && l->thread == pthread_self()) {
        l->level--;
        return;
    }

    pthread_rwlock_unlock(&l->lock);
}

#define n00b_condition_lock_acquire_raw(c) \
    n00b_lock_acquire_raw(&((c)->lock));

#define n00b_condition_lock_acquire(c) \
    n00b_lock_acquire(&((c)->lock));

#define n00b_condition_lock_release(c) \
    n00b_lock_release(&((c)->lock));

#define n00b_condition_lock_release_all(c) \
    n00b_lock_release_all(&((c)->lock))

#define n00b_condition_wait_raw(c, ...) \
    n00b_lock_acquire(&(c)->lock);      \
    __VA_ARGS__;                        \
    pthread_cond_wait(&((c)->cv), (&(c)->lock.lock));

#define n00b_condition_wait(c, ...)                                     \
    n00b_lock_acquire(&(c)->lock);                                      \
    __VA_ARGS__;                                                        \
    n00b_thread_leave_run_state();                                      \
    assert(pthread_cond_wait(&((c)->cv), (&(c)->lock.lock)) != EINVAL); \
    n00b_thread_enter_run_state()

#define n00b_condition_wait_then_unlock(c, ...)          \
    n00b_condition_wait((c)                              \
                            __VA_OPT__(, ) __VA_ARGS__); \
    n00b_condition_lock_release(c)

#define n00b_condition_notify_one(c, ...)   \
    {                                       \
        __VA_OPT__((c)->aux = __VA_ARGS__); \
        void (*cb)(void *) = (c)->cb;       \
        if (cb) {                           \
            (*cb)(c);                       \
        }                                   \
        pthread_cond_signal(&((c)->cv));    \
    }

#define n00b_condition_notify_all(c)        \
    {                                       \
        void (*cb)(void *) = (c)->cb;       \
        if (cb) {                           \
            (*cb)(c);                       \
        }                                   \
                                            \
        pthread_cond_broadcast((&(c)->cv)); \
    }

#define n00b_new_lock()         n00b_new(n00b_type_lock(), NULL)
#define n00b_new_condition(...) n00b_new(n00b_type_condition(), \
                                         __VA_ARGS__            \
                                             __VA_OPT__(, ) NULL)
#define n00b_lock_init(x)             n00b_raw_lock_init(x)
#define n00b_static_lock_init(x)      n00b_raw_lock_init(&(x))
#define n00b_static_condition_init(x) n00b_raw_condition_init(&(x))
#define n00b_static_rw_lock_init(x)   n00b_raw_rw_lock_init(&(x))

extern bool n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *, bool);
