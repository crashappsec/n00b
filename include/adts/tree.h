#pragma once

#include "n00b.h"

extern n00b_list_t      *n00b_tree_children(n00b_tree_node_t *);
extern n00b_tree_node_t *n00b_tree_get_child(n00b_tree_node_t *, int64_t);
extern n00b_tree_node_t *n00b_tree_add_node(n00b_tree_node_t *, void *);
extern n00b_tree_node_t *n00b_tree_prepend_node(n00b_tree_node_t *, void *);
extern void              n00b_tree_adopt_node(n00b_tree_node_t *,
                                              n00b_tree_node_t *);
extern void              n00b_tree_adopt_and_prepend(n00b_tree_node_t *,
                                                     n00b_tree_node_t *);

// TODO: Nuke this.
extern n00b_tree_node_t *
n00b_tree_string_transform(n00b_tree_node_t *, n00b_string_t *(*fn)(void *));

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

static inline n00b_tree_node_t *
n00b_tree(void *contents)
{
    n00b_type_t *t;

    if (!n00b_in_heap(contents)) {
        t = n00b_type_int();
    }
    else {
        t = n00b_get_my_type(contents);

        if (!t) {
            t = n00b_type_ref();
        }
    }

    return n00b_new(n00b_type_tree(t),
                    n00b_header_kargs("contents", (int64_t)contents));
}

static inline n00b_tree_node_t *
n00b_new_tree_node(n00b_type_t *t, void *node)
{
    return n00b_new(n00b_type_tree(t),
                    n00b_header_kargs("contents", (int64_t)node));
}
