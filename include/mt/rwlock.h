#pragma once
#include "n00b.h"

#if defined(N00B_DEBUG)

typedef struct n00b_rwdebug_t {
    struct n00b_rwdebug_t *prev;
    struct n00b_rwdebug_t *next;
    char                  *file;
    int                    line;
    bool                   lock_op;
    int32_t                thread_id;
    int32_t                nest;
    char                  *trace;
} n00b_rwdebug_t;

#endif

typedef struct n00b_rwlock_t {
    N00B_COMMON_LOCK_BASE;
    n00b_futex_t futex;
#ifdef N00B_DEBUG
    n00b_rwdebug_t *first_entry;
    n00b_rwdebug_t *last_entry;
#endif
} n00b_rwlock_t;

extern void N00B_DBG_DECL(n00b_rw_init, n00b_rwlock_t *);
extern int  N00B_DBG_DECL(n00b_rw_write_lock, n00b_rwlock_t *);
extern void N00B_DBG_DECL(n00b_rw_read_lock, n00b_rwlock_t *);
extern bool N00B_DBG_DECL(n00b_rw_unlock, n00b_rwlock_t *);

#define n00b_rw_write_lock(l) N00B_DBG_CALL(n00b_rw_write_lock, l)
#define n00b_rw_read_lock(l)  N00B_DBG_CALL(n00b_rw_read_lock, l)
#define n00b_rw_unlock(l)     N00B_DBG_CALL(n00b_rw_unlock, l)
#define n00b_rw_lock_init(l)  N00B_DBG_CALL(n00b_rw_init, l)
#ifdef N00B_USE_INTERNAL_API

#define N00B_RW_UNLOCKED 0x00000000
#define N00B_RW_W_LOCK   0x40000000

#endif
