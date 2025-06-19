#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    n00b_dict_t            *base_du_info;
    n00b_cfg_branch_info_t *branches;
} n00b_branch_ctx;

typedef struct {
    n00b_module_t *module_ctx;
    n00b_dict_t   *du_info;
    n00b_list_t   *sometimes_info;
} cfg_ctx;

void
n00b_cfg_status_gc_bits(uint64_t *bitmap, n00b_cfg_status_t *cfgstatus)
{
    n00b_mark_raw_to_addr(bitmap, cfgstatus, &cfgstatus->last_use);
}

static n00b_cfg_status_t *
n00b_new_cfg_status()
{
    return n00b_gc_alloc_mapped(n00b_cfg_status_t, n00b_cfg_status_gc_bits);
}

static n00b_symbol_t *
follow_sym_links(n00b_symbol_t *sym)
{
    n00b_assert(sym != NULL);
    while (sym->linked_symbol != NULL) {
        sym = sym->linked_symbol;
    }

    return sym;
}

static bool
sym_cmp(n00b_symbol_t *sym1, n00b_symbol_t *sym2)
{
    return (!strcmp(sym1->name->data, sym2->name->data));
}

static bool
cfg_propogate_def(cfg_ctx         *ctx,
                  n00b_symbol_t   *sym,
                  n00b_cfg_node_t *n,
                  n00b_list_t     *deps)
{
    n00b_dict_t *du_info;

    if (n == NULL) {
        du_info = ctx->du_info;
    }
    else {
        du_info = n->liveness_info;
    }
    // TODO, make use of the data flow information. For now, we're just going
    // to drop it on the floor. Note that this

    sym = follow_sym_links(sym);

    n00b_cfg_status_t *new = n00b_new_cfg_status();
    n00b_cfg_status_t *old = n00b_dict_get(du_info, sym, NULL);

    if (old) {
        new->last_use = old->last_use;
    }

    new->last_def = n;

    n00b_dict_put(du_info, sym, new);

    return old != NULL;
}

static bool
cfg_propogate_use(cfg_ctx         *ctx,
                  n00b_symbol_t   *sym,
                  n00b_cfg_node_t *n)
{
    n00b_dict_t *du_info;

    if (n == NULL) {
        du_info = ctx->du_info;
    }
    else {
        du_info = n->liveness_info;
    }

    sym = follow_sym_links(sym);

    n00b_cfg_status_t *new = n00b_new_cfg_status();

    n00b_list_t *view = n00b_dict_items(du_info);
    uint64_t     x    = n00b_list_len(view);
    new->last_def     = NULL;

    for (unsigned int i = 0; i < x; i++) {
        n00b_tuple_t    *t   = n00b_list_get(view, i, NULL);
        n00b_symbol_t   *s   = n00b_tuple_get(t, 0);
        n00b_cfg_node_t *cfg = n00b_tuple_get(t, 1);

        if (!strcmp(s->name->data, sym->name->data)) {
            new->last_def = cfg;
            break;
        }
    }

    if (!new->last_def) {
        n->use_without_def = 1;
    }

    new->last_use = n;

    n00b_dict_put(du_info, sym, new);

    return new->last_def != NULL;
}

static void
cfg_copy_du_info(cfg_ctx         *ctx,
                 n00b_cfg_node_t *node,
                 n00b_dict_t    **new_dict,
                 n00b_list_t    **new_sometimes)
{
    n00b_dict_t *copy = n00b_dict(n00b_type_ref(), n00b_type_ref());
    n00b_list_t *view = n00b_dict_items(node->liveness_info);
    uint64_t     n    = n00b_list_len(view);
    n00b_dict_t *d    = n00b_dict(n00b_type_string(), n00b_type_int());

    for (uint64_t i = 0; i < n; i++) {
        n00b_tuple_t  *tup = n00b_list_get(view, i, NULL);
        n00b_symbol_t *sym = n00b_tuple_get(tup, 0);

        if (sym->ct->cfg_kill_node && sym->ct->cfg_kill_node == node) {
            continue;
        }

        if (n00b_dict_add(d, sym->name, 0)) {
            n00b_dict_put(copy, sym, n00b_tuple_get(tup, 1));
        }
    }

    *new_dict = copy;

    n00b_list_t *old = node->sometimes_live;

    if (old != NULL) {
        *new_sometimes = n00b_new(n00b_type_list(n00b_type_ref()));
        int l          = n00b_list_len(old);
        for (int i = 0; i < l; i++) {
            n00b_list_add_if_unique(*new_sometimes,
                                    n00b_list_get(old, i, NULL),
                                    (bool (*)(void *, void *))sym_cmp);
        }
    }
}

