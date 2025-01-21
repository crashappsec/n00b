#define N00B_USE_INTERNAL_API
#include "n00b.h"

// clang-format off
char *n00b_symbol_kind_names[N00B_SK_NUM_SYM_KINDS] = {
    [N00B_SK_MODULE]      = "module",
    [N00B_SK_FUNC]        = "function",
    [N00B_SK_EXTERN_FUNC] = "extern function",
    [N00B_SK_ENUM_TYPE]   = "enum type",
    [N00B_SK_ENUM_VAL]    = "enum value",
    [N00B_SK_ATTR]        = "attribute",
    [N00B_SK_VARIABLE]    = "variable",
    [N00B_SK_FORMAL]      = "function parameter",
};
// clang-format on

void
n00b_sym_gc_bits(uint64_t *bitfield, n00b_symbol_t *sym)
{
    n00b_mark_raw_to_addr(bitfield, sym, &sym->value);
}

n00b_symbol_t *
n00b_new_symbol()
{
    n00b_symbol_t *result = n00b_gc_alloc_mapped(n00b_symbol_t, n00b_sym_gc_bits);
    result->ct            = n00b_gc_alloc_mapped(n00b_ct_sym_info_t,
                                      N00B_GC_SCAN_ALL);

    return result;
}

void
n00b_scope_gc_bits(uint64_t *bitfield, n00b_scope_t *scope)
{
    n00b_mark_raw_to_addr(bitfield, scope, &scope->symbols);
}

n00b_scope_t *
n00b_new_scope(n00b_scope_t *parent, n00b_scope_kind kind)
{
    n00b_scope_t *result = n00b_gc_alloc_mapped(n00b_scope_t, n00b_scope_gc_bits);
    result->parent       = parent;
    result->symbols      = n00b_dict(n00b_type_utf8(), n00b_type_ref());
    result->kind         = kind;

    return result;
}

n00b_utf8_t *
n00b_sym_get_best_ref_loc(n00b_symbol_t *sym)
{
    if (sym->loc) {
        return sym->loc;
    }

    n00b_tree_node_t *node = NULL;

    if (sym->ct->sym_defs != NULL) {
        node = n00b_list_get(sym->ct->sym_defs, 0, NULL);
    }

    if (node == NULL && sym->ct->sym_uses != NULL) {
        node = n00b_list_get(sym->ct->sym_defs, 0, NULL);
    }

    if (node) {
        return n00b_node_get_loc_str(node);
    }

    n00b_unreachable();
}

void
n00b_shadow_check(n00b_module_t *ctx,
                  n00b_symbol_t *sym,
                  n00b_scope_t  *cur_scope)
{
    n00b_scope_t *module_scope = ctx->module_scope;
    n00b_scope_t *global_scope = ctx->ct->global_scope;
    n00b_scope_t *attr_scope   = ctx->ct->attribute_scope;

    n00b_symbol_t *in_module = NULL;
    n00b_symbol_t *in_global = NULL;
    n00b_symbol_t *in_attr   = NULL;

    if (module_scope && module_scope != cur_scope) {
        in_module = hatrack_dict_get(module_scope->symbols, sym->name, NULL);
    }
    if (global_scope && global_scope != cur_scope) {
        in_global = hatrack_dict_get(global_scope->symbols, sym->name, NULL);
    }
    if (attr_scope && attr_scope != cur_scope) {
        in_attr = hatrack_dict_get(attr_scope->symbols, sym->name, NULL);
    }

    if (in_module) {
        if (cur_scope == global_scope) {
            n00b_add_error(ctx,
                           n00b_err_decl_mask,
                           sym->ct->declaration_node,
                           n00b_new_utf8("global"),
                           n00b_new_utf8("module"),
                           n00b_node_get_loc_str(in_module->ct->declaration_node));
            return;
        }

        n00b_add_error(ctx,
                       n00b_err_attr_mask,
                       sym->ct->declaration_node,
                       n00b_node_get_loc_str(in_module->ct->declaration_node));
        return;
    }

    if (in_global) {
        if (cur_scope == module_scope) {
            n00b_add_error(ctx,
                           n00b_err_decl_mask,
                           sym->ct->declaration_node,
                           n00b_new_utf8("module"),
                           n00b_new_utf8("global"),
                           n00b_node_get_loc_str(in_global->ct->declaration_node));
            return;
        }
        n00b_add_error(ctx,
                       n00b_err_attr_mask,
                       sym->ct->declaration_node,
                       n00b_node_get_loc_str(in_global->ct->declaration_node));
        return;
    }

    if (in_attr) {
        if (cur_scope == module_scope) {
            n00b_add_warning(ctx, n00b_warn_attr_mask, sym->ct->declaration_node);
        }
        else {
            n00b_add_error(ctx, n00b_err_attr_mask, sym->ct->declaration_node);
        }
    }
}

