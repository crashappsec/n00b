#pragma once
#include "n00b.h"

#if !defined(N00B_DICT_MIN_SIZE_LOG) || (N00B_DICT_MIN_SIZE_LOG < 3)
#define N00B_DICT_MIN_SIZE_LOG 4
#endif

#undef N00B_DICT_MIN_SIZE
#define N00B_DICT_MIN_SIZE (1 << N00B_DICT_MIN_SIZE_LOG)

typedef __int128_t (*n00b_hash_fn)(void *);

typedef struct {
    __int128_t       hv;
    void            *key;
    void            *value;
    uint32_t         insert_order;
    _Atomic uint32_t flags;
} n00b_dict_bucket_t;

typedef struct {
    uint32_t           last_slot;
    uint32_t           threshold;
    _Atomic uint64_t   used_count;
    uint32_t           mmap_sz; // Only for non GC'd items.
    n00b_dict_bucket_t buckets[];
} n00b_dict_store_t;

typedef struct n00b_dict_t {
    n00b_hash_fn                 fn;
    _Atomic(n00b_dict_store_t *) store;
    _Atomic uint32_t             insertion_epoch;
    _Atomic uint32_t             wait_ct;
    _Atomic uint32_t             length;
    n00b_futex_t                 futex;
    uint32_t                     use_mmap        : 1;
    uint32_t                     no_heap_data    : 1;
    uint32_t                     non_object_keys : 1;
} n00b_dict_t;

typedef struct {
    void    *key;
    void    *value;
    uint64_t order;
} n00b_dict_raw_item_t;

extern void  _n00b_dict_init(n00b_dict_t *, ...);
extern void  n00b_dict_init_valist(n00b_dict_t *, va_list);
extern void *_n00b_dict_put(n00b_dict_t *d, void *key, void *value);
extern void *_n00b_dict_get(n00b_dict_t *d, void *key, bool *found);
extern bool  _n00b_dict_replace(n00b_dict_t *d, void *key, void *value);
extern bool  _n00b_dict_add(n00b_dict_t *d, void *key, void *value);
extern bool  _n00b_dict_remove(n00b_dict_t *d, void *key);
extern bool  _n00b_dict_cas(n00b_dict_t *d,
                            void        *key,
                            void       **old_item_ptr,
                            void        *new_item,
                            bool         expect_empty,
                            bool         delete_existing);

#define n00b_dict_init(x, ...) \
    _n00b_dict_init(x, N00B_VA(__VA_ARGS__))
#define n00b_dict_put(d, k, v) \
    _n00b_dict_put(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict_get(d, k, b) \
    _n00b_dict_get(d, ((void *)(int64_t)k), b)

#define n00b_dict_replace(d, k, v) \
    _n00b_dict_replace(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict_add(d, k, v) \
    _n00b_dict_add(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict_remove(d, k) \
    _n00b_dict_remove(d, ((void *)(int64_t)k))

#define n00b_dict_cas(d, k, o, n, b1, b2) \
    _n00b_dict_cas(d,                     \
                   ((void *)(int64_t)k),  \
                   ((void *)(int64_t)o),  \
                   ((void *)(int64_t)n),  \
                   b1,                    \
                   b2)

extern n00b_string_t        *n00b_dict_to_string(n00b_dict_t *d);
extern n00b_dict_t          *n00b_dict_shallow_copy(n00b_dict_t *d);
extern int64_t               n00b_dict_len(n00b_dict_t *d);
extern n00b_dict_t          *n00b_dict_plus(n00b_dict_t *d1, n00b_dict_t *d2);
extern n00b_dict_t          *n00b_dict_copy(n00b_dict_t *d);
extern n00b_dict_raw_item_t *n00b_dict_raw_view(n00b_dict_t *d, uint32_t *n);
extern int64_t              *n00b_dict_items_array(n00b_dict_t *d, uint32_t *n);
extern n00b_list_t          *_n00b_dict_keys(n00b_dict_t *d, ...);
extern n00b_list_t          *_n00b_dict_values(n00b_dict_t *d, ...);
extern n00b_list_t          *_n00b_dict_items(n00b_dict_t *d, ...);
extern n00b_dict_t          *n00b_new_unmanaged_dict(void);
extern __int128_t            n00b_cstring_hash(char *);
extern __int128_t            n00b_word_hash(uint64_t);
extern __int128_t            n00b_string_hash(n00b_string_t *);

#define n00b_dict_keys(x, ...) \
    _n00b_dict_keys(x, N00B_VA(__VA_ARGS__))

#define n00b_dict_values(x, ...) \
    _n00b_dict_values(x, N00B_VA(__VA_ARGS__))

#define n00b_dict_items(x, ...) \
    _n00b_dict_items(x, N00B_VA(__VA_ARGS__))

#define n00b_dict(x, y, ...) \
    n00b_new(n00b_type_dict(x, y), N00B_VA(__VA_ARGS__))

static inline bool
n00b_dict_contains(n00b_dict_t *d, void *v)
{
    bool found;

    n00b_dict_get(d, v, &found);

    return found;
}

#ifdef N00B_USE_INTERNAL_API
extern bool n00b_dict_lock(n00b_dict_t *d, bool try, uint32_t *count);
extern void n00b_dict_unlock_post_copy(n00b_dict_t *d);

#define N00B_HT_FLAG_MUTEX   1
#define N00B_HT_FLAG_COPYING 2
#define N00B_HT_FLAG_DELETED 4
#define N00B_HT_FLAG_MOVING  8

#endif
