#pragma once
#include "n00b.h"

typedef struct {
    union {
        int64_t i; // Absolute
        double  d; // Pct from 0 - 1
    } value;
    bool pct;
} n00b_layout_size_t;

static inline int64_t
compute_size(n00b_layout_size_t *spec, int64_t w)
{
    if (!spec->pct) {
        return spec->value.i;
    }
    double d = spec->value.d * w + 0.5;

    return (int64_t)d;
}

typedef struct {
    n00b_layout_size_t min;
    n00b_layout_size_t max;
    n00b_layout_size_t pref;
    int64_t            priority;
    int64_t            flex_multiple;
    int64_t            child_pad;
} n00b_layout_t;

typedef struct {
    n00b_tree_node_t *cell;
    int64_t           computed_min;
    int64_t           computed_max;
    int64_t           computed_pref;
    int64_t           cached_priority;
    int64_t           cached_flex_multiple;
    int64_t           order;
    int64_t           size;
} n00b_layout_result_t;

extern n00b_tree_node_t *n00b_layout_calculate(n00b_tree_node_t *, int64_t);
extern n00b_tree_node_t *_n00b_new_layout_cell(n00b_tree_node_t *, ...);

#define n00b_new_layout_cell(...) _n00b_new_layout_cell(N00B_VA(__VA_ARGS__))

#define n00b_new_layout() _n00b_new_layout_cell(NULL, NULL);
