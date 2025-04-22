#pragma once
#include "n00b/base.h"

extern bool n00b_is_world_stopped(void);
extern bool n00b_abort_signal;

// Must be a power of 2.
#define N00B_LOCK_DEBUG_RING  8
#define N00B_LOCK_MAX_READERS 32

// All locks use the same header, so the debugigng can iterate and
// print info specific to the kind.

typedef enum {
    N00B_LK_MUTEX,
    N00B_LK_RWLOCK,
    N00B_LK_CONDITION,
} n00b_lock_kind;

typedef struct {
    char *file;
    int   line;
} n00b_lock_record_t;

typedef struct {
    void              *next_held;
    void              *prev_held;
    void              *thread;
    int                level;
    n00b_lock_kind     kind;
    // Debug stuff.
    char              *debug_name;
    n00b_lock_record_t alloc_info;
    n00b_lock_record_t lock_info[N00B_LOCK_DEBUG_RING];
    uint8_t            walked : 1;
    uint8_t            inited : 1;
    void              *lock[0];
} n00b_generic_lock_t;

typedef struct n00b_mutex_t {
    n00b_generic_lock_t info;
    pthread_mutex_t     lock;
} n00b_mutex_t;

typedef n00b_mutex_t n00b_lock_t;

extern void _n00b_lock_init(n00b_lock_t *, char *, char *, int);
extern bool _n00b_lock_acquire_if_unlocked(n00b_lock_t *, char *, int);
extern void _n00b_lock_acquire_raw(n00b_lock_t *, char *, int);
extern void _n00b_lock_acquire(n00b_lock_t *, char *, int);
extern void n00b_mutex_release(n00b_lock_t *);
extern void n00b_mutex_release_all(n00b_lock_t *);

#define n00b_lock_init(ptr) \
    _n00b_lock_init(ptr, NULL, __FILE__, __LINE__)
#define n00b_named_lock_init(l, n) \
    _n00b_lock_init(l, n, __FILE__, __LINE__)
#define n00b_lock_acquire_if_unlocked(l) \
    _n00b_lock_acquire_if_unlocked(l, __FILE__, __LINE__)
#define n00b_lock_acquire_raw(l) \
    _n00b_lock_acquire_raw(l, __FILE__, __LINE__)
#define n00b_lock_acquire(l) \
    _n00b_lock_acquire(l, __FILE__, __LINE__)

typedef struct {
    char *file;
    int   line;
    int   level;
    void *thread;
} n00b_rw_reader_record_t;

typedef struct n00b_rw_lock_t {
    n00b_generic_lock_t     info;
    pthread_rwlock_t        lock;
    _Atomic int             reader_id;
    _Atomic int             num_readers;
    void                   *next_rw_lock;
    n00b_rw_reader_record_t readers[N00B_LOCK_MAX_READERS];
} n00b_rw_lock_t;

extern void _n00b_rw_lock_init(n00b_rw_lock_t *, char *, char *, int);
extern bool _n00b_rw_lock_try_read(n00b_rw_lock_t *, char *, int);
extern void _n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *, char *, int);
extern bool _n00b_rw_lock_try_write(n00b_rw_lock_t *, char *, int);
extern void _n00b_rw_lock_acquire_for_write(n00b_rw_lock_t *, char *, int);
extern bool _n00b_rw_lock_try_read(n00b_rw_lock_t *, char *, int);
extern void _n00b_rw_lock_acquire_for_read(n00b_rw_lock_t *, char *, int);
extern void _n00b_rw_lock_release(n00b_rw_lock_t *, bool);

#define n00b_rw_lock_init(l) _n00b_rw_lock_init(l, NULL, __FILE__, __LINE__)
#define n00b_named_rw_lock_init(l, n) \
    _n00b_rw_lock_init(l, n, __FILE__, __LINE__)
#define n00b_rw_lock_try_read(l) \
    _n00b_rw_lock_try_read(l, __FILE__, __LINE__)
#define n00b_rw_lock_acquire_for_read(l) \
    _n00b_rw_lock_acquire_for_read(l, __FILE__, __LINE__)
#define n00b_rw_lock_try_write(l) \
    _n00b_rw_lock_try_write(l, __FILE__, __LINE__)
#define n00b_rw_lock_acquire_for_write(l) \
    _n00b_rw_lock_acquire_for_write(l, __FILE__, __LINE__)
#define n00b_rw_lock_release(l)     _n00b_rw_lock_release(l, false)
#define n00b_rw_lock_release_all(l) _n00b_rw_lock_release(l, true)