static n00b_string_t *result_text = NULL;

static void
check_for_fn_exit_errors(n00b_module_t *file, n00b_fn_decl_t *fn_decl)
{
    if (result_text == NULL) {
        result_text = n00b_cstring("$result");
        n00b_gc_register_root(&result_text, 1);
    }

    if (fn_decl->signature_info->return_info.type->typeid == N00B_T_VOID) {
        return;
    }

    n00b_scope_t      *fn_scope  = fn_decl->signature_info->fn_scope;
    n00b_symbol_t     *ressym    = n00b_symbol_lookup(fn_scope,
                                               NULL,
                                               NULL,
                                               NULL,
                                               result_text);
    n00b_cfg_node_t   *node      = fn_decl->cfg;
    n00b_cfg_node_t   *exit_node = node->contents.block_entrance.exit_node;
    n00b_cfg_status_t *status    = n00b_dict_get(exit_node->liveness_info,
                                              ressym,
                                              NULL);

    // the result symbol is in the ending live set, so we're done.

    if (status != NULL) {
        return;
    }

    // If nothing is returned at all ever, we already handled that.
    // Only look for when we didn't see it in all paths.
#if 0
    if (n00b_list_len(ressym->sym_defs) > 0) {
        n00b_add_error(file,
                      n00b_cfg_return_coverage,
                      fn_decl->cfg->reference_location);
    }
#endif
}

static void
check_for_module_exit_errors(cfg_ctx *ctx, n00b_cfg_node_t *node)
{
    // TODO: info msg when variables are defined in the module that are:
    // 1. Global; or
    // 2. Used in a function
    // But are not defined by the end of the top-level, taking into
    // account functions the toplevel calls w/in the module.
}

static void
check_block_for_errors(cfg_ctx *ctx, n00b_cfg_node_t *node)
{
    //  Search the graph again for use/before def errors, and give
    //  appropriate error messages, based on my 'sometimes_live'
    //  field.

    if (node == NULL) {
        return;
    }

    node->reached = 1;

    switch (node->kind) {
    case n00b_cfg_block_entrance:
        check_block_for_errors(ctx, node->contents.block_entrance.next_node);
        if (!node->contents.block_entrance.exit_node->reached) {
            check_block_for_errors(ctx,
                                   node->contents.block_entrance.exit_node);
        }
        return;
    case n00b_cfg_block_exit:
        check_block_for_errors(ctx, node->contents.block_exit.next_node);
        return;
    case n00b_cfg_node_branch:
        for (int i = 0; i < node->contents.branches.num_branches; i++) {
            n00b_cfg_node_t *n = node->contents.branches.branch_targets[i];
            check_block_for_errors(ctx, n);
        }
        return;
    case n00b_cfg_def:
        check_block_for_errors(ctx, node->contents.flow.next_node);
        return;
    case n00b_cfg_call:
    case n00b_cfg_use:

        if (node->use_without_def) {
            n00b_symbol_t       *sym;
            bool                 sometimes = false;
            n00b_symbol_t       *check;
            n00b_compile_error_t err;

            sym = node->contents.flow.dst_symbol;

            if (!(sym->flags & N00B_F_USE_ERROR)) {
                int n = n00b_list_len(node->sometimes_live);
                for (int i = 0; i < n; i++) {
                    check = n00b_list_get(node->sometimes_live, i, NULL);
                    if (check == sym) {
                        sometimes = true;
                        break;
                    }
                }

                if (sometimes) {
                    err = n00b_cfg_use_possible_def;
                }
                else {
                    err = n00b_cfg_use_no_def;
                }
                n00b_add_error(ctx->module_ctx,
                               err,
                               node->reference_location,
                               sym->name);
            }
        }

        check_block_for_errors(ctx, node->contents.flow.next_node);
        return;
    case n00b_cfg_jump:
        return;
    }
}

