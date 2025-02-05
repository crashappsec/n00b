#pragma once

#include "n00b.h"

typedef struct n00b_tree_node_t n00b_tree_node_t;

typedef bool (*n00b_walker_fn)(n00b_tree_node_t *, int64_t, void *);
typedef n00b_string_t *(*n00b_node_repr_fn)(void *);

struct n00b_tree_node_t {
    struct n00b_tree_node_t **children;
    struct n00b_tree_node_t  *parent;
    n00b_obj_t                contents;
    n00b_node_repr_fn         repr_fn;
    int64_t                   noscan;
    int32_t                   alloced_kids;
    int32_t                   num_kids;
};

static inline void
n00b_tree_set_repr_callback(n00b_tree_node_t *t, n00b_node_repr_fn fn)
{
    t->repr_fn = fn;
}
