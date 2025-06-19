#define N00B_USE_INTERNAL_API
#include "n00b.h"

bool
n00b_tcmp(int64_t kind_as_64, n00b_tree_node_t *node)
{
    n00b_node_kind_t kind  = (n00b_node_kind_t)(unsigned int)kind_as_64;
    n00b_pnode_t    *pnode = n00b_get_pnode(node);

    if (kind == n00b_nt_any) {
        return true;
    }

    bool result = kind == pnode->kind;

    return result;
}

n00b_tpat_node_t *n00b_first_kid_id = NULL;
n00b_tpat_node_t *n00b_2nd_kid_id;
n00b_tpat_node_t *n00b_enum_items;
n00b_tpat_node_t *n00b_member_prefix;
n00b_tpat_node_t *n00b_member_last;
n00b_tpat_node_t *n00b_func_mods;
n00b_tpat_node_t *n00b_use_uri;
n00b_tpat_node_t *n00b_extern_params;
n00b_tpat_node_t *n00b_extern_return;
n00b_tpat_node_t *n00b_return_extract;
n00b_tpat_node_t *n00b_find_pure;
n00b_tpat_node_t *n00b_find_holds;
n00b_tpat_node_t *n00b_find_allocation_records;
n00b_tpat_node_t *n00b_find_extern_local;
n00b_tpat_node_t *n00b_find_extern_box;
n00b_tpat_node_t *n00b_param_extraction;
n00b_tpat_node_t *n00b_qualifier_extract;
n00b_tpat_node_t *n00b_sym_decls;
n00b_tpat_node_t *n00b_sym_names;
n00b_tpat_node_t *n00b_sym_type;
n00b_tpat_node_t *n00b_sym_init;
n00b_tpat_node_t *n00b_loop_vars;
n00b_tpat_node_t *n00b_case_branches;
n00b_tpat_node_t *n00b_case_else;
n00b_tpat_node_t *n00b_elif_branches;
n00b_tpat_node_t *n00b_else_condition;
n00b_tpat_node_t *n00b_case_cond;
n00b_tpat_node_t *n00b_case_cond_typeof;
n00b_tpat_node_t *n00b_opt_label;
n00b_tpat_node_t *n00b_id_node;
n00b_tpat_node_t *n00b_tuple_assign;