n00b_symbol_t *
n00b_declare_symbol(n00b_module_t    *ctx,
                    n00b_scope_t     *scope,
                    n00b_utf8_t      *name,
                    n00b_tree_node_t *node,
                    n00b_symbol_kind  kind,
                    bool             *success,
                    bool              err_if_present)
{
    n00b_symbol_t *entry        = n00b_new_symbol();
    n00b_pnode_t  *p            = node ? n00b_tree_get_contents(node) : NULL;
    entry->name                 = name;
    entry->ct->declaration_node = node;
    entry->type                 = n00b_new_typevar();
    entry->kind                 = kind;
    entry->ct->sym_defs         = n00b_list(n00b_type_ref());
    entry->loc                  = p ? n00b_format_module_location(ctx, p->token) : NULL;

    if (hatrack_dict_add(scope->symbols, name, entry)) {
        if (success != NULL) {
            *success = true;
        }

        switch (kind) {
        case N00B_SK_ATTR:
        case N00B_SK_VARIABLE:
            break;
        default:
            n00b_list_append(entry->ct->sym_defs, node);
        }
        return entry;
    }

    n00b_symbol_t *old = hatrack_dict_get(scope->symbols, name, NULL);

    if (success != NULL) {
        *success = false;
    }

    if (!err_if_present) {
        return old;
    }

    n00b_add_error(ctx,
                   n00b_err_invalid_redeclaration,
                   node,
                   name,
                   n00b_sym_kind_name(old),
                   n00b_node_get_loc_str(old->ct->declaration_node));

    return old;
}
n00b_symbol_t *
n00b_add_inferred_symbol(n00b_module_t    *ctx,
                         n00b_scope_t     *scope,
                         n00b_utf8_t      *name,
                         n00b_tree_node_t *node)
{
    n00b_symbol_t *entry = n00b_new_symbol();
    n00b_pnode_t  *p     = node ? n00b_tree_get_contents(node) : NULL;
    entry->name          = name;
    entry->type          = n00b_new_typevar();
    entry->loc           = p ? n00b_format_module_location(ctx, p->token) : NULL;

    if (scope->kind & (N00B_SCOPE_FUNC | N00B_SCOPE_FORMALS)) {
        entry->flags |= N00B_F_FUNCTION_SCOPE;
    }

    if (scope->kind & N00B_SK_ATTR) {
        entry->kind = N00B_SK_ATTR;
    }
    else {
        entry->kind = N00B_SK_VARIABLE;
    }

    if (!hatrack_dict_add(scope->symbols, name, entry)) {
        // Indicates a compile error, so return the old symbol.
        return hatrack_dict_get(scope->symbols, name, NULL);
    }

    return entry;
}

n00b_symbol_t *
n00b_add_or_replace_symbol(n00b_module_t    *ctx,
                           n00b_scope_t     *scope,
                           n00b_utf8_t      *name,
                           n00b_tree_node_t *node)
{
    n00b_symbol_t *entry = n00b_new_symbol();
    n00b_pnode_t  *p     = node ? n00b_tree_get_contents(node) : NULL;
    entry->name          = name;
    entry->type          = n00b_new_typevar();
    entry->kind          = N00B_SK_VARIABLE;
    entry->loc           = p ? n00b_format_module_location(ctx, p->token) : NULL;

    hatrack_dict_put(scope->symbols, name, entry);

    return entry;
}