typedef union {
    struct {
        uint16_t count;
        uint16_t uses;
        uint16_t defs;
    } counters;
    void *ptr;
} cfg_merge_ct_t;

// Aux entries come to us through a continue, break or return that
// is NESTED INSIDE OF US. Any data flows on those branches
// didn't propogate down to any previous join branch. So the
// entry def/use info doesn't need to be modified, but the exit
// info might need to be.
//
// What this means really, is:
//
// 1) If we're at a block entrance, we will stick anything not in our
//    exit d/u set to the 'sometimes' list.
//
// 2) For block exits, we'll have to do a proper merge; symbols only
//    should end up in the d/u list if they exist in both entries.

static void
cfg_merge_aux_entries_to_top(cfg_ctx *ctx, n00b_cfg_node_t *node)
{
    n00b_list_t     *inbounds = node->contents.block_entrance.inbound_links;
    n00b_cfg_node_t *exit     = node->contents.block_entrance.exit_node;
    n00b_dict_t     *exit_du  = exit->liveness_info;
    n00b_symbol_t   *sym;

    if (inbounds == NULL) {
        return;
    }
    int num_inbounds = n00b_list_len(inbounds);

    if (num_inbounds == 0) {
        return;
    }

    n00b_set_t *sometimes = n00b_set(n00b_type_string());

    if (exit->sometimes_live != NULL) {
        int n = n00b_list_len(exit->sometimes_live);

        // Any existing sometimes items are always outbound sometimes
        // items.
        for (int i = 0; i < n; i++) {
            sym = n00b_list_get(exit->sometimes_live, i, NULL);

            if (n00b_dict_get(exit_du, sym, NULL) == NULL) {
                n00b_set_add(sometimes, sym);
            }
        }
    }

    for (int i = 0; i < num_inbounds; i++) {
        n00b_cfg_node_t *one = n00b_list_get(inbounds, i, NULL);

        if (one->liveness_info == NULL) {
            continue;
        }
        n00b_list_t *view   = n00b_dict_keys(one->liveness_info);
        uint64_t     nitems = n00b_list_len(view);

        for (unsigned int j = 0; j < nitems; j++) {
            sym = n00b_list_get(view, j, NULL);

            // Dead branch.
            if (exit_du == NULL) {
                continue;
            }

            if (n00b_dict_get(exit_du, sym, NULL) == NULL) {
                n00b_set_add(sometimes, sym);
            }
        }

        if (one->sometimes_live != NULL) {
            int n = n00b_list_len(one->sometimes_live);

            for (int j = 0; j < n; j++) {
                sym = n00b_list_get(one->sometimes_live, i, NULL);

                // If it's in the exit set, it's not a 'sometimes'
                // for the block.
                if (n00b_dict_get(exit_du, sym, NULL) == NULL) {
                    n00b_set_add(sometimes, sym);
                }
            }
        }
    }

    exit->sometimes_live = n00b_list(n00b_type_ref());

    n00b_list_t *not_unique = n00b_set_items(sometimes);

    for (int i = 0; i < n00b_list_len(not_unique); i++) {
        void *item = n00b_list_get(not_unique, i, NULL);
        n00b_list_add_if_unique(exit->sometimes_live,
                                item,
                                (bool (*)(void *, void *))sym_cmp);
    }
}

