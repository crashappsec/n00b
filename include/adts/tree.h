#pragma once

#include "n00b.h"

#pragma once

#include "n00b.h"

typedef struct n00b_tree_node_t n00b_tree_node_t;

typedef bool (*n00b_walker_fn)(n00b_tree_node_t *, int64_t, void *);
typedef n00b_string_t *(*n00b_node_repr_fn)(void *);

struct n00b_tree_node_t {
    struct n00b_tree_node_t **children;
    struct n00b_tree_node_t  *parent;
    void                     *contents;
    n00b_node_repr_fn         repr_fn;
    int64_t                   noscan;
    int32_t                   alloced_kids;
    int32_t                   num_kids;
};

typedef struct n00b_tpat_node_t {
    struct n00b_tpat_node_t **children;
    void                     *contents;
    int64_t                   min;
    int64_t                   max;
    uint64_t                  num_kids;
    unsigned int              walk        : 1;
    unsigned int              capture     : 1;
    unsigned int              ignore_kids : 1;
} n00b_tpat_node_t;

extern n00b_list_t      *n00b_tree_children(n00b_tree_node_t *);
extern n00b_tree_node_t *n00b_tree_get_child(n00b_tree_node_t *, int64_t);
extern n00b_tree_node_t *n00b_tree_add_node(n00b_tree_node_t *, void *);
extern n00b_tree_node_t *n00b_tree_prepend_node(n00b_tree_node_t *, void *);
extern void              n00b_tree_adopt_node(n00b_tree_node_t *,
                                              n00b_tree_node_t *);
extern void              n00b_tree_adopt_and_prepend(n00b_tree_node_t *,
                                                     n00b_tree_node_t *);

typedef n00b_string_t *(*n00b_pattern_fmt_fn)(void *);
extern n00b_tree_node_t *n00b_pat_repr(n00b_tpat_node_t *, n00b_pattern_fmt_fn);
