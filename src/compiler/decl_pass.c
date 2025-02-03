#define N00B_USE_INTERNAL_API
#include "n00b.h"

// First pass of the raw parse tree.

// Not everything we need would be resolvable in one pass, especially
// symbols in other modules.
//
// Since declarations can be in any statement, we do go ahead and
// finish walking down to the expression level, and do initial work on
// literal extraction too.

typedef struct n00b_pass1_ctx {
    n00b_tree_node_t *cur_tnode;
    n00b_pnode_t     *cur;
    n00b_spec_t      *spec;
    n00b_compile_ctx *cctx;
    n00b_module_t    *module_ctx;
    n00b_scope_t     *static_scope;
    n00b_list_t      *extern_decls;
    bool              in_func;
} n00b_pass1_ctx;

static inline n00b_tree_node_t *
n00b_get_match(n00b_pass1_ctx *ctx, n00b_tpat_node_t *pattern)
{
    return n00b_get_match_on_node(ctx->cur_tnode, pattern);
}

static inline void
n00b_set_current_node(n00b_pass1_ctx *ctx, n00b_tree_node_t *n)
{
    ctx->cur_tnode = n;
    ctx->cur       = n00b_tree_get_contents(n);
}

static inline bool
n00b_node_down(n00b_pass1_ctx *ctx, int i)
{
    n00b_tree_node_t *n = ctx->cur_tnode;

    if (i >= n->num_kids) {
        return false;
    }

    if (n->children[i]->parent != n) {
        n00b_print_parse_node(n->children[i]);
    }
    n00b_assert(n->children[i]->parent == n);
    n00b_set_current_node(ctx, n->children[i]);

    return true;
}

static inline void
n00b_node_up(n00b_pass1_ctx *ctx)
{
    n00b_set_current_node(ctx, ctx->cur_tnode->parent);
}

static inline n00b_node_kind_t
n00b_cur_node_type(n00b_pass1_ctx *ctx)
{
    return ctx->cur->kind;
}

static inline n00b_tree_node_t *
n00b_cur_node(n00b_pass1_ctx *ctx)
{
    return ctx->cur_tnode;
}

static void pass_dispatch(n00b_pass1_ctx *ctx);

void
n00b_module_param_gc_bits(uint64_t *bitmap, n00b_module_param_info_t *pi)
{
    n00b_mark_raw_to_addr(bitmap, pi, &pi->default_value);
}

n00b_module_param_info_t *
n00b_new_module_param(void)
{
    return n00b_gc_alloc_mapped(n00b_module_param_info_t,
                                n00b_module_param_gc_bits);
}

void
n00b_sig_info_gc_bits(uint64_t *bitmap, n00b_sig_info_t *si)
{
    n00b_mark_raw_to_addr(bitmap, si, &si->return_info.type);
}

n00b_sig_info_t *
n00b_new_sig_info(void)
{
    return n00b_gc_alloc_mapped(n00b_sig_info_t, n00b_sig_info_gc_bits);
}

void
n00b_fn_decl_gc_bits(uint64_t *bitmap, n00b_fn_decl_t *decl)
{
    n00b_mark_raw_to_addr(bitmap, decl, &decl->cfg);
}

n00b_fn_decl_t *
n00b_new_fn_decl(void)
{
    return n00b_gc_alloc_mapped(n00b_fn_decl_t, n00b_fn_decl_gc_bits);
}

n00b_ffi_decl_t *
n00b_new_ffi_decl(void)
{
    return n00b_gc_alloc_mapped(n00b_ffi_decl_t, N00B_GC_SCAN_ALL);
}

static inline void
process_children(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *n = ctx->cur_tnode;

    for (int i = 0; i < n->num_kids; i++) {
        n00b_node_down(ctx, i);
        pass_dispatch(ctx);
        n00b_node_up(ctx);
    }

    if (n->num_kids == 1) {
        n00b_pnode_t *pparent = n00b_get_pnode(n);
        n00b_pnode_t *pkid    = n00b_get_pnode(n->children[0]);
        pparent->value        = pkid->value;
    }
}

static bool
obj_type_check(n00b_pass1_ctx *ctx, const n00b_obj_t *obj, n00b_type_t *t2)
{
    int  warn;
    bool res = n00b_obj_type_check(obj, t2, &warn);

    return res;
}

static inline n00b_symbol_t *
declare_sym(n00b_pass1_ctx   *ctx,
            n00b_scope_t     *scope,
            n00b_utf8_t      *name,
            n00b_tree_node_t *node,
            n00b_symbol_kind  kind,
            bool             *success,
            bool              err_if_present)
{
    n00b_symbol_t *result = n00b_declare_symbol(ctx->module_ctx,
                                                scope,
                                                name,
                                                node,
                                                kind,
                                                success,
                                                err_if_present);

    n00b_shadow_check(ctx->module_ctx, result, scope);

    result->flags |= N00B_F_IS_DECLARED;

    return result;
}

static void
validate_str_enum_vals(n00b_pass1_ctx *ctx, n00b_list_t *items)
{
    n00b_set_t *set = n00b_new(n00b_type_set(n00b_type_utf8()));
    int64_t     n   = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t *tnode = n00b_list_get(items, i, NULL);
        n00b_pnode_t     *pnode = n00b_get_pnode(tnode);

        if (n00b_tree_get_number_children(tnode) == 0) {
            pnode->value = (void *)n00b_new_utf8("error");
            continue;
        }

        n00b_symbol_t *sym = pnode->extra_info;
        n00b_utf8_t   *val = (n00b_utf8_t *)pnode->value;
        sym->value         = val;

        if (!n00b_set_add(set, val)) {
            n00b_add_error(ctx->module_ctx, n00b_err_dupe_enum, tnode);
            return;
        }
    }
}

