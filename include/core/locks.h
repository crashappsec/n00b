#pragma once
#include "n00b/base.h"

extern bool n00b_is_world_stopped(void);

typedef struct n00b_lock_record_t n00b_lock_record_t;

struct n00b_lock_record_t {
    char               *file;
    int                 line;
    n00b_lock_record_t *prev;
};

typedef struct n00b_lock_t {
    pthread_mutex_t     lock;
    void               *thread;
    unsigned int        level   : 31;
    unsigned int        nosleep : 1;
    int                 slot;
    n00b_lock_record_t *locks;
} n00b_lock_t;

typedef struct n00b_condition_t {
    n00b_lock_t    lock;
    pthread_cond_t cv;
    char           pad[128];
    void          *aux;
    void          *extra; // User-defined.
    void (*cb)(void *);   // Callback called before signaling.
} n00b_condition_t;

typedef struct {
    int pipe[2];
} n00b_notifier_t;

extern n00b_notifier_t *n00b_new_notifier(void);
extern int64_t          n00b_wait(n00b_notifier_t *, int);

typedef struct n00b_rw_lock_t {
    pthread_rwlock_t    lock;
    void               *thread; // Thread holding the write lock.
    unsigned int        level   : 31;
    unsigned int        nosleep : 1;
    n00b_lock_record_t *locks;
} n00b_rw_lock_t;

extern n00b_heap_t *n00b_internal_heap;

#define add_lock_record(lock, f, lineno)                              \
    n00b_push_heap(n00b_internal_heap);                               \
    n00b_lock_record_t *lr = n00b_gc_alloc_mapped(n00b_lock_record_t, \
                                                  N00B_GC_SCAN_ALL);  \
    n00b_pop_heap();                                                  \
    lr->prev    = lock->locks;                                        \
    lock->locks = lr;                                                 \
    lr->file    = f;                                                  \
    lr->line    = lineno;

#define add_unlock_record(lock, f, lineno)                            \
    n00b_push_heap(n00b_internal_heap);                               \
    n00b_lock_record_t *lr = n00b_gc_alloc_mapped(n00b_lock_record_t, \
                                                  N00B_GC_SCAN_ALL);  \
    n00b_pop_heap();                                                  \
    lr->prev      = lock->unlocks;                                    \
    lock->unlocks = lr;                                               \
    lr->file      = f;                                                \
    lr->line      = lineno;

#define clear_lock_records(lock) \
    lock->locks = NULL

extern void n00b_raw_lock_init(n00b_lock_t *);
extern void n00b_raw_condition_init(n00b_condition_t *c);
extern bool _n00b_lock_acquire_if_unlocked(n00b_lock_t *l, char *, int);
extern void _n00b_lock_acquire_raw(n00b_lock_t *l, char *, int);
extern void _n00b_lock_acquire(n00b_lock_t *l, char *, int);
extern void n00b_lock_release(n00b_lock_t *l);
extern void n00b_lock_release_all(n00b_lock_t *l);
extern void n00b_gts_suspend(void);
extern void n00b_gts_resume(void);
extern bool n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *, bool);

#define n00b_lock_acquire_if_unlocked(l) \
    _n00b_lock_acquire_if_unlocked(l, __FILE__, __LINE__)
#define n00b_lock_acquire_raw(l) \
    _n00b_lock_acquire_raw(l, __FILE__, __LINE__)
#define n00b_lock_acquire(l) \
    _n00b_lock_acquire(l, __FILE__, __LINE__)

static inline void
n00b_lock_set_nosleep(n00b_lock_t *l)
{
    l->nosleep = true;
}

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

static inline void
n00b_rw_lock_set_nosleep(n00b_rw_lock_t *l)
{
    l->nosleep = true;
}

static inline void
n00b_raw_rw_lock_init(n00b_rw_lock_t *l)
{
    pthread_rwlock_init(&l->lock, NULL);
    l->level = 0;
}

#define n00b_rw_lock_init(x) n00b_raw_rw_lock_init(x)

static inline bool
_n00b_rw_lock_acquire_for_write_if_unlocked(n00b_rw_lock_t *l, char *f, int ln)
{
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
        assert(!l->level);
        add_lock_record(l, f, ln);
        return true;
    default:
        return false;
    }
}

static inline void
_n00b_rw_lock_acquire_for_write(n00b_rw_lock_t *l, char *f, int ln)
{
    if (!_n00b_rw_lock_acquire_for_write_if_unlocked(l, f, ln)) {
        if (!l->nosleep) {
            n00b_gts_suspend();
            pthread_rwlock_wrlock(&l->lock);
            n00b_gts_resume();
            add_lock_record(l, f, ln);
        }
        else {
            n00b_gts_suspend();
            pthread_rwlock_wrlock(&l->lock);
            n00b_gts_resume();
            add_lock_record(l, f, ln);
        }
        l->thread = pthread_self();
        assert(!l->level);
        add_lock_record(l, f, ln);
    }
}

#define n00b_rw_lock_acquire_for_write_if_unlocked(l) \
    _n00b_rw_lock_acquire_for_write_if_unlocked(l, __FILE__, __LINE__)

#define n00b_rw_lock_acquire_for_write(l) \
    _n00b_rw_lock_acquire_for_write(l, __FILE__, __LINE__)

static inline void
n00b_rw_lock_release(n00b_rw_lock_t *l)
{
    if (l->level && l->thread == pthread_self()) {
        l->level--;
        return;
    }
    l->thread = NULL;
    clear_lock_records(l);
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
    n00b_gts_suspend();                                                 \
    assert(pthread_cond_wait(&((c)->cv), (&(c)->lock.lock)) != EINVAL); \
    n00b_gts_resume()

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

#if defined(N00B_USE_INTERNAL_API)
extern void n00b_setup_lock_registry(void);

#if defined(N00B_DEBUG_LOCKS)
extern void         n00b_debug_held_locks(char *, int);
extern void         n00b_debug_all_held_locks(void);
extern _Atomic bool __n00b_lock_debug;

#define N00B_DEBUG_HELD_LOCKS()                    \
    if (__n00b_lock_debug) {                       \
        n00b_debug_held_locks(__FILE__, __LINE__); \
    }
#define n00b_lock_debugging_on()  atomic_store(&__n00b_lock_debug, true)
#define n00b_lock_debugging_off() atomic_store(&__n00b_lock_debug, false)
#else
#define N00B_DEBUG_HELD_LOCKS()
#define n00b_lock_debugging_on()
#define n00b_lock_debugging_off()
#endif
#endif
