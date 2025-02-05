#pragma once

#include "n00b.h"

extern n00b_set_t  *n00b_set_shallow_copy(n00b_set_t *);
extern n00b_list_t *n00b_set_to_list(n00b_set_t *);

#define n00b_set_contains    hatrack_set_contains
#define n00b_set_put         hatrack_set_put
#define n00b_set_add         hatrack_set_add
#define n00b_set_remove      hatrack_set_remove
#define n00b_set_items       hatrack_set_items
#define n00b_set_items_sort  hatrack_set_items_sort
#define n00b_set_is_eq       hatrack_set_is_eq
#define n00b_set_is_superset hatrack_set_is_superset
#define n00b_set_is_subset   hatrack_set_is_subset
#define n00b_set_is_disjoint hatrack_set_is_disjoint
#define n00b_set_any_item    hatrack_set_any_item

static inline n00b_set_t *
n00b_set_difference(n00b_set_t *s1, n00b_set_t *s2)
{
    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    hatrack_set_difference(s1, s2, result);
    return result;
}

static inline n00b_set_t *
n00b_set_union(n00b_set_t *s1, n00b_set_t *s2)
{
    if (!s1) {
        return s2;
    }
    if (!s2) {
        return s1;
    }
    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    hatrack_set_union(s1, s2, result);
    return result;
}

static inline n00b_set_t *
n00b_set_intersection(n00b_set_t *s1, n00b_set_t *s2)
{
    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    hatrack_set_intersection(s1, s2, result);
    return result;
}

static inline n00b_set_t *
n00b_set_disjunction(n00b_set_t *s1, n00b_set_t *s2)
{
    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    hatrack_set_disjunction(s1, s2, result);
    return result;
}

#define n00b_set(x) n00b_new(n00b_type_set(x))