// There's also a "n00b_symbol_lookup" below. We should resolve
// these names by deleting this and rewriting any call sites (TODO).
n00b_symbol_t *
n00b_lookup_symbol(n00b_scope_t *scope, n00b_utf8_t *name)
{
    return hatrack_dict_get(scope->symbols, name, NULL);
}

// Used for declaration comparisons. use_declared_type is passed when
// the ORIGINAL symbol had a declared type. If it doesn't, then we are
// comparing against a third place, which had an explicit decl, which
// will have the type stored in inferred_type.

static void
type_cmp_exact_match(n00b_module_t *new_ctx,
                     n00b_symbol_t *new_sym,
                     n00b_symbol_t *old_sym)
{
    n00b_type_t *t1 = new_sym->type;
    n00b_type_t *t2 = old_sym->type;

    switch (n00b_type_cmp_exact(t1, t2)) {
    case n00b_type_match_exact:
        // Link any type variables together now.
        old_sym->type = n00b_merge_types(t1, t2, NULL);
        return;
    case n00b_type_match_left_more_specific:
        // Even if the authoritative symbol did not have a declared
        // type, it got the field set whenever some other version of
        // the symbol explicitly declared. So it's the previous
        // location to complain about.
        n00b_add_error(new_ctx,
                       n00b_err_redecl_neq_generics,
                       new_sym->ct->declaration_node,
                       new_sym->name,
                       n00b_new_utf8("a less generic / more concrete"),
                       n00b_value_obj_repr(t2),
                       n00b_value_obj_repr(t1),
                       n00b_node_get_loc_str(old_sym->ct->declaration_node));
        return;
    case n00b_type_match_right_more_specific:
        n00b_add_error(new_ctx,
                       n00b_err_redecl_neq_generics,
                       new_sym->ct->declaration_node,
                       new_sym->name,
                       n00b_new_utf8("a more generic / less concrete"),
                       n00b_value_obj_repr(t2),
                       n00b_value_obj_repr(t1),
                       n00b_node_get_loc_str(old_sym->ct->declaration_node));
        return;
    case n00b_type_match_both_have_more_generic_bits:
        n00b_add_error(new_ctx,
                       n00b_err_redecl_neq_generics,
                       new_sym->ct->declaration_node,
                       new_sym->name,
                       n00b_new_utf8("a type with different generic parts"),
                       n00b_value_obj_repr(t2),
                       n00b_value_obj_repr(t1),
                       n00b_node_get_loc_str(old_sym->ct->declaration_node));
        return;
    case n00b_type_cant_match:
        n00b_add_error(new_ctx,
                       n00b_err_redecl_neq_generics,
                       new_sym->ct->declaration_node,
                       new_sym->name,
                       n00b_new_utf8("a completely incompatible"),
                       n00b_value_obj_repr(t2),
                       n00b_value_obj_repr(t1),
                       n00b_node_get_loc_str(old_sym->ct->declaration_node));
        return;
    }
}

// This is meant for statically merging global symbols that exist in
// multiple modules, etc.
//
// This happens BEFORE any type inferencing happens, so we only need
// to compare when exact types are provided.

