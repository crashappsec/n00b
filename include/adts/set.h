#pragma once

#include "n00b.h"

typedef n00b_dict_t n00b_set_t;

#define n00b_set_contains(s, v)  n00b_dict_contains(s, v)
#define n00b_set_put(s, v)       n00b_dict_put(s, v, NULL)
#define n00b_set_add(s, v)       n00b_dict_add(s, v, NULL)
#define n00b_set_remove(s, v)    n00b_dict_remove(s, v)
#define n00b_set_len(s)          n00b_dict_len(s)
#define n00b_set_shallow_copy(s) n00b_dict_copy(s)

extern n00b_list_t *n00b_set_items(n00b_set_t *);
extern bool         n00b_set_is_eq(n00b_set_t *, n00b_set_t *);
extern bool         _n00b_set_is_superset(n00b_set_t *, n00b_set_t *, ...);
extern bool         n00b_sets_are_disjoint(n00b_set_t *, n00b_set_t *);
extern void        *_n00b_set_any_item(n00b_set_t *, ...);
extern n00b_set_t  *n00b_set_difference(n00b_set_t *, n00b_set_t *);
extern n00b_set_t  *n00b_set_union(n00b_set_t *, n00b_set_t *);
extern n00b_set_t  *n00b_set_intersection(n00b_set_t *, n00b_set_t *);
extern n00b_set_t  *n00b_set_disjunction(n00b_set_t *, n00b_set_t *);

#define n00b_set_is_superset(x, y, ...) \
    _n00b_set_is_superset(x, y __VA_OPT__(, ) __VA_ARGS__, 0ULL)
#define n00b_set_is_subset(x, y, ...) \
    _n00b_set_is_superset(y, x __VA_OPT__(, ) __VA_ARGS__, 0ULL)
#define n00b_set_any_item(x, ...) \
    _n00b_set_any_item(x __VA_OPT__(, ) __VA_ARGS__, 0ULL)

#define n00b_set(x, ...) \
    n00b_new(n00b_type_set(x) __VA_OPT__(, ) __VA_ARGS__, 0ULL)
