#pragma once
#include "n00b.h"

typedef struct n00b_tpat_node_t {
    struct n00b_tpat_node_t **children;
    n00b_obj_t                contents;
    int64_t                  min;
    int64_t                  max;
    uint64_t                 num_kids;
    unsigned int             walk        : 1;
    unsigned int             capture     : 1;
    unsigned int             ignore_kids : 1;
} n00b_tpat_node_t;

typedef n00b_string_t *(*n00b_pattern_fmt_fn)(void *);
extern n00b_tree_node_t *n00b_pat_repr(n00b_tpat_node_t *, n00b_pattern_fmt_fn);
