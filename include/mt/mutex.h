// The top part is the part passed to the kernel for monitoring.
// For portability, this stuff probably shouldn't change often.
#pragma once
#include "n00b.h"

struct n00b_mutex_t {
    N00B_COMMON_LOCK_BASE;
    n00b_futex_t     futex;
    _Atomic uint32_t should_wake;
};

extern void N00B_DBG_DECL(n00b_mutex_init, n00b_mutex_t *);
extern int  N00B_DBG_DECL(n00b_mutex_lock, n00b_mutex_t *);
extern bool N00B_DBG_DECL(n00b_mutex_unlock, n00b_mutex_t *);
extern bool N00B_DBG_DECL(n00b_mutex_try_lock, n00b_mutex_t *, int usec);

#define n00b_mutex_init(x)   N00B_DBG_CALL(n00b_mutex_init, x)
#define n00b_mutex_lock(x)   N00B_DBG_CALL(n00b_mutex_lock, x)
#define n00b_mutex_unlock(x) N00B_DBG_CALL(n00b_mutex_unlock, x)