static void
process_branch_exit(cfg_ctx *ctx, n00b_cfg_node_t *node)
{
    // Merge and push forward info on partial crapola.
    n00b_dict_t            *counters  = n00b_new(n00b_type_dict(n00b_type_ref(),
                                                    n00b_type_ref()));
    n00b_set_t             *sometimes = n00b_new(n00b_type_set(n00b_type_ref()));
    n00b_cfg_branch_info_t *bi        = &node->contents.branches;
    n00b_dict_t            *merged    = n00b_new(n00b_type_dict(n00b_type_ref(),
                                                  n00b_type_ref()));
    n00b_cfg_node_t        *exit_node;
    n00b_symbol_t          *sym;
    n00b_list_t            *view;
    n00b_cfg_status_t      *status;
    n00b_cfg_status_t      *old_status;
    cfg_merge_ct_t          count_info;
    uint64_t                len;

    for (int i = 0; i < bi->num_branches; i++) {
        exit_node = bi->branch_targets[i]->contents.block_entrance.exit_node;

        n00b_dict_t *duinfo = exit_node->liveness_info;
        n00b_list_t *stinfo = exit_node->sometimes_live;

        // TODO: fix this.
        if (duinfo == NULL) {
            continue;
        }

        view = n00b_dict_items(duinfo);
        len  = n00b_list_len(view);

        for (unsigned int j = 0; j < len; j++) {
            n00b_tuple_t *tup = n00b_list_get(view, j, NULL);
            sym               = n00b_tuple_get(tup, 0);
            status            = n00b_tuple_get(tup, 1);
            old_status        = n00b_dict_get(node->liveness_info, sym, NULL);
            count_info.ptr    = n00b_dict_get(counters, sym, NULL);

            count_info.counters.count++;
            if (old_status == NULL) {
                if (status->last_use != NULL) {
                    count_info.counters.uses++;
                }
                if (status->last_def != NULL) {
                    count_info.counters.defs++;
                }
            }
            else {
                if (status->last_use != old_status->last_use) {
                    count_info.counters.uses++;
                }
                if (status->last_def != old_status->last_def) {
                    count_info.counters.defs++;
                }
            }

            n00b_dict_put(counters, sym, count_info.ptr);
        }

        // If it's not always live in a subblock, it's not always live
        // in the full block.
        if (stinfo != NULL) {
            len = n00b_list_len(stinfo);

            for (unsigned int j = 0; j < len; j++) {
                sym = n00b_list_get(stinfo, j, NULL);
                n00b_set_add(sometimes, sym);
            }
        }
    }

    view = n00b_dict_items(counters);
    len  = n00b_list_len(view);

    for (unsigned int i = 0; i < len; i++) {
        n00b_tuple_t *tup = n00b_list_get(view, i, NULL);
        sym               = n00b_tuple_get(tup, 0);
        count_info.ptr    = n00b_tuple_get(tup, 1);

        // Symbol didn't show up in every branch, so it goes on the
        // 'sometimes' list.
        if (count_info.counters.count < bi->num_branches) {
            n00b_set_add(sometimes, sym);
            continue;
        }

        status     = n00b_new_cfg_status();
        old_status = n00b_dict_get(node->liveness_info, sym, NULL);
        if (old_status != NULL) {
            status->last_def = old_status->last_def;
            status->last_use = old_status->last_use;
        }
        if (count_info.counters.uses == bi->num_branches) {
            status->last_use = node;
        }
        if (count_info.counters.defs == bi->num_branches) {
            status->last_def = node;
        }

        n00b_dict_put(merged, sym, status);
    }

    // Okay, we've done all the merging, now we have to propogate the
    // results to the exit node for the whole branching structure.
    node->liveness_info  = merged;
    node->sometimes_live = n00b_list(n00b_type_ref());

    n00b_list_t *not_unique = n00b_set_items(sometimes);

    for (int i = 0; i < n00b_list_len(not_unique); i++) {
        void *item = n00b_list_get(not_unique, i, NULL);
        n00b_list_add_if_unique(node->sometimes_live,
                                item,
                                (bool (*)(void *, void *))sym_cmp);
    }
}

static void
set_starting_du_info(cfg_ctx *ctx, n00b_cfg_node_t *n, n00b_cfg_node_t *parent)
{
    if (!parent) {
        if (n->liveness_info != NULL) {
            return;
        }
        n->liveness_info          = ctx->du_info;
        n->starting_liveness_info = ctx->du_info;
        n->sometimes_live         = ctx->sometimes_info;
        n->starting_sometimes     = ctx->sometimes_info;
        return;
    }
    else {
        cfg_copy_du_info(ctx,
                         parent,
                         &n->starting_liveness_info,
                         &n->starting_sometimes);
        cfg_copy_du_info(ctx,
                         parent,
                         &n->liveness_info,
                         &n->sometimes_live);
    }
}