bool
n00b_merge_symbols(n00b_module_t *ctx1,
                   n00b_symbol_t *sym1,
                   n00b_symbol_t *sym2) // The older symbol.
{
    if (sym1->kind != sym2->kind) {
        n00b_add_error(ctx1,
                       n00b_err_redecl_kind,
                       sym1->ct->declaration_node,
                       sym1->name,
                       n00b_sym_kind_name(sym2),
                       n00b_sym_kind_name(sym1),
                       n00b_node_get_loc_str(sym2->ct->declaration_node));
        return false;
    }

    switch (sym1->kind) {
    case N00B_SK_FUNC:
    case N00B_SK_EXTERN_FUNC:
    case N00B_SK_ENUM_TYPE:
    case N00B_SK_ENUM_VAL:
        n00b_add_error(ctx1,
                       n00b_err_no_redecl,
                       sym1->ct->declaration_node,
                       sym1->name,
                       n00b_sym_kind_name(sym1),
                       n00b_node_get_loc_str(sym2->ct->declaration_node));
        return false;

    case N00B_SK_VARIABLE:
        if (!n00b_type_is_error(sym1->type)) {
            type_cmp_exact_match(ctx1, sym1, sym2);
            // TODO: we are not doing anything with this right now.
            if (!sym2->ct->type_decl_node) {
                if (n00b_type_is_declared(sym2)) {
                    sym2->ct->type_decl_node = sym2->ct->declaration_node;
                }
                else {
                    if (n00b_type_is_declared(sym1)) {
                        sym2->ct->type_decl_node = sym1->ct->declaration_node;
                    }
                }
            }
        }
        return true;
    default:
        // For instance, we never call this on scopes
        // that hold N00B_SK_MODULE symbols or N00B_SK_FORMAL symbols.
        n00b_unreachable();
    }
}

#define one_scope_lookup(scopevar, name)                          \
    if (scopevar != NULL) {                                       \
        result = hatrack_dict_get(scopevar->symbols, name, NULL); \
        if (result) {                                             \
            return result;                                        \
        }                                                         \
    }

n00b_symbol_t *
n00b_symbol_lookup(n00b_scope_t *local_scope,
                   n00b_scope_t *module_scope,
                   n00b_scope_t *global_scope,
                   n00b_scope_t *attr_scope,
                   n00b_utf8_t  *name)
{
    n00b_symbol_t *result = NULL;

    one_scope_lookup(local_scope, name);
    one_scope_lookup(module_scope, name);
    one_scope_lookup(global_scope, name);
    one_scope_lookup(attr_scope, name);

    return NULL;
}