static n00b_type_t *
validate_int_enum_vals(n00b_pass1_ctx *ctx, n00b_list_t *items)
{
    n00b_set_t  *set           = n00b_new(n00b_type_set(n00b_type_u64()));
    int64_t      n             = n00b_list_len(items);
    int          bits          = 0;
    bool         neg           = false;
    uint64_t     next_implicit = 0;
    n00b_type_t *result;

    // First, extract numbers from set values.
    for (int i = 0; i < n; i++) {
        n00b_tree_node_t *tnode = n00b_list_get(items, i, NULL);
        if (n00b_tree_get_number_children(tnode) == 0) {
            continue;
        }

        n00b_pnode_t   *pnode  = n00b_get_pnode(tnode);
        n00b_obj_t     *ref    = pnode->value;
        n00b_type_t    *ty     = n00b_get_my_type(ref);
        n00b_dt_info_t *dtinfo = n00b_type_get_data_type_info(ty);
        int             sz;
        uint64_t        val;

        switch (dtinfo->alloc_len) {
        case 1:
            val = (uint64_t) * (uint8_t *)ref;
            break;
        case 2:
            val = (uint64_t) * (uint16_t *)ref;
            break;
        case 4:
            val = (uint64_t) * (uint32_t *)ref;
            break;
        case 8:
            val = *(uint64_t *)ref;
            break;
        default:
            N00B_CRAISE("Invalid int size for enum item");
        }

        sz = 64 - __builtin_clzll(val);

        switch (sz) {
        case 64:
            if (dtinfo->typeid == N00B_T_INT) {
                neg = true;
            }
            break;

        case 32:
            if (dtinfo->typeid == N00B_T_I32) {
                neg = true;
            }
            break;
        case 8:
            if (dtinfo->typeid == N00B_T_I8) {
                neg = true;
            }
            break;

        default:
            break;
        }

        if (sz > bits) {
            bits = sz;
        }

        if (!n00b_set_add(set, (void *)val)) {
            n00b_add_error(ctx->module_ctx, n00b_err_dupe_enum, tnode);
        }
    }

    if (bits > 32) {
        bits = 64;
    }
    else {
        if (bits <= 8) {
            bits = 8;
        }
        else {
            bits = 32;
        }
    }

    switch (bits) {
    case 8:
        result = neg ? n00b_type_i8() : n00b_type_u8();
        break;
    case 32:
        result = neg ? n00b_type_i32() : n00b_type_u32();
        break;
    default:
        result = neg ? n00b_type_i64() : n00b_type_u64();
        break;
    }

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t *tnode = n00b_list_get(items, i, NULL);
        n00b_pnode_t     *pnode = n00b_get_pnode(tnode);

        if (n00b_tree_get_number_children(tnode) != 0) {
            pnode->value       = n00b_coerce_object(pnode->value, result);
            n00b_symbol_t *sym = pnode->extra_info;
            sym->value         = pnode->value;
            continue;
        }

        while (n00b_set_contains(set, (void *)next_implicit)) {
            next_implicit++;
        }

        pnode->value       = n00b_coerce_object(n00b_box_u64(next_implicit++),
                                          result);
        n00b_symbol_t *sym = pnode->extra_info;
        sym->value         = pnode->value;
    }

    return result;
}

// For now, enum types are either going to be integer types, or they're
// going to be string types.
//
// Once we add UDTs, it will be possible to make them propert UDTs,
// so that we can do proper value checking.

static n00b_list_t *
extract_enum_items(n00b_pass1_ctx *ctx)
{
    n00b_list_t      *result = n00b_list(n00b_type_ref());
    n00b_tree_node_t *node   = ctx->cur_tnode;
    int               len    = node->num_kids;

    for (int i = 0; i < len; i++) {
        n00b_pnode_t *kid = n00b_get_pnode(node->children[i]);

        if (kid->kind == n00b_nt_enum_item) {
            n00b_list_append(result, node->children[i]);
        }
    }

    return result;
}

static void
handle_enum_decl(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *item;
    n00b_tree_node_t *tnode  = n00b_get_match(ctx, n00b_first_kid_id);
    n00b_pnode_t     *id     = n00b_get_pnode(tnode);
    n00b_symbol_t    *idsym  = NULL;
    n00b_list_t      *items  = extract_enum_items(ctx);
    int               n      = n00b_list_len(items);
    bool              is_str = false;
    n00b_scope_t     *scope;
    n00b_utf8_t      *varname;
    n00b_type_t      *inferred_type;

    if (n00b_cur_node_type(ctx) == n00b_nt_global_enum) {
        scope = ctx->module_ctx->ct->global_scope;
    }
    else {
        scope = ctx->module_ctx->module_scope;
    }

    inferred_type = n00b_new_typevar();

    if (id != NULL) {
        idsym = declare_sym(ctx,
                            scope,
                            n00b_identifier_text(id->token),
                            tnode,
                            N00B_SK_ENUM_TYPE,
                            NULL,
                            true);

        idsym->type = inferred_type;
    }

    for (int i = 0; i < n; i++) {
        n00b_pnode_t *pnode;

        item    = n00b_list_get(items, i, NULL);
        varname = n00b_node_text(item);
        pnode   = n00b_get_pnode(item);

        if (n00b_node_num_kids(item) != 0) {
            pnode->value = n00b_node_simp_literal(n00b_tree_get_child(item, 0));

            if (!n00b_obj_is_int_type(pnode->value)) {
                if (!obj_type_check(ctx, pnode->value, n00b_type_utf8())) {
                    n00b_add_error(ctx->module_ctx,
                                   n00b_err_invalid_enum_lit_type,
                                   item);
                    return;
                }
                if (i == 0) {
                    is_str = true;
                }
                else {
                    if (!is_str) {
                        n00b_add_error(ctx->module_ctx,
                                       n00b_err_enum_str_int_mix,
                                       item);
                        return;
                    }
                }
            }
            else {
                if (is_str) {
                    n00b_add_error(ctx->module_ctx,
                                   n00b_err_enum_str_int_mix,
                                   item);
                    return;
                }
            }
        }
        else {
            if (is_str) {
                n00b_add_error(ctx->module_ctx,
                               n00b_err_omit_string_enum_value,
                               item);
                return;
            }
        }

        n00b_symbol_t *item_sym = declare_sym(ctx,
                                              scope,
                                              varname,
                                              item,
                                              N00B_SK_ENUM_VAL,
                                              NULL,
                                              true);

        item_sym->flags |= N00B_F_DECLARED_CONST;

        if (idsym) {
            item_sym->type = idsym->type;
        }
        else {
            item_sym->type = inferred_type;
        }
        pnode->extra_info = item_sym;
    }

    if (is_str) {
        validate_str_enum_vals(ctx, items);
        n00b_merge_types(inferred_type, n00b_type_utf8(), NULL);
    }
    else {
        n00b_type_t *ty = validate_int_enum_vals(ctx, items);
        int          warn;

        if (n00b_type_is_error(n00b_merge_types(inferred_type, ty, &warn))) {
            n00b_add_error(ctx->module_ctx,
                           n00b_err_inconsistent_type,
                           n00b_cur_node(ctx),
                           inferred_type,
                           ty);
        }
    }
}

