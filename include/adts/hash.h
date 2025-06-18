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
    uint32_t last_slot;
    uint32_t threshold;
    union {
        _Atomic uint64_t used_count;
        void            *next_store;
    } info;
    uint32_t           mmap_sz; // Only for non GC'd items.
    n00b_dict_bucket_t buckets[];
} n00b_dict_store_t;

typedef struct {
    n00b_hash_fn                 fn;
    _Atomic(n00b_dict_store_t *) store;
    _Atomic uint32_t             insertion_epoch;
    _Atomic uint32_t             wait_ct;
    n00b_futex_t                 futex;
    uint32_t                     use_mmap    : 1;
    uint32_t                     custom_hash : 1;
} n00b_dict2_t;

typedef struct {
    void    *key;
    void    *value;
    uint32_t order;
} n00b_dict_raw_item_t;

extern void n00b_dict2_init(n00b_dict2_t *dict,
                            uint32_t      n_buckets,
                            bool          use_mmap,
                            bool          cstring);

extern void *_n00b_dict2_put(n00b_dict2_t *d, void *key, void *value);
extern void *_n00b_dict2_get(n00b_dict2_t *d, void *key, bool *found);
extern bool  _n00b_dict2_replace(n00b_dict2_t *d, void *key, void *value);
extern bool  _n00b_dict2_add(n00b_dict2_t *d, void *key, void *value);
extern bool  _n00b_dict2_remove(n00b_dict2_t *d, void *key);
extern bool  _n00b_dict2_cas(n00b_dict2_t *d,
                             void         *key,
                             void         *old_item,
                             void         *new_item,
                             bool          expect_empty,
                             bool          delete_existing);

#define n00b_dict2_put(d, k, v) \
    _n00b_dict2_put(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict2_get(d, k, b) \
    _n00b_dict2_get(d, ((void *)(int64_t)k), b)

#define n00b_dict2_replace(d, k, v) \
    _n00b_dict2_replace(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict2_add(d, k, v) \
    _n00b_dict2_add(d, ((void *)(int64_t)k), ((void *)(int64_t)v))

#define n00b_dict2_remove(d, k) \
    _n00b_dict2_remove(d, ((void *)(int64_t)k))

#define n00b_dict2_cas(d, k, o, n, b1, b2) \
    _n00b_dict2_cas(d,                     \
                    ((void *)(int64_t)k),  \
                    ((void *)(int64_t)o),  \
                    ((void *)(int64_t)n),  \
                    b1,                    \
                    b2)

extern n00b_dict_raw_item_t *n00b_dict2_raw_view(n00b_dict2_t *d, uint32_t *n);
extern n00b_list_t          *_n00b_dict2_keys(n00b_dict2_t *d, ...);
extern n00b_list_t          *_n00b_dict2_values(n00b_dict2_t *d, ...);
extern n00b_list_t          *_n00b_dict2_items(n00b_dict2_t *d, ...);

#define n00b_dict2_keys(x, ...) \
    _n00b_dict2_keys(x __VA_OPT__(, ) __VA_ARGS__, 0ULL)

#define n00b_dict2_values(x, ...) \
    _n00b_dict2_values(x __VA_OPT__(, ) __VA_ARGS__, 0ULL)

#define n00b_dict2_items(x, ...) \
    _n00b_dict2_items(x __VA_OPT__(, ) __VA_ARGS__, 0ULL)
