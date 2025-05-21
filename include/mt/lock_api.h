#pragma once
#include "n00b.h"

static inline uint8_t
__n00b_get_lock_type(n00b_lock_base_t *l)
{
    n00b_core_lock_info_t info = atomic_read(&l->data);
    return info.type;
}

static inline void
_n00b_lock_init(n00b_lock_base_t *l,
                uint8_t           t,
                char             *name,
                char             *__file,
                int               __line)
{
    l->debug_name = name;

    switch (t) {
    case N00B_NLT_MUTEX:
        N00B_DBG_CALL_NESTED(n00b_mutex_init, (void *)l);
        return;
    case N00B_NLT_RW:
        N00B_DBG_CALL_NESTED(n00b_rw_init, (void *)l);
        return;
    case N00B_NLT_CV:
        N00B_DBG_CALL_NESTED(n00b_condition_init, (void *)l);
        return;
    default:
        fprintf(stderr, "Fatal: Invalid lock type.\n");
        abort();
    }
}

#define n00b_lock_init(x, t) \
    _n00b_lock_init((n00b_lock_base_t *)x, t, "unnamed", __FILE__, __LINE__)
#define n00b_named_lock_init(x, t, n) \
    _n00b_lock_init((n00b_lock_base_t *)x, t, n, __FILE__, __LINE__)

#if defined(N00B_DEBUG)
#define n00b_lock_acquire(x) \
    _n00b_lock_acquire((n00b_lock_base_t *)x, __FILE__, __LINE__)

static inline void
_n00b_lock_acquire(n00b_lock_base_t *l, char *__file, int __line)
#else
#define n00b_lock_acquire(x) _n00b_lock_acquire((n00b_lock_base_t *)x)

static inline void
_n00b_lock_acquire(n00b_lock_base_t *l)
#endif
{
    switch (__n00b_get_lock_type(l)) {
    case N00B_NLT_MUTEX:
        N00B_DBG_CALL_NESTED(n00b_mutex_lock, (void *)l);
        return;
    case N00B_NLT_RW:
        N00B_DBG_CALL_NESTED(n00b_rw_write_lock, (void *)l);
        return;
    case N00B_NLT_CV:
        N00B_DBG_CALL_NESTED(n00b_condition_lock, (void *)l);
        return;
    default:
        __builtin_unreachable();
    }
}

#if defined(N00B_DEBUG)
extern void n00b_debug_all_locks(char *);
extern void n00b_debug_thread_locks(n00b_thread_t *, FILE *);

#define n00b_lock_release(x) \
    _n00b_lock_release((n00b_lock_base_t *)x, __FILE__, __LINE__)

static inline bool
_n00b_lock_release(n00b_lock_base_t *l, char *__file, int __line)
#else
#define n00b_lock_release(x) _n00b_lock_release((n00b_lock_base_t *)x)
static inline bool
_n00b_lock_release(n00b_lock_base_t *l)
#endif
{
    switch (__n00b_get_lock_type(l)) {
    case N00B_NLT_MUTEX:
        return N00B_DBG_CALL_NESTED(n00b_mutex_unlock, (void *)l);
    case N00B_NLT_RW:
        return N00B_DBG_CALL_NESTED(n00b_rw_unlock, (void *)l);
    case N00B_NLT_CV:
        return N00B_DBG_CALL_NESTED(n00b_condition_unlock, (void *)l);
    default:
        __builtin_unreachable();
    }
}

#define static_lock_init(x) \
    (n00b_lock_init((n00b_lock_base_t *)&(x)))
#define static_condition_init(x) static_lock_init(x)
#define static_rw_lock_init(x)   static_rw_lock_init(x)

static inline void
_n00b_lock_release_all(n00b_lock_base_t *l)
{
    while (!n00b_lock_release(l))
        ;
}

#define n00b_lock_release_all(x) _n00b_lock_release_all((n00b_lock_base_t *)x)

#if defined(N00B_USE_INTERNAL_API)
extern void N00B_DBG_DECL(n00b_lock_init_accounting, n00b_lock_base_t *, int);
extern int  N00B_DBG_DECL(n00b_lock_acquire_accounting,
                          n00b_lock_base_t *,
                          n00b_tsi_t *);
extern bool N00B_DBG_DECL(n00b_lock_release_accounting, n00b_lock_base_t *);

#if defined(N00B_DEBUG)
extern void
n00b_rlock_accounting(n00b_rwlock_t   *lock,
                      n00b_lock_log_t *record,
                      n00b_tsi_t      *tsi,
                      int              val,
                      char            *__file,
                      int              __line);
extern void
n00b_runlock_accounting(n00b_rwlock_t   *lock,
                        n00b_lock_log_t *record,
                        n00b_tsi_t      *tsi,
                        int              val,
                        char            *__file,
                        int              __line);

#define n00b_register_lock_wait(tsi, lock) \
    _n00b_register_lock_wait(tsi, lock, __file, __line)

extern void n00b_wait_log(n00b_tsi_t *,
                          n00b_lock_base_t *,
                          char *,
                          int,
                          bool);

extern void
_n00b_register_lock_wait(n00b_tsi_t *tsi, void *lock, char *f, int line);

#else
#define n00b_rlock_accounting(...)
#define n00b_runlock_accounting(...)

#define n00b_register_lock_wait(tsi, lock) tsi->lock_wait_target = (void *)lock

#endif

#if defined(N00B_DEBUG)
#define n00b_wait_done(tsi) _n00b_wait_done(tsi, __FILE__, __LINE__)

extern void _n00b_wait_done(n00b_tsi_t *tsi, char *__file, int __line);

#else
static inline void
n00b_wait_done(n00b_tsi_t *tsi)
{
    tsi->lock_wait_target = NULL;
}
#endif
#endif

// For locks incredibly likely to be uncontested.
typedef _Atomic(int) n00b_spin_lock_t;
static inline void
n00b_spin_lock(n00b_spin_lock_t *lock)
{
    while (atomic_fetch_or(lock, 1))
        ;
}

static inline void
n00b_spin_unlock(n00b_spin_lock_t *lock)
{
    atomic_store(lock, 0ULL);
}

static inline void
n00b_init_spin_lock(n00b_spin_lock_t *lock)
{
    n00b_spin_unlock(lock);
}

extern n00b_thread_t *n00b_thread_to_cancel;

static inline bool
n00b_other_canceled(n00b_futex_t *f)
{
    return false;
}
