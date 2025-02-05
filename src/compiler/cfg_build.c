#include "n00b.h"

void
n00b_cfg_gc_bits(uint64_t *bitmap, n00b_cfg_node_t *cfgnode)
{
    switch (cfgnode->kind) {
    case n00b_cfg_block_entrance:
        n00b_mark_raw_to_addr(bitmap,
                              cfgnode,
                              &cfgnode->contents.block_entrance.to_merge);
        return;
    case n00b_cfg_block_exit:
        n00b_mark_raw_to_addr(bitmap,
                              cfgnode,
                              &cfgnode->contents.block_exit.to_merge);
        return;
    case n00b_cfg_node_branch:
        n00b_mark_raw_to_addr(bitmap,
                              cfgnode,
                              &cfgnode->contents.branches.label);
        return;
    case n00b_cfg_jump:
        n00b_mark_raw_to_addr(bitmap, cfgnode, &cfgnode->contents.jump.target);
        return;
    case n00b_cfg_use:
    case n00b_cfg_def:
    case n00b_cfg_call:
        n00b_mark_raw_to_addr(bitmap, cfgnode, &cfgnode->contents.flow.deps);
        return;
    }
}

static n00b_cfg_node_t *
n00b_new_cfg_node()
{
    return n00b_gc_alloc_mapped(n00b_cfg_node_t, n00b_cfg_gc_bits);
}

static void
add_child(n00b_cfg_node_t *parent, n00b_cfg_node_t *child)
{
    n00b_cfg_branch_info_t *branch_info;

    switch (parent->kind) {
    case n00b_cfg_block_entrance:
        parent->contents.block_entrance.next_node = child;
        return;
    case n00b_cfg_block_exit:
        parent->contents.block_exit.next_node = child;
        return;
    case n00b_cfg_node_branch:
        branch_info = &parent->contents.branches;

        branch_info->branch_targets[branch_info->next_to_process++] = child;
        return;
    case n00b_cfg_jump:
        parent->contents.jump.dead_code = child;
        return;
    default:
        parent->contents.flow.next_node = child;
    }
}

n00b_cfg_node_t *
n00b_cfg_enter_block(n00b_cfg_node_t  *parent,
                     n00b_tree_node_t *treeloc)
{
    n00b_cfg_node_t *result = n00b_new_cfg_node();
    n00b_cfg_node_t *exit   = n00b_new_cfg_node();

    result->parent                            = parent;
    result->kind                              = n00b_cfg_block_entrance;
    result->reference_location                = treeloc;
    result->contents.block_entrance.exit_node = exit;

    result->contents.block_entrance.inbound_links = n00b_new(
        n00b_type_list(n00b_type_ref()));

    exit->reference_location                = treeloc;
    exit->parent                            = result;
    exit->starting_liveness_info            = NULL;
    exit->starting_sometimes                = NULL;
    exit->liveness_info                     = NULL;
    exit->sometimes_live                    = NULL;
    exit->kind                              = n00b_cfg_block_exit;
    exit->contents.block_exit.next_node     = NULL;
    exit->contents.block_exit.entry_node    = result;
    exit->contents.block_exit.to_merge      = NULL;
    exit->contents.block_exit.inbound_links = n00b_new(
        n00b_type_list(n00b_type_ref()));

    if (parent != NULL) {
        add_child(parent, result);
    }

    return result;
}

n00b_cfg_node_t *
n00b_cfg_exit_block(n00b_cfg_node_t  *parent,
                    n00b_cfg_node_t  *entry,
                    n00b_tree_node_t *treeloc)
{
    if (parent == NULL) {
        return NULL;
    }

    if (entry == NULL) {
        n00b_cfg_node_t *cur = parent;

        // Search parents until we find the block start.
        while (cur && cur->kind != n00b_cfg_block_entrance) {
            cur = cur->parent;
        }

        if (!cur) {
            return NULL;
        }
        entry = cur;
    }

    n00b_cfg_node_t *result = entry->contents.block_entrance.exit_node;

    if (parent->kind != n00b_cfg_jump) {
        n00b_list_append(result->contents.block_exit.inbound_links, parent);
        add_child(parent, result);
    }

    return result;
}

