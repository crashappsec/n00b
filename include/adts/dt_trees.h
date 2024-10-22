#pragma once

#include "n00b.h"

typedef struct n00b_tree_node_t {
    struct n00b_tree_node_t **children;
    struct n00b_tree_node_t  *parent;
    n00b_obj_t                contents;
    int32_t                  alloced_kids;
    int32_t                  num_kids;
} n00b_tree_node_t;

typedef bool (*n00b_walker_fn)(n00b_tree_node_t *, int64_t, void *);