static void
handle_var_decl(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *n         = n00b_cur_node(ctx);
    n00b_list_t      *quals     = n00b_apply_pattern_on_node(n,
                                                    n00b_qualifier_extract);
    bool              is_const  = false;
    bool              is_let    = false;
    bool              is_global = false;
    n00b_scope_t     *scope;

    for (int i = 0; i < n00b_list_len(quals); i++) {
        n00b_utf8_t *qual = n00b_node_text(n00b_list_get(quals, i, NULL));
        switch (qual->data[0]) {
        case 'g':
            is_global = true;
            continue;
        case 'c':
            is_const = true;
            continue;
        case 'l':
            is_let = true;
        default:
            continue;
        }
    }

    scope = ctx->static_scope;

    if (is_global) {
        while (scope->parent != NULL) {
            scope = scope->parent;
        }
    }

    n00b_list_t *syms = n00b_apply_pattern_on_node(n, n00b_sym_decls);

    for (int i = 0; i < n00b_list_len(syms); i++) {
        n00b_tree_node_t *one_set   = n00b_list_get(syms, i, NULL);
        n00b_list_t      *var_names = n00b_apply_pattern_on_node(one_set,
                                                            n00b_sym_names);
        n00b_tree_node_t *type_node = n00b_get_match_on_node(one_set, n00b_sym_type);
        n00b_tree_node_t *init      = n00b_get_match_on_node(one_set, n00b_sym_init);
        n00b_type_t      *type      = NULL;

        if (type_node != NULL) {
            type = n00b_node_to_type(ctx->module_ctx, type_node, NULL);
        }
        else {
            type = n00b_new_typevar();
        }

        for (int j = 0; j < n00b_list_len(var_names); j++) {
            n00b_tree_node_t *name_node = n00b_list_get(var_names, j, NULL);
            n00b_utf8_t      *name      = n00b_node_text(name_node);
            n00b_symbol_t    *sym       = declare_sym(ctx,
                                             scope,
                                             name,
                                             name_node,
                                             N00B_SK_VARIABLE,
                                             NULL,
                                             true);
            sym->type                   = type;

            if (sym != NULL) {
                if (ctx->in_func && !is_global) {
                    sym->flags |= N00B_F_STACK_STORAGE | N00B_F_FUNCTION_SCOPE;
                }

                n00b_pnode_t *pn = n00b_get_pnode(name_node);
                pn->value        = (void *)sym;

                if (is_const) {
                    sym->flags |= N00B_F_DECLARED_CONST;
                }
                if (is_let) {
                    sym->flags |= N00B_F_DECLARED_LET;
                }

                if (init != NULL && j + 1 == n00b_list_len(var_names)) {
                    n00b_set_current_node(ctx, init);
                    process_children(ctx);
                    n00b_set_current_node(ctx, n);

                    sym->flags |= N00B_F_HAS_INITIALIZER;
                    sym->ct->value_node = init;

                    n00b_pnode_t *initpn = n00b_get_pnode(init);

                    if (initpn->value == NULL) {
                        return;
                    }

                    n00b_type_t *inf_type = n00b_get_my_type(initpn->value);

                    sym->value = initpn->value;

                    sym->flags |= N00B_F_TYPE_IS_DECLARED;
                    int warning = 0;

                    if (!n00b_types_are_compat(inf_type, type, &warning)) {
                        n00b_add_error(ctx->module_ctx,
                                       n00b_err_inconsistent_type,
                                       name_node,
                                       inf_type,
                                       type,
                                       n00b_node_get_loc_str(type_node));
                    }
                }
                else {
                    if (type) {
                        sym->flags |= N00B_F_TYPE_IS_DECLARED;
                    }
                }
            }
        }
    }
}