void
n00b_setup_treematch_patterns()
{
    if (n00b_first_kid_id != NULL) {
        return;
    }

    // Returns first child if it's an identifier, null otherwise.
    n00b_first_kid_id = n00b_tmatch(n00b_nt_any,
                                    0,
                                    n00b_tmatch(n00b_nt_identifier,
                                                1,
                                                N00B_PAT_NO_KIDS),
                                    n00b_tcount_content(n00b_nt_any,
                                                        0,
                                                        n00b_max_nodes,
                                                        0));

    n00b_2nd_kid_id    = n00b_tmatch(n00b_nt_any,
                                  0,
                                  n00b_tcontent(n00b_nt_any, 0),
                                  n00b_tmatch(n00b_nt_identifier,
                                              1,
                                              N00B_PAT_NO_KIDS),
                                  n00b_tcount_content(n00b_nt_any,
                                                      0,
                                                      n00b_max_nodes,
                                                      0));
    // Skips the identifier if there, and returns all the enum items,
    // regardless of the subtree shape.
    n00b_enum_items    = n00b_tmatch(n00b_nt_any,
                                  0,
                                  n00b_toptional(n00b_nt_identifier, 0),
                                  n00b_tcount_content(n00b_nt_enum_item,
                                                      1,
                                                      n00b_max_nodes,
                                                      1));
    n00b_member_last   = n00b_tfind(n00b_nt_member,
                                  0,
                                  n00b_tcount(n00b_nt_identifier,
                                              0,
                                              n00b_max_nodes,
                                              0),
                                  n00b_tmatch(n00b_nt_identifier,
                                              1,
                                              N00B_PAT_NO_KIDS));
    n00b_member_prefix = n00b_tfind(n00b_nt_member,
                                    0,
                                    n00b_tcount(n00b_nt_identifier,
                                                0,
                                                n00b_max_nodes,
                                                1,
                                                N00B_PAT_NO_KIDS),
                                    n00b_tmatch(n00b_nt_identifier,
                                                0,
                                                N00B_PAT_NO_KIDS));
    n00b_func_mods     = n00b_tfind(n00b_nt_func_mods,
                                0,
                                n00b_tcount(n00b_nt_func_mod,
                                            0,
                                            n00b_max_nodes,
                                            1,
                                            N00B_PAT_NO_KIDS),
                                0);
    n00b_extern_params = n00b_tfind(n00b_nt_extern_sig,
                                    0,
                                    n00b_tcount_content(n00b_nt_extern_param,
                                                        0,
                                                        n00b_max_nodes,
                                                        1),
                                    n00b_tcount_content(
                                        n00b_nt_lit_tspec_return_type,
                                        0,
                                        1,
                                        0),
                                    0);
    n00b_extern_return = n00b_tfind(
        n00b_nt_extern_sig,
        0,
        n00b_tcount_content(n00b_nt_extern_param, 0, n00b_max_nodes, 0),
        n00b_tcount_content(n00b_nt_lit_tspec_return_type,
                            0,
                            1,
                            1));
    n00b_return_extract   = n00b_tfind(n00b_nt_lit_tspec_return_type,
                                     0,
                                     n00b_tmatch(n00b_nt_any, 1, N00B_PAT_NO_KIDS));
    n00b_use_uri          = n00b_tfind(n00b_nt_simple_lit, 1, N00B_PAT_NO_KIDS);
    n00b_param_extraction = n00b_tfind(
        n00b_nt_formals,
        0,
        n00b_tcount_content(n00b_nt_sym_decl, 0, n00b_max_nodes, 1));
    n00b_find_pure               = n00b_tfind_content(n00b_nt_extern_pure, 1);
    n00b_find_holds              = n00b_tfind_content(n00b_nt_extern_holds, 1);
    n00b_find_allocation_records = n00b_tfind_content(n00b_nt_extern_allocs, 1);
    n00b_find_extern_local       = n00b_tfind_content(n00b_nt_extern_local, 1);
    n00b_find_extern_box         = n00b_tfind_content(n00b_nt_extern_box, 1);
    n00b_qualifier_extract       = n00b_tfind(n00b_nt_decl_qualifiers,
                                        0,
                                        n00b_tcount(n00b_nt_identifier,
                                                    0,
                                                    n00b_max_nodes,
                                                    1,
                                                    N00B_PAT_NO_KIDS));
    n00b_sym_decls               = n00b_tmatch(n00b_nt_variable_decls,
                                 0,
                                 n00b_tcount_content(n00b_nt_decl_qualifiers,
                                                     1,
                                                     1,
                                                     0),
                                 n00b_tcount_content(n00b_nt_sym_decl,
                                                     1,
                                                     n00b_max_nodes,
                                                     1));
    n00b_sym_names               = n00b_tfind(n00b_nt_sym_decl,
                                0,
                                n00b_tcount_content(n00b_nt_identifier,
                                                    1,
                                                    n00b_max_nodes,
                                                    1),
                                n00b_tcount_content(n00b_nt_lit_tspec, 0, 1, 0),
                                n00b_tcount_content(n00b_nt_assign, 0, 1, 0));
    n00b_sym_type                = n00b_tfind(n00b_nt_sym_decl,
                               0,
                               n00b_tcount_content(n00b_nt_identifier,
                                                   1,
                                                   n00b_max_nodes,
                                                   0),
                               n00b_tcount_content(n00b_nt_lit_tspec, 0, 1, 1),
                               n00b_tcount_content(n00b_nt_assign, 0, 1, 0));
    n00b_sym_init                = n00b_tfind(n00b_nt_sym_decl,
                               0,
                               n00b_tcount_content(n00b_nt_identifier,
                                                   1,
                                                   n00b_max_nodes,
                                                   0),
                               n00b_tcount_content(n00b_nt_lit_tspec, 0, 1, 0),
                               n00b_tcount_content(n00b_nt_assign, 0, 1, 1));
    n00b_loop_vars               = n00b_tfind(n00b_nt_variable_decls,
                                0,
                                n00b_tcount_content(n00b_nt_identifier, 1, 2, 1));
    n00b_case_branches           = n00b_tmatch(n00b_nt_any,
                                     0,
                                     n00b_tcount_content(n00b_nt_any, 0, 2, 0),
                                     n00b_tcount_content(n00b_nt_case,
                                                         1,
                                                         n00b_max_nodes,
                                                         1),
                                     n00b_tcount_content(n00b_nt_else, 0, 1, 0));
    n00b_elif_branches           = n00b_tmatch(n00b_nt_any,
                                     0,
                                     n00b_tcontent(n00b_nt_cmp, 0),
                                     n00b_tcontent(n00b_nt_body, 0),
                                     n00b_tcount_content(n00b_nt_elif,
                                                         0,
                                                         n00b_max_nodes,
                                                         1),
                                     n00b_tcount_content(n00b_nt_else, 0, 1, 0));
    n00b_else_condition          = n00b_tfind_content(n00b_nt_else, 1);
    n00b_case_cond               = n00b_tmatch(n00b_nt_any,
                                 0,
                                 n00b_toptional(n00b_nt_label, 0, N00B_PAT_NO_KIDS),
                                 n00b_tcontent(n00b_nt_expression, 1),
                                 n00b_tcount_content(n00b_nt_case,
                                                     1,
                                                     n00b_max_nodes,
                                                     0),
                                 n00b_tcount_content(n00b_nt_else, 0, 1, 0));

    n00b_case_cond_typeof = n00b_tmatch(n00b_nt_any,
                                        0,
                                        n00b_toptional(n00b_nt_label, 0),
                                        n00b_tcontent(n00b_nt_member, 1),
                                        n00b_tcount_content(n00b_nt_case,
                                                            1,
                                                            n00b_max_nodes,
                                                            0),
                                        n00b_tcount_content(n00b_nt_else, 0, 1, 0));
    n00b_opt_label        = n00b_tfind(n00b_nt_label, 1, N00B_PAT_NO_KIDS);
    n00b_id_node          = n00b_tfind(n00b_nt_identifier, 1, N00B_PAT_NO_KIDS);
    n00b_tuple_assign     = n00b_tmatch(n00b_nt_assign,
                                    0,
                                    n00b_tmatch(n00b_nt_expression,
                                                0,
                                                n00b_tcontent(n00b_nt_lit_tuple, 1)),
                                    n00b_tcontent(n00b_nt_expression, 0));

    n00b_gc_register_root(&n00b_first_kid_id, 1);
    n00b_gc_register_root(&n00b_2nd_kid_id, 1);
    n00b_gc_register_root(&n00b_enum_items, 1);
    n00b_gc_register_root(&n00b_member_prefix, 1);
    n00b_gc_register_root(&n00b_member_last, 1);
    n00b_gc_register_root(&n00b_func_mods, 1);
    n00b_gc_register_root(&n00b_use_uri, 1);
    n00b_gc_register_root(&n00b_extern_params, 1);
    n00b_gc_register_root(&n00b_extern_return, 1);
    n00b_gc_register_root(&n00b_return_extract, 1);
    n00b_gc_register_root(&n00b_find_pure, 1);
    n00b_gc_register_root(&n00b_find_holds, 1);
    n00b_gc_register_root(&n00b_find_allocation_records, 1);
    n00b_gc_register_root(&n00b_find_extern_local, 1);
    n00b_gc_register_root(&n00b_find_extern_box, 1);
    n00b_gc_register_root(&n00b_param_extraction, 1);
    n00b_gc_register_root(&n00b_qualifier_extract, 1);
    n00b_gc_register_root(&n00b_sym_decls, 1);
    n00b_gc_register_root(&n00b_sym_names, 1);
    n00b_gc_register_root(&n00b_sym_type, 1);
    n00b_gc_register_root(&n00b_sym_init, 1);
    n00b_gc_register_root(&n00b_loop_vars, 1);
    n00b_gc_register_root(&n00b_case_branches, 1);
    n00b_gc_register_root(&n00b_case_else, 1);
    n00b_gc_register_root(&n00b_elif_branches, 1);
    n00b_gc_register_root(&n00b_else_condition, 1);
    n00b_gc_register_root(&n00b_case_cond, 1);
    n00b_gc_register_root(&n00b_case_cond_typeof, 1);
    n00b_gc_register_root(&n00b_opt_label, 1);
    n00b_gc_register_root(&n00b_id_node, 1);
    n00b_gc_register_root(&n00b_tuple_assign, 1);
}

