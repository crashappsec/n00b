#pragma once
#include "n00b.h"

extern bool           n00b_parse(n00b_module_t *);
extern bool           n00b_parse_type(n00b_module_t *);
extern void           n00b_print_parse_node(n00b_tree_node_t *);
extern n00b_string_t *n00b_node_type_name(n00b_node_kind_t);
extern n00b_string_t *n00b_repr_n00b_parse_node(n00b_pnode_t *one);

#ifdef N00B_USE_INTERNAL_API

static inline n00b_string_t *
n00b_identifier_text(n00b_token_t *tok)
{
    if (tok->literal_value == NULL) {
        tok->literal_value = n00b_token_raw_content(tok);
    }

    return (n00b_string_t *)tok->literal_value;
}

static inline n00b_string_t *
n00b_node_text(n00b_tree_node_t *n)
{
    n00b_pnode_t *p = n00b_tree_get_contents(n);
    return n00b_token_raw_content(p->token);
}

static inline int32_t
n00b_node_get_line_number(n00b_tree_node_t *n)
{
    if (n == NULL) {
        return 0;
    }

    n00b_pnode_t *p = n00b_tree_get_contents(n);

    return p->token->line_no;
}

static inline n00b_string_t *
n00b_node_get_loc_str(n00b_tree_node_t *n)
{
    n00b_pnode_t *p = n00b_tree_get_contents(n);

    return n00b_token_get_location_str(p->token);
}

static inline n00b_string_t *
n00b_node_list_join(n00b_list_t *nodes, n00b_string_t *joiner, bool trailing)
{
    int64_t      n      = n00b_list_len(nodes);
    n00b_list_t *strarr = n00b_new(n00b_type_list(n00b_type_string()));

    for (int64_t i = 0; i < n; i++) {
        n00b_tree_node_t *one = n00b_list_get(nodes, i, NULL);
        n00b_list_append(strarr, n00b_node_text(one));
    }

    return n00b_string_join(strarr,
                            joiner,
                            n00b_kw("add_trailing", n00b_ka(trailing)));
}

static inline int
n00b_node_num_kids(n00b_tree_node_t *t)
{
    return (int)t->num_kids;
}

static inline n00b_obj_t
n00b_node_simp_literal(n00b_tree_node_t *n)
{
    n00b_pnode_t *p   = n00b_tree_get_contents(n);
    n00b_token_t *tok = p->token;

    return tok->literal_value;
}

#endif
