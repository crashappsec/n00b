#pragma once

#include "n00b.h"

typedef struct {
    void **items;
    int    num_items;
} n00b_tuple_t;

extern void    n00b_tuple_set(n00b_tuple_t *, int64_t, void *);
extern void   *n00b_tuple_get(n00b_tuple_t *, int64_t);
extern int64_t n00b_tuple_len(n00b_tuple_t *);
extern void   *n00b_clean_internal_list(n00b_list_t *l);
