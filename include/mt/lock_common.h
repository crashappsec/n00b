// The top part is the part passed to the kernel for monitoring.
// For portability, this stuff probably shouldn't change often.
#pragma once
#include "n00b.h"

enum {
    N00B_NLT_MUTEX = 1,
    N00B_NLT_RW    = 2,
    N00B_NLT_CV    = 3,
};

#if defined(N00B_USE_INTERNAL_API)
#define N00B_SPIN_LIMIT 16

// 0x80000000U is reserved for thread cancelation.
#undef N00B_THREAD_CANCEL
#define N00B_THREAD_CANCEL 0x80000000U
#define N00B_GIL           0x40000000U
#define N00B_BLOCKING      0x20000000U
#define N00B_STARTING      0x10000000U
#define N00B_SUSPEND       0x00000001U
#define N00B_RUNNING       0x00000000U
#define N00B_NO_OWNER      -1
#endif

#if defined(N00B_DEBUG)
#define N00B_PTR_WORDS 4

struct n00b_lock_log_t {
    n00b_lock_log_t *next_entry;
    n00b_lock_log_t *prev_entry;
    void            *obj;
    char            *file;
    int              line;
    bool             lock_op;
    int32_t          thread_id;
};

#else
#define N00B_PTR_WORDS 2

struct n00b_lock_log_t {
    n00b_lock_log_t *next_entry;
    n00b_lock_log_t *prev_entry;
    void            *obj;
    int32_t          thread_id;
};

#endif

typedef struct {
    int32_t owner;
    int16_t nesting;
    uint8_t type;
    uint8_t reserved;
} n00b_core_lock_info_t;

#if defined(N00B_DEBUG)
#define N00B_COMMON_LOCK_BASE                       \
    _Atomic(n00b_lock_base_t *)   next_thread_lock; \
    _Atomic(n00b_lock_base_t *)   prev_thread_lock; \
    char                         *debug_name;       \
    _Atomic n00b_core_lock_info_t data;             \
    n00b_alloc_hdr               *allocation;       \
    n00b_lock_log_t              *logs;             \
    char                         *creation_file;    \
    int                           creation_line;    \
    bool                          inited
#else
#define N00B_COMMON_LOCK_BASE                       \
    _Atomic(n00b_lock_base_t *)   next_thread_lock; \
    _Atomic(n00b_lock_base_t *)   prev_thread_lock; \
    _Atomic n00b_core_lock_info_t data;             \
    char                         *debug_name
#endif

struct n00b_lock_base_t {
    N00B_COMMON_LOCK_BASE;
};

#if defined(N00B_USE_INTERNAL_API)
static inline int64_t n00b_thread_id(void);

static inline bool
n00b_lock_already_owner(n00b_lock_base_t *lock)
{
    int32_t tid = n00b_thread_id();
    assert(tid >= 0);
    n00b_core_lock_info_t info = atomic_read(&lock->data);
    return info.owner == tid;
}
#endif

static inline void
_n00b_lock_set_debug_name(n00b_lock_base_t *l, char *name)
{
    l->debug_name = name;
}

#define n00b_lock_set_debug_name(x, y) _n00b_lock_set_debug_name(x, y)
