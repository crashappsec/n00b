#pragma once
#include "n00b.h"

#ifdef N00B_USE_INTERNAL_API

#ifdef N00B_DEBUG_PATTERNS

static inline void
n00b_print_type(n00b_obj_t o)
{
    if (o == NULL) {
        printf("<null>\n");
        return;
    }
    n00b_print(n00b_get_my_type(o));
}

extern n00b_string_t *n00b_node_type_name(n00b_node_kind_t);

static inline n00b_string_t *
n00b_content_formatter(void *contents)
{
    n00b_node_kind_t kind = (n00b_node_kind_t)(uint64_t)contents;

    if (kind == n00b_nt_any) {
        return n00b_rich("«em2»n00b_nt_any");
    }

    return n00b_cformat("«em2»«#»", n00b_node_type_name(kind));
}

static inline void
_show_pattern(n00b_tpat_node_t *pat)
{
    n00b_tree_node_t *t = n00b_pat_repr(pat, n00b_content_formatter);
    n00b_print(n00b_tree_format(t, NULL, NULL, true));
}

#define show_pattern(x)                                              \
    printf("Showing pattern: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
    _show_pattern(x)

#endif

#define n00b_get_pnode(x) ((x) ? n00b_tree_get_contents(x) : NULL)

extern bool         n00b_tcmp(int64_t, n00b_tree_node_t *);
extern void         n00b_setup_treematch_patterns();
extern n00b_type_t *n00b_node_to_type(n00b_module_t *,
                                      n00b_tree_node_t *,
                                      n00b_dict_t *);
extern n00b_obj_t   n00b_node_to_callback(n00b_module_t *,
                                          n00b_tree_node_t *);

static inline bool
n00b_node_has_type(n00b_tree_node_t *node, n00b_node_kind_t expect)
{
    n00b_pnode_t *pnode = (n00b_pnode_t *)n00b_get_pnode(node);
    return expect == pnode->kind;
}

static inline n00b_string_t *
n00b_get_litmod(n00b_pnode_t *p)
{
    return p->extra_info;
}

// Return the first capture if there's a match, and NULL if not.
extern n00b_tree_node_t *n00b_get_match_on_node(n00b_tree_node_t *,
                                                n00b_tpat_node_t *);

// Return every capture on match.
extern n00b_list_t *n00b_apply_pattern_on_node(n00b_tree_node_t *,
                                               n00b_tpat_node_t *);

extern n00b_tpat_node_t *n00b_first_kid_id;
extern n00b_tpat_node_t *n00b_2nd_kid_id;
extern n00b_tpat_node_t *n00b_enum_items;
extern n00b_tpat_node_t *n00b_member_prefix;
extern n00b_tpat_node_t *n00b_member_last;
extern n00b_tpat_node_t *n00b_func_mods;
extern n00b_tpat_node_t *n00b_use_uri;
extern n00b_tpat_node_t *n00b_extern_params;
extern n00b_tpat_node_t *n00b_extern_return;
extern n00b_tpat_node_t *n00b_return_extract;
extern n00b_tpat_node_t *n00b_find_pure;
extern n00b_tpat_node_t *n00b_find_holds;
extern n00b_tpat_node_t *n00b_find_allocation_records;
extern n00b_tpat_node_t *n00b_find_extern_local;
extern n00b_tpat_node_t *n00b_find_extern_box;
extern n00b_tpat_node_t *n00b_param_extraction;
extern n00b_tpat_node_t *n00b_qualifier_extract;
extern n00b_tpat_node_t *n00b_sym_decls;
extern n00b_tpat_node_t *n00b_sym_names;
extern n00b_tpat_node_t *n00b_sym_type;
extern n00b_tpat_node_t *n00b_sym_init;
extern n00b_tpat_node_t *n00b_loop_vars;
extern n00b_tpat_node_t *n00b_case_branches;
extern n00b_tpat_node_t *n00b_case_else;
extern n00b_tpat_node_t *n00b_elif_branches;
extern n00b_tpat_node_t *n00b_else_condition;
extern n00b_tpat_node_t *n00b_case_cond;
extern n00b_tpat_node_t *n00b_case_cond_typeof;
extern n00b_tpat_node_t *n00b_opt_label;
extern n00b_tpat_node_t *n00b_id_node;
extern n00b_tpat_node_t *n00b_tuple_assign;
#endif