n00b_cfg_node_t *
n00b_cfg_block_new_branch_node(n00b_cfg_node_t  *parent,
                               int               num_branches,
                               n00b_string_t    *label,
                               n00b_tree_node_t *treeloc)
{
    n00b_cfg_node_t  *result  = n00b_new_cfg_node();
    n00b_cfg_node_t **targets = n00b_gc_array_alloc(n00b_cfg_node_t *,
                                                    num_branches);

    result->parent                            = parent;
    result->kind                              = n00b_cfg_node_branch;
    result->reference_location                = treeloc;
    result->contents.branches.num_branches    = num_branches;
    result->contents.branches.branch_targets  = targets;
    result->contents.branches.next_to_process = 0;
    result->contents.branches.label           = label;

    add_child(parent, result);

    while (parent->kind != n00b_cfg_block_entrance) {
        parent = parent->parent;
    }

    result->contents.branches.exit_node = parent->contents.block_entrance.exit_node;

    return result;
}

n00b_cfg_node_t *
n00b_cfg_add_return(n00b_cfg_node_t  *parent,
                    n00b_tree_node_t *treeloc,
                    n00b_cfg_node_t  *fn_exit_node)
{
    n00b_cfg_node_t *result = n00b_new_cfg_node();

    add_child(parent, result);

    result->kind               = n00b_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    n00b_list_append(fn_exit_node->contents.block_exit.inbound_links, result);

    result->contents.jump.target = fn_exit_node;

    return result;
}

n00b_cfg_node_t *
n00b_cfg_add_continue(n00b_cfg_node_t  *parent,
                      n00b_tree_node_t *treeloc,
                      n00b_string_t    *label)
{
    // Loops are structured as:
    // pre-entry initialization def/use info
    // block-start
    // loop-condition checking def/use info
    // branch_info.
    //
    // But the block-start node doesn't hold the loop info, so we need
    // to check the branch_info node we pass, and when we see it, the
    // next block entrance we find is where continue statements should
    // link to.
    //
    // Note that if the 'continue' statement targets a loop label, and
    // we never find that loop label, we return NULL and the caller is
    // responsible for issuing the error.

    n00b_cfg_node_t *result             = n00b_new_cfg_node();
    n00b_cfg_node_t *cur                = parent;
    bool             found_proper_block = false;

    result->kind               = n00b_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    add_child(parent, result);

    while (true) {
        switch (cur->kind) {
        case n00b_cfg_block_entrance:
            if (found_proper_block) {
                n00b_list_append(cur->contents.block_entrance.inbound_links,
                                 result);
                result->contents.jump.target = cur;
                return result;
            }
            break;
        case n00b_cfg_node_branch:
            if (label == NULL) {
                found_proper_block = true;
                break;
            }
            if (cur->contents.branches.label == NULL) {
                break;
            }

            if (!strcmp(cur->contents.branches.label->data, label->data)) {
                found_proper_block = true;
            }
            break;
        default:
            break;
        }
        if (cur->parent == NULL) {
            return NULL;
        }
        cur = cur->parent;
    }
}

// This should look EXACTLY like continue, except that, when we find the
// right block-enter node, we link to the associated EXIT node instead.
n00b_cfg_node_t *
n00b_cfg_add_break(n00b_cfg_node_t  *parent,
                   n00b_tree_node_t *treeloc,
                   n00b_string_t    *label)
{
    n00b_cfg_node_t *result             = n00b_new_cfg_node();
    n00b_cfg_node_t *cur                = parent;
    bool             found_proper_block = false;

    add_child(parent, result);

    result->kind               = n00b_cfg_jump;
    result->reference_location = treeloc;
    result->parent             = parent;

    while (true) {
        switch (cur->kind) {
        case n00b_cfg_block_entrance:
            if (found_proper_block) {
                n00b_cfg_node_t *target = cur->contents.block_entrance.exit_node;

                n00b_list_append(target->contents.block_exit.inbound_links,
                                 result);
                result->contents.jump.target = target;
                return result;
            }
            break;
        case n00b_cfg_node_branch:
            if (label == NULL) {
                found_proper_block = true;
                break;
            }
            if (cur->contents.branches.label == NULL) {
                break;
            }

            if (!strcmp(cur->contents.branches.label->data, label->data)) {
                found_proper_block = true;
            }
            break;
        default:
            break;
        }
        if (cur->parent == NULL) {
            return result;
        }
        cur = cur->parent;
    }
}