n00b_obj_t
n00b_node_to_callback(n00b_module_t    *ctx,
                      n00b_tree_node_t *n)
{
    if (!n00b_node_has_type(n, n00b_nt_lit_callback)) {
        return NULL;
    }

    n00b_string_t *name = n00b_node_text(n00b_tree_get_child(n, 0));
    n00b_type_t   *type = NULL;

    if (n->num_kids > 1) {
        type = n00b_node_to_type(ctx, n00b_tree_get_child(n, 1), NULL);
    }

    n00b_callback_info_t *result = n00b_callback_info_init();

    n00b_pnode_t *pn = n00b_get_pnode(n);
    pn->type         = type;
    pn->value        = (void *)result;

    result->target_symbol_name = name;
    result->target_type        = type;
    result->decl_loc           = n;

    return result;
}

n00b_type_t *
n00b_node_to_type(n00b_module_t    *ctx,
                  n00b_tree_node_t *n,
                  n00b_dict_t      *type_ctx)
{
    if (type_ctx == NULL) {
        type_ctx = n00b_new(n00b_type_dict(n00b_type_string(), n00b_type_ref()));
    }

    n00b_pnode_t  *pnode = n00b_get_pnode(n);
    n00b_string_t *varname;
    n00b_type_t   *t;
    bool           found;
    int            numkids;

    switch (pnode->kind) {
    case n00b_nt_lit_tspec:
        return n00b_node_to_type(ctx, n00b_tree_get_child(n, 0), type_ctx);
    case n00b_nt_lit_tspec_tvar:
        varname = n00b_node_text(n00b_tree_get_child(n, 0));
        t       = n00b_dict_get(type_ctx, varname, &found);
        if (!found) {
            t       = n00b_new_typevar();
            t->name = varname;
            n00b_dict_put(type_ctx, varname, t);
        }
        return t;
    case n00b_nt_lit_tspec_named_type:
        varname = n00b_node_text(n);
        for (int i = 0; i < N00B_NUM_BUILTIN_DTS; i++) {
            if (!strcmp(varname->data, n00b_base_type_info[i].name)) {
                return n00b_bi_types[i];
            }
        }

        n00b_add_error(ctx, n00b_err_unk_primitive_type, n, varname);
        return n00b_new_typevar();

    case n00b_nt_lit_tspec_parameterized_type:
        varname = n00b_node_text(n);
        // Need to do this more generically, but OK for now.
        if (!strcmp(varname->data, "list")) {
            return n00b_type_list(n00b_node_to_type(ctx,
                                                    n00b_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "list")) {
            return n00b_type_list(n00b_node_to_type(ctx,
                                                    n00b_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "tree")) {
            return n00b_type_tree(n00b_node_to_type(ctx,
                                                    n00b_tree_get_child(n, 0),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "set")) {
            return n00b_type_set(n00b_node_to_type(ctx,
                                                   n00b_tree_get_child(n, 0),
                                                   type_ctx));
        }
        if (!strcmp(varname->data, "dict")) {
            return n00b_type_dict(n00b_node_to_type(ctx,
                                                    n00b_tree_get_child(n, 0),
                                                    type_ctx),
                                  n00b_node_to_type(ctx,
                                                    n00b_tree_get_child(n, 1),
                                                    type_ctx));
        }
        if (!strcmp(varname->data, "tuple")) {
            n00b_list_t *subitems;

            subitems = n00b_new(n00b_type_list(n00b_type_typespec()));

            for (int i = 0; i < n00b_tree_get_number_children(n); i++) {
                n00b_list_append(subitems,
                                 n00b_node_to_type(ctx,
                                                   n00b_tree_get_child(n, i),
                                                   type_ctx));
            }

            return n00b_type_tuple_from_list(subitems);
        }
        n00b_add_error(ctx, n00b_err_unk_param_type, n);
        return n00b_new_typevar();
    case n00b_nt_lit_tspec_func:
        numkids = n00b_tree_get_number_children(n);
        if (numkids == 0) {
            return n00b_type_varargs_fn(n00b_new_typevar(), 0);
        }

        n00b_list_t      *args = n00b_new(n00b_type_list(n00b_type_typespec()));
        n00b_tree_node_t *kid  = n00b_tree_get_child(n, numkids - 1);
        bool              va   = false;

        pnode = n00b_get_pnode(kid);

        if (pnode->kind == n00b_nt_lit_tspec_return_type) {
            t = n00b_node_to_type(ctx, n00b_tree_get_child(kid, 0), type_ctx);
            numkids--;
        }
        else {
            t = n00b_new_typevar();
        }

        for (int i = 0; i < numkids; i++) {
            kid = n00b_tree_get_child(n, i);

            if (i + 1 == numkids) {
                pnode = n00b_get_pnode(kid);

                if (pnode->kind == n00b_nt_lit_tspec_varargs) {
                    va  = true;
                    kid = n00b_tree_get_child(kid, 0);
                }
            }

            n00b_list_append(args, n00b_node_to_type(ctx, kid, type_ctx));
        }

        return n00b_type_fn(t, args, va);

    default:
        n00b_unreachable();
    }
}

n00b_list_t *
n00b_apply_pattern_on_node(n00b_tree_node_t *node, n00b_tpat_node_t *pattern)
{
    n00b_list_t *cap = NULL;
    bool         ok  = n00b_tree_match(node,
                              pattern,
                              (n00b_cmp_fn)n00b_tcmp,
                              &cap);
    if (!ok) {
        return NULL;
    }

    for (int i = 0; i < n00b_list_len(cap); i++) {
        n00b_assert(n00b_list_get(cap, i, NULL) != NULL);
    }
    return cap;
}

// Return the first capture if there's a match, and NULL if not.
n00b_tree_node_t *
n00b_get_match_on_node(n00b_tree_node_t *node, n00b_tpat_node_t *pattern)
{
    n00b_list_t *cap = n00b_apply_pattern_on_node(node, pattern);

    if (cap != NULL) {
        return n00b_list_get(cap, 0, NULL);
    }

    return NULL;
}