n00b_grid_t *
n00b_format_scope(n00b_scope_t *scope)
{
    uint64_t              len = 0;
    hatrack_dict_value_t *values;
    n00b_grid_t          *grid       = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_cols",
                                         n00b_ka(6),
                                         "header_rows",
                                         n00b_ka(1),
                                         "stripe",
                                         n00b_ka(true)));
    n00b_list_t          *row        = n00b_new_table_row();
    n00b_utf8_t          *decl_const = n00b_new_utf8("declared");
    n00b_utf8_t          *inf_const  = n00b_new_utf8("inferred");
    n00b_dict_t          *memos      = n00b_dict(n00b_type_typespec(),
                                   n00b_type_utf8());
    int64_t               nexttid    = 0;
    n00b_utf8_t          *snap       = n00b_new_utf8("snap");
    n00b_utf8_t          *full_snap  = n00b_new_utf8("full_snap");

    if (scope != NULL) {
        values = hatrack_dict_values_sort(scope->symbols,
                                          &len);
    }

    if (len == 0) {
        grid = n00b_new(n00b_type_grid(), n00b_kw("start_cols", n00b_ka(1)));
        n00b_list_append(row, n00b_new_utf8("Scope is empty"));
        n00b_grid_add_row(grid, row);
        n00b_set_column_style(grid, 0, full_snap);
        return grid;
    }

    n00b_list_append(row, n00b_new_utf8("Name"));
    n00b_list_append(row, n00b_new_utf8("Kind"));
    n00b_list_append(row, n00b_new_utf8("Type"));
    n00b_list_append(row, n00b_new_utf8("Offset"));
    n00b_list_append(row, n00b_new_utf8("Defs"));
    n00b_list_append(row, n00b_new_utf8("Uses"));
    n00b_grid_add_row(grid, row);

    for (uint64_t i = 0; i < len; i++) {
        n00b_utf8_t   *kind;
        n00b_symbol_t *entry = values[i];

        kind = inf_const;

        if (n00b_type_is_declared(entry) || entry->kind == N00B_SK_EXTERN_FUNC) {
            kind = decl_const;
        }
        row = n00b_new_table_row();

        n00b_list_append(row, entry->name);

        if (entry->kind == N00B_SK_VARIABLE) {
            if (entry->flags & N00B_F_DECLARED_CONST) {
                n00b_list_append(row, n00b_new_utf8("const"));
            }
            else {
                if (entry->flags & N00B_F_USER_IMMUTIBLE) {
                    n00b_list_append(row, n00b_new_utf8("loop var"));
                }
                else {
                    n00b_list_append(row,
                                     n00b_new_utf8(
                                         n00b_symbol_kind_names[entry->kind]));
                }
            }
        }
        else {
            n00b_list_append(row,
                             n00b_new_utf8(n00b_symbol_kind_names[entry->kind]));
        }

        n00b_type_t *symtype = n00b_get_sym_type(entry);
        symtype              = n00b_type_resolve(symtype);

        if (n00b_type_is_box(symtype)) {
            symtype = n00b_type_unbox(symtype);
        }

        n00b_list_append(row,
                         n00b_cstr_format("[em]{} [/][i]({})",
                                          n00b_internal_type_repr(symtype,
                                                                  memos,
                                                                  &nexttid),
                                          kind));
        n00b_list_append(row,
                         n00b_cstr_format("{:x}",
                                          n00b_box_u64(entry->static_offset)));

        int          n = n00b_list_len(entry->ct->sym_defs);
        n00b_utf8_t *def_text;

        if (n == 0) {
            def_text = n00b_cstr_format("[gray]none[/]");
        }
        else {
            n00b_list_t *defs = n00b_new(n00b_type_list(n00b_type_utf8()));
            for (int i = 0; i < n; i++) {
                n00b_tree_node_t *t = n00b_list_get(entry->ct->sym_defs, i, NULL);
                if (t == NULL) {
                    n00b_list_append(defs, n00b_new_utf8("??"));
                    continue;
                }
                n00b_list_append(defs, n00b_node_get_loc_str(t));
            }
            def_text = n00b_str_join(defs, n00b_new_utf8(", "));
        }

        if (n00b_sym_is_declared_const(entry)) {
            if (entry->value == NULL) {
                def_text = n00b_str_concat(n00b_cstr_format("[i]value:[red] not set[/]\n"),
                                           def_text);
            }
            else {
                def_text = n00b_str_concat(n00b_cstr_format("[i]value:[jazzberry] {}[/]\n",
                                                            n00b_value_obj_repr(entry->value)),
                                           def_text);
            }
        }

        n00b_list_append(row, def_text);

        n = n00b_list_len(entry->ct->sym_uses);
        n00b_utf8_t *use_text;

        if (n == 0) {
            use_text = n00b_cstr_format("[gray]none[/]");
        }
        else {
            n00b_list_t *uses = n00b_new(n00b_type_list(n00b_type_utf8()));
            for (int i = 0; i < n; i++) {
                n00b_tree_node_t *t = n00b_list_get(entry->ct->sym_uses, i, NULL);
                if (t == NULL) {
                    n00b_list_append(uses, n00b_new_utf8("??"));
                    continue;
                }
                n00b_list_append(uses, n00b_node_get_loc_str(t));
            }
            use_text = n00b_str_join(uses, n00b_new_utf8(", "));
        }
        n00b_list_append(row, use_text);
        n00b_grid_add_row(grid, row);
    }

    n00b_set_column_style(grid, 0, snap);
    n00b_set_column_style(grid, 1, snap);
    n00b_set_column_style(grid, 2, snap);
    n00b_set_column_style(grid, 3, snap);

    return grid;
}

n00b_scope_t *
n00b_scope_copy(n00b_scope_t *s)
{
    // We just need a shallow copy.
    n00b_scope_t *result = n00b_gc_alloc_mapped(n00b_scope_t, n00b_scope_gc_bits);
    result->parent       = s->parent;
    result->symbols      = n00b_shallow(s->symbols);
    result->kind         = s->kind;

    return result;
}