n00b_cfg_node_t *
n00b_cfg_add_def(n00b_cfg_node_t  *parent,
                 n00b_tree_node_t *treeloc,
                 n00b_symbol_t    *symbol,
                 n00b_list_t      *dependencies)
{
    n00b_cfg_node_t *result          = n00b_new_cfg_node();
    result->kind                     = n00b_cfg_def;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

n00b_cfg_node_t *
n00b_cfg_add_call(n00b_cfg_node_t  *parent,
                  n00b_tree_node_t *treeloc,
                  n00b_symbol_t    *symbol,
                  n00b_list_t      *dependencies)
{
    n00b_cfg_node_t *result          = n00b_new_cfg_node();
    result->kind                     = n00b_cfg_call;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;
    result->contents.flow.deps       = dependencies;

    add_child(parent, result);

    return result;
}

n00b_cfg_node_t *
n00b_cfg_add_use(n00b_cfg_node_t  *parent,
                 n00b_tree_node_t *treeloc,
                 n00b_symbol_t    *symbol)
{
    if (symbol->kind == N00B_SK_ENUM_VAL) {
        return parent;
    }
    n00b_cfg_node_t *result          = n00b_new_cfg_node();
    result->kind                     = n00b_cfg_use;
    result->reference_location       = treeloc;
    result->parent                   = parent;
    result->contents.flow.dst_symbol = symbol;

    add_child(parent, result);

    return result;
}

static n00b_string_t *
du_format_node(n00b_cfg_node_t *n)
{
    n00b_string_t *result;

    n00b_dict_t *liveness_info;
    n00b_list_t *sometimes_live;

    switch (n->kind) {
    case n00b_cfg_block_entrance:
        liveness_info  = n->starting_liveness_info;
        sometimes_live = n->starting_sometimes;
        break;
    case n00b_cfg_block_exit:
    case n00b_cfg_use:
    case n00b_cfg_def:
    case n00b_cfg_call:
        liveness_info  = n->liveness_info;
        sometimes_live = n->sometimes_live;
        break;
    default:
        n00b_unreachable();
    }

    if (liveness_info == NULL) {
        return n00b_cached_minus();
    }

    uint64_t             num_syms;
    hatrack_dict_item_t *info  = hatrack_dict_items_sort(liveness_info,
                                                        &num_syms);
    n00b_list_t         *cells = n00b_new(n00b_type_list(n00b_type_string()));

    for (unsigned int i = 0; i < num_syms; i++) {
        n00b_symbol_t     *sym    = info[i].key;
        n00b_cfg_status_t *status = info[i].value;

        if (status->last_def) {
            n00b_list_append(cells, sym->name);
        }
        else {
            n00b_list_append(cells,
                             n00b_string_concat(sym->name,
                                                n00b_cstring(" (err)")));
        }
    }

    if (num_syms == 0) {
        result = n00b_cached_minus();
    }
    else {
        if (num_syms) {
            result = n00b_string_join(cells, n00b_cached_comma_padded());
        }
        else {
            result = n00b_crich("«i»(none)");
        }
    }

    if (sometimes_live == NULL) {
        return result;
    }

    int num_sometimes = n00b_list_len(sometimes_live);
    if (num_sometimes == 0) {
        return result;
    }

    n00b_list_t *l2 = n00b_new(n00b_type_list(n00b_type_string()));

    for (int i = 0; i < num_sometimes; i++) {
        n00b_symbol_t *sym = n00b_list_get(sometimes_live, i, NULL);

        n00b_list_append(l2, sym->name);
    }

    return n00b_cformat("«#»; st: «#»",
                        result,
                        n00b_string_join(l2, n00b_cached_comma_padded()));
}

static n00b_tree_node_t *
n00b_cfg_build_repr(n00b_cfg_node_t  *node,
                    n00b_tree_node_t *tree_parent,
                    n00b_cfg_node_t  *cfg_parent,
                    n00b_string_t    *label)
{
    n00b_string_t    *str = NULL;
    n00b_tree_node_t *result;
    n00b_cfg_node_t  *link;
    uint64_t          node_addr = (uint64_t)(void *)node;
    uint64_t          link_addr;

    if (!node) {
        return NULL;
    }

    switch (node->kind) {
    case n00b_cfg_block_entrance:
        link      = node->contents.block_entrance.exit_node;
        link_addr = (uint64_t)(void *)link;
        str       = n00b_cformat("@«#:x»: «em»Enter«/» «i»(«#»)",
                           node_addr,
                           du_format_node(node));
        if (label == NULL) {
            label = n00b_cstring("block");
        }
        result                 = n00b_new(n00b_type_tree(n00b_type_string()),
                          n00b_kw("contents", label));
        n00b_tree_node_t *sub1 = n00b_new(n00b_type_tree(n00b_type_string()),
                                          n00b_kw("contents", n00b_ka(str)));

        n00b_tree_adopt_node(tree_parent, result);
        n00b_tree_adopt_node(result, sub1);
        n00b_cfg_build_repr(node->contents.flow.next_node, sub1, node, NULL);

        n00b_cfg_build_repr(node->contents.block_entrance.exit_node,
                            result,
                            NULL,
                            NULL);

        return result;
    case n00b_cfg_node_branch:
        if (node->contents.branches.label != NULL) {
            str = n00b_cformat("@«#:x»: «em»branch«/» «i»«#»«/»",
                               node_addr,
                               node->contents.branches.label);
        }
        else {
            str = n00b_cformat("@«#:x»: «em»branch", node_addr);
        }

        result = n00b_new(n00b_type_tree(n00b_type_string()),
                          n00b_kw("contents", str));
        n00b_tree_adopt_node(tree_parent, result);

        for (int64_t i = 0; i < node->contents.branches.num_branches; i++) {
            n00b_string_t    *label = n00b_cformat("b«#»", i);
            n00b_tree_node_t *sub   = n00b_new(n00b_type_tree(n00b_type_string()),
                                             n00b_kw("contents", n00b_ka(label)));
            n00b_cfg_node_t  *kid   = node->contents.branches.branch_targets[i];

            n00b_assert(kid != NULL);

            n00b_tree_adopt_node(result, sub);
            n00b_cfg_build_repr(kid, sub, node, NULL);
        }

        n00b_cfg_build_repr(node->contents.branches.exit_node,
                            tree_parent,
                            node,
                            NULL);

        return result;

    case n00b_cfg_block_exit:
        if (cfg_parent) {
            return NULL;
        }
        n00b_cfg_node_t *nn = node->contents.block_exit.next_node;

        if (nn != NULL) {
            str = n00b_cformat("@«#:x»: «em»Exit«/» (next @«#:x»)  «i»(«#»)",
                               (int64_t)node_addr,
                               (int64_t)(void *)nn,
                               du_format_node(node));
        }
        else {
            str = n00b_cformat("@«#:x»: «em»Exit«/»  «i»(«#»)",
                               node_addr,
                               du_format_node(node));
        }
        break;
    case n00b_cfg_use:
        str = n00b_cformat("@«#:x»: [«em»USE«/» «#» «i»(«#»)",
                           node_addr,
                           node->contents.flow.dst_symbol->name,
                           du_format_node(node));
        break;
    case n00b_cfg_def:
        str = n00b_cformat("@«#:x»: «em»DEF«/» «#» «i»(«#»)",
                           node_addr,
                           node->contents.flow.dst_symbol->name,
                           du_format_node(node));
        break;
    case n00b_cfg_call:
        str = n00b_cformat("@«#:x»: «em»call«/» «#»",
                           node_addr,
                           node->contents.flow.dst_symbol->name);
        break;
    case n00b_cfg_jump:
        link      = node->contents.jump.target;
        link_addr = (uint64_t)(void *)link;
        str       = n00b_cformat("@«#:x»: «em»jmp«/» «#:x»",
                           node_addr,
                           link_addr);
        break;
    default:
        n00b_unreachable();
    }

    if (node->kind == n00b_cfg_block_entrance) {
    }

    result = n00b_new(n00b_type_tree(n00b_type_string()),
                      n00b_kw("contents", n00b_ka(str)));

    n00b_tree_adopt_node(tree_parent, result);

    switch (node->kind) {
    case n00b_cfg_block_entrance:
    case n00b_cfg_node_branch:
        n00b_unreachable();
    case n00b_cfg_block_exit:
        link = node->contents.flow.next_node;

        if (link) {
            n00b_cfg_build_repr(link, result->parent, node, NULL);
        }
        break;

    default:
        link = node->contents.flow.next_node;

        if (link) {
            n00b_cfg_build_repr(link, tree_parent, node, NULL);
        }
        break;
    }

    return result;
}

n00b_table_t *
n00b_cfg_repr(n00b_cfg_node_t *node)
{
    n00b_tree_node_t *root = n00b_tree(n00b_cstring("Root"));

    n00b_cfg_build_repr(node, root, NULL, NULL);
    return n00b_tree_format(root, NULL, NULL, false);
}