static void
handle_param_block(n00b_pass1_ctx *ctx)
{
    // Reminder to self: make sure to check for not const in the decl.
    // That really needs to happen at the end of the pass through :)
    n00b_module_param_info_t *prop      = n00b_new_module_param();
    n00b_tree_node_t         *root      = n00b_cur_node(ctx);
    n00b_pnode_t             *pnode     = n00b_get_pnode(root);
    n00b_tree_node_t         *name_node = n00b_tree_get_child(root, 0);
    n00b_pnode_t             *name_pn   = n00b_get_pnode(name_node);
    n00b_utf8_t              *sym_name  = n00b_node_text(name_node);
    n00b_utf8_t              *dot       = n00b_new_utf8(".");
    int                       nkids     = n00b_tree_get_number_children(root);
    n00b_symbol_t            *sym;
    bool                      attr;

    if (pnode->short_doc) {
        prop->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            prop->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    if (name_pn->kind == n00b_nt_member) {
        attr            = true;
        int num_members = n00b_tree_get_number_children(name_node);

        for (int i = 1; i < num_members; i++) {
            sym_name = n00b_str_concat(sym_name, dot);
            sym_name = n00b_str_concat(sym_name,
                                       n00b_node_text(
                                           n00b_tree_get_child(name_node, i)));
        }
        sym_name = n00b_to_utf8(sym_name);
    }
    else {
        attr = false;
    }

    for (int i = 1; i < nkids; i++) {
        n00b_tree_node_t *prop_node = n00b_tree_get_child(root, i);
        n00b_utf8_t      *prop_name = n00b_node_text(prop_node);
        n00b_obj_t        lit       = n00b_tree_get_child(prop_node, 0);

        switch (prop_name->data[0]) {
        case 'v':
            prop->validator = n00b_node_to_callback(ctx->module_ctx, lit);
            if (!prop->validator) {
                n00b_add_error(ctx->module_ctx,
                               n00b_err_spec_callback_required,
                               prop_node);
            }
            else {
                n00b_list_append(ctx->module_ctx->ct->callback_lits,
                                 prop->validator);
            }

            break;
        case 'c':
            prop->callback = n00b_node_to_callback(ctx->module_ctx, lit);
            if (!prop->callback) {
                n00b_add_error(ctx->module_ctx,
                               n00b_err_spec_callback_required,
                               prop_node);
            }
            else {
                n00b_list_append(ctx->module_ctx->ct->callback_lits,
                                 prop->callback);
            }
            break;
        case 'd':
            prop->default_value = lit;
            break;
        default:
            n00b_unreachable();
        }
    }

    if (!hatrack_dict_add(ctx->module_ctx->parameters, sym_name, prop)) {
        n00b_add_error(ctx->module_ctx, n00b_err_dupe_param, name_node);
        return;
    }

    if (attr) {
        sym = n00b_lookup_symbol(ctx->module_ctx->ct->attribute_scope, sym_name);
        if (sym) {
            if (n00b_sym_is_declared_const(sym)) {
                n00b_add_error(ctx->module_ctx, n00b_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->module_ctx->ct->attribute_scope,
                              sym_name,
                              name_node,
                              N00B_SK_ATTR,
                              NULL,
                              false);
        }
    }
    else {
        sym = n00b_lookup_symbol(ctx->module_ctx->module_scope, sym_name);
        if (sym) {
            if (n00b_sym_is_declared_const(sym)) {
                n00b_add_error(ctx->module_ctx, n00b_err_const_param, name_node);
            }
        }
        else {
            sym = declare_sym(ctx,
                              ctx->module_ctx->module_scope,
                              sym_name,
                              name_node,
                              N00B_SK_VARIABLE,
                              NULL,
                              false);
        }
    }

    prop->linked_symbol = sym;
    pnode->extra_info   = prop;
}

static void
one_section_prop(n00b_pass1_ctx      *ctx,
                 n00b_spec_section_t *section,
                 n00b_tree_node_t    *n)
{
    bool        *value;
    n00b_obj_t   callback;
    n00b_utf8_t *prop = n00b_node_text(n);

    switch (prop->data[0]) {
    case 'u': // user_def_ok
        value = n00b_node_simp_literal(n00b_tree_get_child(n, 0));

        if (!value || !obj_type_check(ctx, (n00b_obj_t)value, n00b_type_bool())) {
            n00b_add_error(ctx->module_ctx,
                           n00b_err_spec_bool_required,
                           n00b_tree_get_child(n, 0));
        }
        else {
            if (*value) {
                section->user_def_ok = 1;
            }
        }
        break;
    case 'h': // hidden
        value = n00b_node_simp_literal(n00b_tree_get_child(n, 0));
        if (!value || !obj_type_check(ctx, (n00b_obj_t)value, n00b_type_bool())) {
            n00b_add_error(ctx->module_ctx,
                           n00b_err_spec_bool_required,
                           n00b_tree_get_child(n, 0));
        }
        else {
            if (*value) {
                section->hidden = 1;
            }
        }
        break;
    case 'v': // validator
        callback = n00b_node_to_callback(ctx->module_ctx,
                                         n00b_tree_get_child(n, 0));

        if (!callback) {
            n00b_add_error(ctx->module_ctx,
                           n00b_err_spec_callback_required,
                           n00b_tree_get_child(n, 0));
        }
        else {
            section->validator = callback;
            n00b_list_append(ctx->module_ctx->ct->callback_lits, callback);
        }
        break;
    case 'r': // require
        for (int i = 0; i < n00b_tree_get_number_children(n); i++) {
            n00b_utf8_t *name = n00b_node_text(n00b_tree_get_child(n, i));
            if (!n00b_set_add(section->required_sections, name)) {
                n00b_add_warning(ctx->module_ctx,
                                 n00b_warn_dupe_require,
                                 n00b_tree_get_child(n, i));
            }
            if (n00b_set_contains(section->allowed_sections, name)) {
                n00b_add_warning(ctx->module_ctx,
                                 n00b_warn_require_allow,
                                 n00b_tree_get_child(n, i));
            }
        }
        break;
    default: // allow
        for (int i = 0; i < n00b_tree_get_number_children(n); i++) {
            n00b_utf8_t *name = n00b_node_text(n00b_tree_get_child(n, i));
            if (!n00b_set_add(section->allowed_sections, name)) {
                n00b_add_warning(ctx->module_ctx,
                                 n00b_warn_dupe_allow,
                                 n00b_tree_get_child(n, i));
            }
            if (n00b_set_contains(section->required_sections, name)) {
                n00b_add_warning(ctx->module_ctx,
                                 n00b_warn_require_allow,
                                 n00b_tree_get_child(n, i));
            }
        }
    }
}

static void
one_field(n00b_pass1_ctx      *ctx,
          n00b_spec_section_t *section,
          n00b_tree_node_t    *tnode)
{
    n00b_spec_field_t *f        = n00b_new_spec_field();
    n00b_utf8_t       *name     = n00b_node_text(n00b_tree_get_child(tnode, 0));
    n00b_pnode_t      *pnode    = n00b_get_pnode(tnode);
    int                num_kids = n00b_tree_get_number_children(tnode);
    bool              *value;
    n00b_obj_t         callback;

    f->exclusions       = n00b_new(n00b_type_set(n00b_type_utf8()));
    f->name             = name;
    f->declaration_node = tnode;
    f->location_string  = n00b_format_module_location(ctx->module_ctx,
                                                     pnode->token);
    pnode->extra_info   = f;

    if (pnode->short_doc) {
        section->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    for (int i = 1; i < num_kids; i++) {
        n00b_tree_node_t *kid  = n00b_tree_get_child(tnode, i);
        n00b_utf8_t      *prop = n00b_node_text(kid);
        switch (prop->data[0]) {
        case 'c': // choice:
                  // For now, we just stash the raw nodes, and
                  // evaluate it later.
            f->stashed_options = n00b_tree_get_child(kid, 0);
            f->validate_choice = true;
            break;

        case 'd': // default:
                  // Same.
            f->default_value    = n00b_tree_get_child(kid, 0);
            f->default_provided = true;
            break;

        case 'h': // hidden
            value = n00b_node_simp_literal(n00b_tree_get_child(kid, 0));
            // clang-format off
            if (!value ||
		!obj_type_check(ctx, (n00b_obj_t)value, n00b_type_bool())) {
                n00b_add_error(ctx->module_ctx,
                              n00b_err_spec_bool_required,
                              n00b_tree_get_child(kid, 0));
                // clang-format on
            }
            else {
                if (*value) {
                    f->hidden = true;
                }
            }
            break;

        case 'l': // lock
            value = n00b_node_simp_literal(n00b_tree_get_child(kid, 0));
            // clang-format off
            if (!value ||
		!obj_type_check(ctx, (n00b_obj_t)value, n00b_type_bool())) {
                // clang-format on
                n00b_add_error(ctx->module_ctx,
                               n00b_err_spec_bool_required,
                               n00b_tree_get_child(kid, 0));
            }
            else {
                if (*value) {
                    f->lock_on_write = 1;
                }
            }
            break;

        case 'e': // exclusions
            for (int i = 0; i < n00b_tree_get_number_children(kid); i++) {
                n00b_utf8_t *name = n00b_node_text(n00b_tree_get_child(kid, i));

                if (!n00b_set_add(f->exclusions, name)) {
                    n00b_add_warning(ctx->module_ctx,
                                     n00b_warn_dupe_exclusion,
                                     n00b_tree_get_child(kid, i));
                }
            }
            break;

        case 't': // type
            if (n00b_node_has_type(n00b_tree_get_child(kid, 0), n00b_nt_identifier)) {
                f->tinfo.type_pointer = n00b_node_text(n00b_tree_get_child(kid, 0));
            }
            else {
                f->tinfo.type = n00b_node_to_type(ctx->module_ctx,
                                                  n00b_tree_get_child(kid, 0),
                                                  NULL);
            }
            break;

        case 'v': // validator
            callback = n00b_node_to_callback(ctx->module_ctx,
                                             n00b_tree_get_child(kid, 0));

            if (!callback) {
                n00b_add_error(ctx->module_ctx,
                               n00b_err_spec_callback_required,
                               n00b_tree_get_child(kid, 0));
            }
            else {
                f->validator = callback;
                n00b_list_append(ctx->module_ctx->ct->callback_lits, callback);
            }
            break;
        default:
            if (!strcmp(prop->data, "range")) {
                f->stashed_options = n00b_tree_get_child(kid, 0);
                f->validate_range  = true;
            }
            else {
                // required.
                value = n00b_node_simp_literal(n00b_tree_get_child(kid, 0));
                // clang-format off
                if (!value ||
		    !obj_type_check(ctx, (n00b_obj_t)value, n00b_type_bool())) {
                    n00b_add_error(ctx->module_ctx,
                                  n00b_err_spec_bool_required,
                                  n00b_tree_get_child(kid, 0));
                    // clang-format on
                }
                else {
                    if (*value) {
                        f->required = true;
                    }
                }
                break;
            }
        }
    }

    if (!hatrack_dict_add(section->fields, name, f)) {
        n00b_add_error(ctx->module_ctx, n00b_err_dupe_spec_field, tnode);
    }
}

static void
handle_section_spec(n00b_pass1_ctx *ctx)
{
    n00b_spec_t         *spec     = ctx->spec;
    n00b_spec_section_t *section  = n00b_new_spec_section();
    n00b_tree_node_t    *tnode    = n00b_cur_node(ctx);
    n00b_pnode_t        *pnode    = n00b_get_pnode(tnode);
    int                  ix       = 2;
    int                  num_kids = n00b_tree_get_number_children(tnode);

    section->declaration_node = tnode;
    section->location_string  = n00b_format_module_location(ctx->module_ctx,
                                                           pnode->token);

    if (pnode->short_doc) {
        section->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            section->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    n00b_utf8_t *kind = n00b_node_text(n00b_tree_get_child(tnode, 0));
    switch (kind->data[0]) {
    case 's': // singleton
        section->singleton = 1;
        // fallthrough
    case 'n': // named
        section->name = n00b_node_text(n00b_tree_get_child(tnode, 1));
        break;
    default: // root
        ix = 1;
        break;
    }

    for (; ix < num_kids; ix++) {
        n00b_tree_node_t *tkid = n00b_tree_get_child(tnode, ix);
        n00b_pnode_t     *pkid = n00b_get_pnode(tkid);

        if (pkid->kind == n00b_nt_section_prop) {
            one_section_prop(ctx, section, tkid);
        }
        else {
            one_field(ctx, section, tkid);
        }
    }

    if (section->name == NULL) {
        if (spec->in_use) {
            n00b_add_error(ctx->module_ctx,
                           n00b_err_dupe_root_section,
                           tnode);
        }
        else {
            spec->root_section = section;
        }
    }
    else {
        if (!hatrack_dict_add(spec->section_specs, section->name, section)) {
            n00b_add_error(ctx->module_ctx, n00b_err_dupe_section, tnode);
        }
    }
}

static void
handle_config_spec(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *tnode = n00b_cur_node(ctx);
    n00b_pnode_t     *pnode = n00b_get_pnode(tnode);

    if (ctx->module_ctx->ct->local_specs == NULL) {
        ctx->module_ctx->ct->local_specs = n00b_new_spec();
    }
    else {
        n00b_add_error(ctx->module_ctx, n00b_err_dupe_section, tnode);
        return;
    }

    ctx->spec                   = ctx->module_ctx->ct->local_specs;
    ctx->spec->declaration_node = tnode;

    if (pnode->short_doc) {
        ctx->spec->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            ctx->spec->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    process_children(ctx);
}

static n00b_sig_info_t *
new_sig_info(int num_params)
{
    n00b_sig_info_t *result = n00b_new_sig_info();
    result->num_params      = num_params;

    if (result->num_params > 0) {
        result->param_info = n00b_gc_array_alloc(n00b_fn_param_info_t,
                                                 num_params);
    }

    return result;
}

static n00b_sig_info_t *
extract_fn_sig_info(n00b_pass1_ctx   *ctx,
                    n00b_tree_node_t *tree)
{
    n00b_list_t     *decls     = n00b_apply_pattern_on_node(tree,
                                                    n00b_param_extraction);
    n00b_dict_t     *type_ctx  = n00b_dict(n00b_type_utf8(), n00b_type_ref());
    int              ndecls    = n00b_list_len(decls);
    int              nparams   = 0;
    int              cur_param = 0;
    n00b_list_t     *ptypes    = n00b_new(n00b_type_list(n00b_type_typespec()));
    n00b_sig_info_t *info;

    // Allocate space for parameters by counting how many variable
    // names we find.

    for (int i = 0; i < ndecls; i++) {
        n00b_tree_node_t *node  = n00b_list_get(decls, i, NULL);
        int               kidct = n00b_tree_get_number_children(node);

        if (kidct > 1) {
            n00b_tree_node_t *kid   = n00b_tree_get_child(node, kidct - 1);
            n00b_pnode_t     *pnode = n00b_get_pnode(kid);

            // Skip type specs.
            if (pnode->kind != n00b_nt_identifier) {
                kidct--;
            }
        }
        nparams += kidct;
    }

    info           = new_sig_info(nparams);
    info->fn_scope = n00b_new_scope(ctx->module_ctx->module_scope,
                                    N00B_SCOPE_FUNC);
    info->formals  = n00b_new_scope(ctx->module_ctx->module_scope,
                                   N00B_SCOPE_FORMALS);

    // Now, we loop through the parameter trees again. In function
    // declarations, named variables with omitted types are given a
    // type variable as a return type. Similarly, omitted return
    // values get a type variable.

    for (int i = 0; i < ndecls; i++) {
        n00b_tree_node_t *node     = n00b_list_get(decls, i, NULL);
        int               kidct    = n00b_tree_get_number_children(node);
        n00b_type_t      *type     = NULL;
        bool              got_type = false;

        if (kidct > 1) {
            n00b_tree_node_t *kid   = n00b_tree_get_child(node, kidct - 1);
            n00b_pnode_t     *pnode = n00b_get_pnode(kid);

            if (pnode->kind != n00b_nt_identifier) {
                type = n00b_node_to_type(ctx->module_ctx, kid, type_ctx);
                kidct--;
                got_type = true;
            }
        }

        // All but the last one in a subtree get type variables.
        for (int j = 0; j < kidct - 1; j++) {
            n00b_fn_param_info_t *pi  = &info->param_info[cur_param++];
            n00b_tree_node_t     *kid = n00b_tree_get_child(node, j);

            pi->name = n00b_node_text(kid);
            pi->type = n00b_new_typevar();

            declare_sym(ctx,
                        info->formals,
                        pi->name,
                        kid,
                        N00B_SK_FORMAL,
                        NULL,
                        true);

            // Redeclare in the 'actual' scope. If there's a declared
            // type this won't get it; it will be fully inferred, and
            // then we'll compare against the declared type at the
            // end.
            declare_sym(ctx,
                        info->fn_scope,
                        pi->name,
                        kid,
                        N00B_SK_VARIABLE,
                        NULL,
                        true);

            n00b_list_append(ptypes, pi->type);
        }

        // last item.
        if (!type) {
            type = n00b_new_typevar();
        }

        n00b_fn_param_info_t *pi  = &info->param_info[cur_param++];
        n00b_tree_node_t     *kid = n00b_tree_get_child(node, kidct - 1);
        pi->name                  = n00b_node_text(kid);
        pi->type                  = type;

        n00b_symbol_t *formal = declare_sym(ctx,
                                            info->formals,
                                            pi->name,
                                            kid,
                                            N00B_SK_FORMAL,
                                            NULL,
                                            true);

        formal->type = type;
        if (got_type) {
            formal->flags |= N00B_F_TYPE_IS_DECLARED;
        }

        declare_sym(ctx,
                    info->fn_scope,
                    pi->name,
                    kid,
                    N00B_SK_VARIABLE,
                    NULL,
                    true);

        n00b_list_append(ptypes, pi->type ? pi->type : n00b_new_typevar());
    }

    n00b_tree_node_t *retnode = n00b_get_match_on_node(tree, n00b_return_extract);

    n00b_symbol_t *formal = declare_sym(ctx,
                                        info->formals,
                                        n00b_new_utf8("$result"),
                                        retnode,
                                        N00B_SK_FORMAL,
                                        NULL,
                                        true);
    if (retnode) {
        info->return_info.type = n00b_node_to_type(ctx->module_ctx,
                                                   retnode,
                                                   type_ctx);
        formal->type           = info->return_info.type;
        formal->flags |= N00B_F_TYPE_IS_DECLARED | N00B_F_REGISTER_STORAGE;
    }
    else {
        formal->type = n00b_new_typevar();
    }

    n00b_symbol_t *actual = declare_sym(ctx,
                                        info->fn_scope,
                                        n00b_new_utf8("$result"),
                                        ctx->cur_tnode,
                                        N00B_SK_VARIABLE,
                                        NULL,
                                        true);

    actual->flags = formal->flags;

    // Now fill out the 'local_type' field of the ffi decl.
    // TODO: support varargs.
    //
    // Note that this will get replaced once we do some checking.

    n00b_type_t *ret_for_sig = info->return_info.type;

    if (!ret_for_sig) {
        ret_for_sig = n00b_new_typevar();
    }

    info->full_type = n00b_type_fn(ret_for_sig, ptypes, false);
    return info;
}

static n00b_list_t *
n00b_get_func_mods(n00b_tree_node_t *tnode)
{
    n00b_list_t *result = n00b_list(n00b_type_tree(n00b_type_parse_node()));

    for (int i = 0; i < tnode->num_kids; i++) {
        n00b_list_append(result, tnode->children[i]);
    }

    return result;
}

static void
handle_func_decl(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *tnode = n00b_cur_node(ctx);
    n00b_fn_decl_t   *decl  = n00b_new_fn_decl();
    n00b_utf8_t      *name  = n00b_node_text(tnode->children[1]);
    n00b_list_t      *mods  = n00b_get_func_mods(tnode->children[0]);
    int               nmods = n00b_list_len(mods);
    n00b_symbol_t    *sym;

    decl->signature_info = extract_fn_sig_info(ctx, n00b_cur_node(ctx));

    for (int i = 0; i < nmods; i++) {
        n00b_tree_node_t *mod_node = n00b_list_get(mods, i, NULL);
        switch (n00b_node_text(mod_node)->data[0]) {
        case 'p':
            decl->private = 1;
            break;
        default:
            decl->once = 1;
            break;
        }
    }

    sym = declare_sym(ctx,
                      ctx->module_ctx->module_scope,
                      name,
                      tnode,
                      N00B_SK_FUNC,
                      NULL,
                      true);

    if (sym) {
        sym->type  = decl->signature_info->full_type;
        sym->value = (void *)decl;
    }

    ctx->static_scope = decl->signature_info->fn_scope;

    n00b_pnode_t *pnode = n00b_get_pnode(tnode);
    pnode->value        = (n00b_obj_t)sym;

    ctx->in_func = true;
    n00b_node_down(ctx, tnode->num_kids - 1);
    process_children(ctx);
    n00b_node_up(ctx);
    ctx->in_func = false;
}

static void
handle_extern_block(n00b_pass1_ctx *ctx)
{
    n00b_ffi_decl_t  *info          = n00b_new_ffi_decl();
    n00b_utf8_t      *external_name = n00b_node_text(n00b_get_match(ctx,
                                                               n00b_first_kid_id));
    n00b_tree_node_t *ext_ret       = n00b_get_match(ctx, n00b_extern_return);
    n00b_tree_node_t *cur           = n00b_cur_node(ctx);
    n00b_tree_node_t *ext_pure      = n00b_get_match(ctx, n00b_find_pure);
    n00b_tree_node_t *ext_holds     = n00b_get_match(ctx, n00b_find_holds);
    n00b_tree_node_t *ext_allocs    = n00b_get_match(ctx, n00b_find_allocation_records);
    n00b_tree_node_t *csig          = n00b_cur_node(ctx)->children[1];
    n00b_tree_node_t *ext_lsig      = n00b_get_match(ctx, n00b_find_extern_local);
    n00b_tree_node_t *ext_box       = n00b_get_match(ctx, n00b_find_extern_box);
    n00b_pnode_t     *pnode         = n00b_get_pnode(cur);

    if (pnode->short_doc) {
        info->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            info->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    // Since we don't have a tree search primitive to collect
    // all nodes of a type yet, we do this.
    for (int i = 2; i < cur->num_kids; i++) {
        n00b_pnode_t *kid = n00b_get_pnode(cur->children[i]);

        if (kid->kind == n00b_nt_extern_dll) {
            if (info->dll_list == NULL) {
                info->dll_list = n00b_list(n00b_type_utf8());
            }
            n00b_utf8_t *s = n00b_node_text(cur->children[i]->children[0]);
            n00b_list_append(info->dll_list, s);
        }
    }

    int64_t n            = csig->num_kids - 1;
    info->num_ext_params = n;
    info->external_name  = external_name;

    if (n) {
        info->external_params = n00b_gc_array_alloc(uint8_t, n);

        for (int64_t i = 0; i < n; i++) {
            n00b_tree_node_t *tnode = csig->children[i];
            n00b_pnode_t     *pnode = n00b_tree_get_contents(tnode);
            uint64_t          val   = (uint64_t)pnode->extra_info;

            info->external_params[i] = (uint8_t)val;
        }
    }

    if (ext_ret) {
        n00b_pnode_t *pnode = n00b_get_pnode(ext_ret);
        uint64_t      val   = (uint64_t)pnode->extra_info;

        info->external_return_type = (uint8_t)val;
    }

    // BREAK HERE.
    info->local_params = extract_fn_sig_info(ctx, ext_lsig);

    if (n00b_type_is_void(
            n00b_type_get_last_param(info->local_params->full_type))) {
        info->local_params->void_return = 1;
    }

    if (ext_pure) {
        bool *pure_ptr = n00b_node_simp_literal(n00b_tree_get_child(ext_pure, 0));

        if (pure_ptr && *pure_ptr) {
            info->local_params->pure = 1;
        }
    }

    info->skip_boxes = false;

    if (ext_box) {
        bool *box_ptr = n00b_node_simp_literal(n00b_tree_get_child(ext_box, 0));

        if (box_ptr && !*box_ptr) {
            info->skip_boxes = true;
        }
    }

    info->local_name = n00b_node_text(n00b_get_match_on_node(ext_lsig,
                                                             n00b_first_kid_id));

    if (ext_holds) {
        if (info->local_params == NULL) {
            n00b_add_error(ctx->module_ctx, n00b_err_no_params_to_hold, ext_holds);
            return;
        }

        uint64_t         bitfield  = 0;
        n00b_sig_info_t *si        = info->local_params;
        int              num_holds = n00b_tree_get_number_children(ext_holds);

        for (int i = 0; i < num_holds; i++) {
            n00b_tree_node_t *kid = n00b_tree_get_child(ext_holds, i);
            n00b_utf8_t      *txt = n00b_node_text(kid);

            for (int j = 0; j < si->num_params; j++) {
                n00b_fn_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_holds = 1;
                if (j < 64) {
                    uint64_t flag = (uint64_t)(1 << j);
                    if (bitfield & flag) {
                        n00b_add_warning(ctx->module_ctx, n00b_warn_dupe_hold, kid);
                    }
                    bitfield |= flag;
                }
                goto next_i;
            }
            n00b_add_error(ctx->module_ctx, n00b_err_bad_hold_name, kid);
            break;
next_i:
    /* nothing. */;
        }
        info->cif.hold_info = bitfield;
    }

    if (ext_allocs) {
        uint64_t         bitfield   = 0;
        bool             got_ret    = false;
        n00b_sig_info_t *si         = info->local_params;
        int              num_allocs = n00b_tree_get_number_children(ext_allocs);

        for (int i = 0; i < num_allocs; i++) {
            n00b_tree_node_t *kid = n00b_tree_get_child(ext_allocs, i);
            n00b_utf8_t      *txt = n00b_node_text(kid);

            if (!strcmp(txt->data, "return")) {
                if (got_ret) {
                    n00b_add_warning(ctx->module_ctx, n00b_warn_dupe_alloc, kid);
                    continue;
                }
                si->return_info.ffi_allocs = 1;
                continue;
            }

            for (int j = 0; j < si->num_params; j++) {
                n00b_fn_param_info_t *param = &si->param_info[j];
                if (strcmp(txt->data, param->name->data)) {
                    continue;
                }
                param->ffi_allocs = 1;
                if (j < 63) {
                    uint64_t flag = (uint64_t)(1 << j);
                    if (bitfield & flag) {
                        n00b_add_warning(ctx->module_ctx,
                                         n00b_warn_dupe_alloc,
                                         kid);
                    }
                    bitfield |= flag;
                }
                goto next_alloc;
            }
            n00b_add_error(ctx->module_ctx, n00b_err_bad_alloc_name, kid);
            break;
next_alloc:
    /* nothing. */;
        }
        info->cif.alloc_info = bitfield;
    }

    n00b_symbol_t *sym = declare_sym(ctx,
                                     ctx->module_ctx->module_scope,
                                     info->local_name,
                                     n00b_get_match(ctx, n00b_first_kid_id),
                                     N00B_SK_EXTERN_FUNC,
                                     NULL,
                                     true);

    if (sym) {
        sym->type  = info->local_params->full_type;
        sym->value = (void *)info;
    }

    n00b_list_append(ctx->module_ctx->extern_decls, sym);
}

static n00b_list_t *
get_member_prefix(n00b_tree_node_t *n)
{
    n00b_list_t *result = n00b_list(n00b_type_tree(n00b_type_parse_node()));

    for (int i = 0; i < n->num_kids - 1; i++) {
        n00b_list_append(result, n->children[i]);
    }

    return result;
}

static void
handle_use_stmt(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *unode   = n00b_get_match(ctx, n00b_use_uri);
    n00b_tree_node_t *modnode = n00b_get_match(ctx, n00b_member_last);
    n00b_list_t      *prefix  = get_member_prefix(ctx->cur_tnode->children[0]);
    bool              status  = false;
    n00b_utf8_t      *modname = n00b_node_text(modnode);
    n00b_utf8_t      *package = NULL;
    n00b_utf8_t      *uri     = NULL;
    n00b_pnode_t     *pnode   = n00b_get_pnode(ctx->cur_tnode);
    n00b_module_t    *mi;

    if (n00b_list_len(prefix) != 0) {
        package = n00b_node_list_join(prefix, n00b_utf32_repeat('.', 1), false);
    }

    if (unode) {
        uri = n00b_node_simp_literal(unode);
    }

    mi = n00b_find_module(ctx->cctx,
                          uri,
                          modname,
                          package,
                          ctx->module_ctx->package,
                          ctx->module_ctx->path,
                          NULL);

    pnode->value = (void *)mi;

    if (!mi) {
        if (package != NULL) {
            modname = n00b_cstr_format("{}.{}", package, modname);
        }

        n00b_add_error(ctx->module_ctx,
                       n00b_err_search_path,
                       ctx->cur_tnode,
                       modname);
        return;
    }

    n00b_add_module_to_worklist(ctx->cctx, mi);

    n00b_symbol_t *sym = declare_sym(ctx,
                                     ctx->module_ctx->ct->imports,
                                     n00b_module_fully_qualified(mi),
                                     n00b_cur_node(ctx),
                                     N00B_SK_MODULE,
                                     &status,
                                     false);

    if (!status) {
        n00b_add_info(ctx->module_ctx,
                      n00b_info_dupe_import,
                      n00b_cur_node(ctx));
    }
    else {
        sym->value = mi;
    }
}

static void
look_for_dead_code(n00b_pass1_ctx *ctx)
{
    n00b_tree_node_t *cur    = n00b_cur_node(ctx);
    n00b_tree_node_t *parent = cur->parent;

    if (parent->num_kids > 1) {
        if (parent->children[parent->num_kids - 1] != cur) {
            n00b_add_warning(ctx->module_ctx, n00b_warn_dead_code, cur);
        }
    }
}

static void
pass_dispatch(n00b_pass1_ctx *ctx)
{
    n00b_scope_t *saved_scope;
    n00b_pnode_t *pnode = n00b_get_pnode(n00b_cur_node(ctx));
    pnode->static_scope = ctx->static_scope;

    switch (n00b_cur_node_type(ctx)) {
    case n00b_nt_global_enum:
    case n00b_nt_enum:
        handle_enum_decl(ctx);
        break;

    case n00b_nt_func_def:
        saved_scope         = ctx->static_scope;
        pnode->static_scope = ctx->static_scope;
        handle_func_decl(ctx);
        ctx->static_scope = saved_scope;
        break;

    case n00b_nt_variable_decls:
        handle_var_decl(ctx);
        break;

    case n00b_nt_config_spec:
        handle_config_spec(ctx);
        break;

    case n00b_nt_section_spec:
        handle_section_spec(ctx);
        break;

    case n00b_nt_param_block:
        handle_param_block(ctx);
        break;

    case n00b_nt_extern_block:
        handle_extern_block(ctx);
        break;

    case n00b_nt_use:
        handle_use_stmt(ctx);
        break;

    case n00b_nt_break:
    case n00b_nt_continue:
    case n00b_nt_return:
        look_for_dead_code(ctx);
        process_children(ctx);
        break;
    default:
        process_children(ctx);
        break;
    }
}

static void
find_dependencies(n00b_compile_ctx *cctx, n00b_module_t *module_ctx)
{
    n00b_scope_t         *imports = module_ctx->ct->imports;
    uint64_t              len     = 0;
    hatrack_dict_value_t *values  = hatrack_dict_values(imports->symbols,
                                                       &len);

    for (uint64_t i = 0; i < len; i++) {
        n00b_symbol_t    *sym = values[i];
        n00b_module_t    *mi  = sym->value;
        n00b_tree_node_t *n   = sym->ct->declaration_node;
        n00b_pnode_t     *pn  = n00b_get_pnode(n);

        pn->value = (n00b_obj_t)mi;

        if (n00b_set_contains(cctx->processed, mi)) {
            continue;
        }
    }
}

void
n00b_module_decl_pass(n00b_compile_ctx *cctx, n00b_module_t *module_ctx)
{
    if (n00b_fatal_error_in_module(module_ctx)) {
        return;
    }

    if (module_ctx->ct->status >= n00b_compile_status_code_loaded) {
        return;
    }

    if (module_ctx->ct->status != n00b_compile_status_code_parsed) {
        N00B_CRAISE("Cannot extract declarations for code that is not parsed.");
    }

    n00b_setup_treematch_patterns();

    n00b_pass1_ctx ctx = {
        .module_ctx = module_ctx,
        .cctx       = cctx,
    };

    n00b_set_current_node(&ctx, module_ctx->ct->parse_tree);

    module_ctx->ct->global_scope = n00b_new_scope(NULL, N00B_SCOPE_GLOBAL);
    module_ctx->module_scope     = n00b_new_scope(module_ctx->ct->global_scope,
                                              N00B_SCOPE_MODULE);

    module_ctx->ct->attribute_scope = n00b_new_scope(NULL, N00B_SCOPE_ATTRIBUTES);
    module_ctx->ct->imports         = n00b_new_scope(NULL, N00B_SCOPE_IMPORTS);

    module_ctx->parameters        = n00b_new(n00b_type_dict(n00b_type_utf8(),
                                                     n00b_type_ref()));
    module_ctx->fn_def_syms       = n00b_new(n00b_type_list(n00b_type_ref()));
    module_ctx->ct->callback_lits = n00b_new(n00b_type_list(n00b_type_ref()));
    module_ctx->extern_decls      = n00b_new(n00b_type_list(n00b_type_ref()));

    ctx.cur->static_scope = module_ctx->module_scope;
    ctx.static_scope      = module_ctx->module_scope;

    n00b_pnode_t *pnode = n00b_get_pnode(module_ctx->ct->parse_tree);

    if (pnode->short_doc) {
        module_ctx->short_doc = n00b_token_raw_content(pnode->short_doc);

        if (pnode->long_doc) {
            module_ctx->long_doc = n00b_token_raw_content(pnode->long_doc);
        }
    }

    pass_dispatch(&ctx);
    find_dependencies(cctx, module_ctx);
    if (module_ctx->ct->fatal_errors) {
        cctx->fatality = true;
    }

    n00b_module_set_status(module_ctx, n00b_compile_status_code_loaded);

    return;
}
