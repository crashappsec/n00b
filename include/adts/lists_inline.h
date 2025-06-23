#pragma once

#include "n00b.h"

static inline bool
n00b_private_list_contains(n00b_list_t *list, void *item)
{
    return n00b_private_list_find(list, item) != -1;
}

static inline bool
n00b_list_contains(n00b_list_t *list, void *item)
{
    return n00b_list_find(list, item) != -1;
}

#define n00b_list(x, ...) \
    n00b_new(n00b_type_list(x) __VA_OPT__(, ) __VA_ARGS__, 0ULL)

#define n00b_to_list(t, ...) \
    _n00b_to_list(t, N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)

#define n00b_lock_list(x)            \
    if (x) {                         \
        n00b_lock_acquire(&x->lock); \
    }
#define n00b_unlock_list(x)          \
    if (x) {                         \
        n00b_lock_release(&x->lock); \
    }

static inline void
n00b_list_enforce_uniqueness_when_adding(n00b_list_t *l)
{
    l->enforce_uniqueness = true;
}

#define n00b_lock_list_read(list_obj) \
    n00b_rw_read_lock(&(list_obj->lock));

#define n00b_lock_list_write n00b_lock_list

extern n00b_list_t *_n00b_c_map(char *, ...);
extern n00b_list_t *n00b_from_cstr_list(char **arr, int64_t n);

#define n00b_c_map(s, ...) _n00b_c_map(s, N00B_VA(__VA_ARGS__))

// These should take n00b_sort_fn in the 2nd param, but declared
// as void * to avoid needing casts.
static inline void
n00b_private_list_sort(n00b_list_t *list, void *f)
{
    qsort(list->data, list->append_ix, sizeof(int64_t *), f);
}

static inline void
n00b_list_sort(n00b_list_t *list, void *f)
{
    n00b_lock_list(list);
    qsort(list->data, list->append_ix, sizeof(int64_t *), f);
    n00b_unlock_list(list);
}

#ifdef N00B_USE_INTERNAL_API
extern void n00b_list_add_if_unique(n00b_list_t *list,
                                    void        *item,
                                    bool (*fn)(void *, void *));
extern void n00b_list_init(n00b_list_t *, va_list);
#endif