static n00b_cfg_node_t *
cfg_process_node(cfg_ctx *ctx, n00b_cfg_node_t *node, n00b_cfg_node_t *parent)
{
    n00b_cfg_node_t             *next;
    n00b_cfg_block_enter_info_t *enter_info;
    n00b_branch_ctx              branch_info;

    if (node == NULL) {
        return NULL;
    }

    node->reached = 1;

    set_starting_du_info(ctx, node, parent);
    switch (node->kind) {
    case n00b_cfg_block_entrance:
        enter_info = &node->contents.block_entrance;

        // Anything defined coming into the block will be defined
        // coming out, so we can copy those into the exit node as
        // 'sure things'. Similarly, anything that happens before the
        // first branch in this block is a sure thing, so we are going
        // to copy the state now, and use the same reference in the
        // entrance and the exit node, so that any d/u info before the
        // branch automatically propogates.
        //
        // Anything that is changes in a branch will, if the branch
        // returns to an entrance or exit node, be pushed into the
        // `to_merge` field. When we're done with a block, we do the
        // merging, which accomplishes a few things:

        // 1. It determines what gets set in EVERY branch, and is
        //    guaranteed to be defined underneath.
        // 2. It allows us to determine what symbols MIGHT be undefined
        //    below this block, depending on the branch, so that we can
        //    give a clear enough error message (as opposed to insisting
        //    on a definitive use-before-def).
        // 3. It allows us to do the same WITHIN our block, when we
        //    can re-enter from the top via continues.
        //
        // Because of #3, we don't want to emit errors for use-before-def
        // until AFTER processing the block. We just mark nodes that
        // have errors, and make a second pass through the block once
        // we can give the right error message.

        n00b_cfg_node_t *ret = cfg_process_node(ctx,
                                                enter_info->next_node,
                                                node);

        if (ret) {
            cfg_copy_du_info(ctx,
                             ret,
                             &node->liveness_info,
                             &node->sometimes_live);
        }
        if (!enter_info->exit_node->reached) {
            ret = cfg_process_node(ctx, enter_info->exit_node, node);
            cfg_copy_du_info(ctx,
                             ret,
                             &node->liveness_info,
                             &node->sometimes_live);
        }
        else {
            // Clear the flag for the error collection pass.
            enter_info->exit_node->reached = 0;
        }

        cfg_merge_aux_entries_to_top(ctx, node);

        return ret;

    case n00b_cfg_block_exit:

        next = node->contents.block_exit.next_node;

        if (!next) {
            return node;
        }

        return cfg_process_node(ctx, next, node);

    case n00b_cfg_node_branch:

        branch_info.base_du_info = node->liveness_info;
        branch_info.branches     = &node->contents.branches;

        for (int i = 0; i < branch_info.branches->num_branches; i++) {
            set_starting_du_info(ctx,
                                 branch_info.branches->branch_targets[i],
                                 node);
            n00b_cfg_node_t *one = cfg_process_node(
                ctx,
                branch_info.branches->branch_targets[i],
                NULL);

            if (one && one->kind == n00b_cfg_block_exit) {
                n00b_cfg_node_t *exit = branch_info.branches->exit_node;
                n00b_list_append(exit->contents.block_exit.inbound_links, one);
            }
        }

        process_branch_exit(ctx, node);

        return cfg_process_node(ctx, branch_info.branches->exit_node, node);

    case n00b_cfg_use:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        cfg_propogate_use(ctx, node->contents.flow.dst_symbol, node);
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;

    case n00b_cfg_def:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);
        cfg_propogate_def(ctx,
                          node->contents.flow.dst_symbol,
                          node,
                          node->contents.flow.deps);
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;

    case n00b_cfg_call:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        for (int i = 0; i < n00b_list_len(node->contents.flow.deps); i++) {
            n00b_symbol_t *sym = n00b_list_get(node->contents.flow.deps,
                                               i,
                                               NULL);
            cfg_propogate_use(ctx, sym, node);
        }
        cfg_process_node(ctx, node->contents.flow.next_node, node);
        return node;
    case n00b_cfg_jump:
        cfg_copy_du_info(ctx,
                         node->parent,
                         &node->liveness_info,
                         &node->sometimes_live);

        n00b_cfg_node_t *ta = node->contents.jump.target;
        n00b_list_t     *v  = n00b_dict_keys(node->liveness_info);
        uint64_t         n  = n00b_list_len(v);

        if (!ta) {
            return NULL;
        }
        if (!ta->sometimes_live) {
            ta->sometimes_live = n00b_new(n00b_type_list(n00b_type_ref()));
        }

        for (uint64_t i = 0; i < n; i++) {
            n00b_list_add_if_unique(ta->sometimes_live,
                                    n00b_list_get(v, i, NULL),
                                    (bool (*)(void *, void *))sym_cmp);
        }

        n00b_list_t *old = node->sometimes_live;
        for (int64_t i = 0; i < n00b_list_len(old); i++) {
            n00b_list_add_if_unique(ta->sometimes_live,
                                    n00b_list_get(old, i, NULL),
                                    (bool (*)(void *, void *))sym_cmp);
        }

        return NULL;
    }
    n00b_unreachable();
}