typedef struct n00b_condition_t {
    n00b_lock_t    mutex;
    pthread_cond_t cv;
    void          *aux;
    void          *extra; // User-defined.
    void (*cb)(void *);   // Callback called before signaling.
    n00b_rw_reader_record_t last_notify;
} n00b_condition_t;

extern void _n00b_condition_init(n00b_condition_t *, char *, char *, int);
extern void n00b_condition_pre_wait(n00b_condition_t *);
extern void n00b_condition_post_wait(n00b_condition_t *, char *, int);
extern bool _n00b_condition_timed_wait(n00b_condition_t *,
                                       n00b_duration_t *,
                                       char *,
                                       int);
extern void _n00b_condition_notify_one(n00b_condition_t *, void *, char *, int);
extern void _n00b_condition_notify_all(n00b_condition_t *, char *, int);

#define n00b_condition_init(c) \
    _n00b_condition_init(c, NULL, __FILE__, __LINE__)
#define n00b_named_condition_init(c, n) \
    _n00b_condition_init(c, n, __FILE__, __LINE__)

#define n00b_condition_lock_acquire_raw(c) \
    n00b_lock_acquire_raw(&(((n00b_condition_t *)c)->lock));

#define n00b_condition_lock_acquire(c) \
    n00b_lock_acquire(&(((n00b_condition_t *)c)->mutex));

#define n00b_condition_lock_release(c) \
    n00b_lock_release(&(((n00b_condition_t *)c)->mutex));

#define n00b_condition_lock_release_all(c) \
    n00b_lock_release_all(&(((n00b_condition_t *)c)->mutex))

#define n00b_condition_wait_raw(c, ...)                        \
    n00b_condition_pre_wait((n00b_condition_t *)c);            \
    __VA_ARGS__;                                               \
    pthread_cond_wait(&(((n00b_condition_t *)c)->cv),          \
                      (&((n00b_condition_t *)c)->mutex.lock)); \
    n00b_condition_post_wait((n00b_condition_t *)c, __FILE__, __LINE__)

#define n00b_condition_wait(c, ...)                                       \
    n00b_condition_pre_wait((n00b_condition_t *)c);                       \
    __VA_ARGS__;                                                          \
    n00b_gts_suspend();                                                   \
    n00b_assert(pthread_cond_wait(&(((n00b_condition_t *)c)->cv),         \
                                  (&((n00b_condition_t *)c)->mutex.lock)) \
                != EINVAL);                                               \
    n00b_gts_resume();                                                    \
                                                                          \
    n00b_condition_post_wait((n00b_condition_t *)c, __FILE__, __LINE__)

#define n00b_condition_timed_wait(c, d)                   \
    n00b_condition_pre_wait(c),                           \
        _n00b_condition_timed_wait((n00b_condition_t *)c, \
                                   d,                     \
                                   __FILE__,              \
                                   __LINE__)

#define n00b_condition_timed_wait_arg(c, d, CODE)     \
    n00b_condition_pre_wait(c);                       \
    CODE;                                             \
    _n00b_condition_timed_wait((n00b_condition_t *)c, \
                               d,                     \
                               __FILE__,              \
                               __LINE__)

#define n00b_condition_wait_then_unlock(c, ...)          \
    n00b_condition_wait(((n00b_condition_t *)c)          \
                            __VA_OPT__(, ) __VA_ARGS__); \
    n00b_condition_lock_release((n00b_condition_t *)c)

#define n00b_condition_notify_one(c) \
    _n00b_condition_notify_one(c, NULL, __FILE__, __LINE__)
#define n00b_condition_notify_aux(c, arg) \
    _n00b_condition_notify_one(c, arg, __FILE__, __LINE__)
#define n00b_condition_notify_all(c) \
    _n00b_condition_notify_all(c, __FILE__, __LINE__)

#define n00b_new_lock()         n00b_new(n00b_type_lock(), NULL)
#define n00b_new_condition(...) n00b_new(n00b_type_condition(), \
                                         __VA_ARGS__            \
                                             __VA_OPT__(, ) NULL)

#define n00b_static_lock_init(x)      n00b_lock_init(&(x))
#define n00b_static_condition_init(x) n00b_condition_init(&(x))
#define n00b_static_rw_lock_init(x)   n00b_rw_lock_init(&(x))

extern void
            n00b_debug_all_locks(void);
extern void n00b_lock_release(void *);
extern void n00b_lock_release_all(void *);
