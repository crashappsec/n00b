#pragma once

#include "n00b.h"

extern n00b_list_t      *n00b_tree_children(n00b_tree_node_t *);
extern n00b_tree_node_t *n00b_tree_get_child(n00b_tree_node_t *, int64_t);
extern n00b_tree_node_t *n00b_tree_add_node(n00b_tree_node_t *, void *);
extern n00b_tree_node_t *n00b_tree_prepend_node(n00b_tree_node_t *, void *);
extern void             n00b_tree_adopt_node(n00b_tree_node_t *,
                                            n00b_tree_node_t *);
extern void             n00b_tree_adopt_and_prepend(n00b_tree_node_t *,
                                                   n00b_tree_node_t *);

extern n00b_tree_node_t *
n00b_tree_str_transform(n00b_tree_node_t *, n00b_str_t *(*fn)(void *));

void n00b_tree_walk_with_cycles(n00b_tree_node_t *,
                               n00b_walker_fn,
                               n00b_walker_fn,
                               void *);
void n00b_tree_walk(n00b_tree_node_t *, n00b_walker_fn, void *);

static inline n00b_obj_t
n00b_tree_get_contents(n00b_tree_node_t *t)
{
    return t->contents;
}

static inline void
n00b_tree_replace_contents(n00b_tree_node_t *t, void *new_contents)
{
    t->contents = new_contents;
}

static inline int64_t
n00b_tree_get_number_children(n00b_tree_node_t *t)
{
    return t->num_kids;
}

static inline n00b_tree_node_t *
n00b_tree_get_parent(n00b_tree_node_t *t)
{
    return t->parent;
}

// For use from Nim where we only instantiate w/ strings.
static inline n00b_tree_node_t *
n00b_tree(n00b_str_t *s)
{
    return n00b_new(n00b_type_tree(n00b_type_utf32()),
                   n00b_kw("contents", n00b_ka(s)));
}

static inline n00b_tree_node_t *
n00b_new_tree_node(n00b_type_t *t, void *node)
{
    return n00b_new(n00b_type_tree(t), n00b_kw("contents", n00b_ka(node)));
}