// The input will be the module, plus any d/u info that we inherit, which
// includes attributes that are set during the initial import of any
// previous modules to have been analyzed.
//
// We must first analyze the module entry, then any defined functions,
// and then we return def/use info for the lop-level module eval, to pass
// to the next one.
//
// Inbound calls here must provided a mutable du_info dict.
//
//
void
n00b_cfg_analyze(n00b_module_t *module_ctx, n00b_dict_t *du_info)
{
    if (du_info == NULL) {
        du_info = n00b_new(n00b_type_dict(n00b_type_ref(), n00b_type_ref()));
    }

    cfg_ctx ctx = {
        .module_ctx     = module_ctx,
        .du_info        = du_info,
        .sometimes_info = NULL,
    };

    n00b_list_t *view    = n00b_dict_values(module_ctx->parameters);
    uint64_t     nparams = n00b_list_len(view);

    for (uint64_t i = 0; i < nparams; i++) {
        n00b_module_param_info_t *param = n00b_list_get(view, i, NULL);
        n00b_symbol_t            *sym   = param->linked_symbol;

        cfg_propogate_def(&ctx, sym, NULL, NULL);
    }

    module_ctx->ct->cfg->liveness_info = du_info;
    cfg_process_node(&ctx, module_ctx->ct->cfg, NULL);

    check_block_for_errors(&ctx, module_ctx->ct->cfg);
    check_for_module_exit_errors(&ctx, module_ctx->ct->cfg);

    int n = n00b_list_len(module_ctx->fn_def_syms);

    n00b_cfg_node_t *modexit = module_ctx->ct->cfg->contents.block_entrance.exit_node;
    n00b_dict_t     *moddefs = modexit->liveness_info;
    n00b_list_t     *stdefs  = modexit->sometimes_live;

    for (int i = 0; i < n; i++) {
        ctx.du_info          = moddefs;
        ctx.sometimes_info   = stdefs;
        n00b_symbol_t  *sym  = n00b_list_get(module_ctx->fn_def_syms,
                                           i,
                                           NULL);
        n00b_fn_decl_t *decl = sym->value;

        cfg_process_node(&ctx, decl->cfg, NULL);
        check_block_for_errors(&ctx, decl->cfg);
        check_for_fn_exit_errors(module_ctx, decl);

        n00b_list_t *cleanup = n00b_list(n00b_type_ref());

        for (int i = 0; i < n00b_list_len(stdefs); i++) {
            void *item = n00b_list_get(stdefs, i, NULL);
            n00b_list_add_if_unique(cleanup,
                                    item,
                                    (bool (*)(void *, void *))sym_cmp);
        }
        modexit->sometimes_live = stdefs;
    }
}
