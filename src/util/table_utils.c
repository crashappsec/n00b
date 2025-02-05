// Implement basic widgets using tables:
// 1. Tree printing
// 2. Ordered lists
// 3. Unordered lists

#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    n00b_table_t      *table;
    n00b_tree_props_t *props;
    void              *callback;
    void              *thunk;
    bool               show_cycles;
    n00b_dict_t       *cycle_nodes;
    bool               cycle;
    bool               at_start;
    n00b_set_t        *to_collapse;
    n00b_list_t       *depth_positions;
    n00b_list_t       *depth_totals;

} tree_fmt_t;

typedef n00b_string_t *(*thunk_cb)(void *, void *);
typedef n00b_string_t *(*thunkless_cb)(void *);

static n00b_string_t *
build_tree_pad(tree_fmt_t *ctx)
{
    int depth = n00b_list_len(ctx->depth_positions);

    if (depth == 1) {
        return n00b_cached_empty_string();
    }

    int num_cps = (depth - 1) * (ctx->props->vpad + 1)
                + ctx->props->ipad;
    n00b_codepoint_t *arr = alloca(num_cps * sizeof(n00b_codepoint_t));
    n00b_codepoint_t *p   = arr;
    int64_t           pos;
    int64_t           tot;

    // Because we treat the root specially, start this at 1, not 0.
    for (int i = 1; i < depth - 1; i++) {
        pos = (int64_t)n00b_list_get(ctx->depth_positions, i, NULL);
        tot = (int64_t)n00b_list_get(ctx->depth_totals, i, NULL);

        if (pos == tot) {
            *p++ = ctx->props->pad;
        }
        else {
            *p++ = ctx->props->vchar;
        }
        for (int j = 0; j < ctx->props->vpad; j++) {
            *p++ = ctx->props->pad;
        }
    }

    pos = (int64_t)n00b_list_get(ctx->depth_positions, depth - 1, NULL);
    tot = (int64_t)n00b_list_get(ctx->depth_totals, depth - 1, NULL);

    if (pos == tot) {
        *p++ = ctx->props->lchar;
    }
    else {
        *p++ = ctx->props->tchar;
    }

    for (int i = ctx->props->ipad; i < ctx->props->vpad; i++) {
        *p++ = ctx->props->hchar;
    }

    for (int i = 0; i < ctx->props->ipad; i++) {
        *p++ = ctx->props->pad;
    }

    n00b_string_t *pad = n00b_utf32(arr, p - arr);
    n00b_string_style(pad, &ctx->props->text_style);

    return pad;
}

static inline void
reset_builder_ctx(tree_fmt_t *ctx)
{
    ctx->depth_positions = n00b_list(n00b_type_int());
    ctx->depth_totals    = n00b_list(n00b_type_int());
    ctx->cycle           = false;
    ctx->at_start        = true;
}

static bool
tree_builder(n00b_tree_node_t *node, int depth, tree_fmt_t *ctx)
{
    int64_t        my_position;
    int64_t        total_in_group;
    n00b_string_t *repr;
    bool           result   = true;
    void          *contents = n00b_tree_get_contents(node);

    if (!ctx->depth_positions) {
        reset_builder_ctx(ctx);
    }

    if (ctx->at_start) {
        my_position    = 1;
        total_in_group = 1;
        ctx->at_start  = false;
    }
    else {
        my_position    = (int64_t)n00b_list_pop(ctx->depth_positions) + 1;
        total_in_group = (int64_t)n00b_list_pop(ctx->depth_totals);
    }

    n00b_list_append(ctx->depth_positions, (void *)my_position);
    n00b_list_append(ctx->depth_totals, (void *)total_in_group);

    if (ctx->to_collapse && n00b_set_contains(ctx->to_collapse, node)) {
        result = false;
    }

    if (ctx->callback) {
        if (ctx->thunk) {
            repr = (*(thunk_cb)ctx->callback)(contents, ctx->thunk);
        }

        else {
            repr = (*(thunkless_cb)ctx->callback)(contents);
        }
    }
    else {
        repr = n00b_to_string(contents);
    }

    if (!repr) {
        // Note that we can still descend, we'll just hide the node.
        goto on_exit;
    }

    if (ctx->cycle) {
        repr = n00b_cformat("«#» «em6»(CYCLE #«#»)«/» ",
                            repr,
                            hatrack_dict_get(ctx->cycle_nodes, node, NULL));
    }

    if (ctx->props->remove_newlines) {
        int64_t ix = n00b_string_find(repr, n00b_cached_newline());

        if (ix != -1) {
            repr = n00b_string_slice(repr, 0, ix);
            repr = n00b_string_concat(repr, n00b_cached_ellipsis());
        }
    }
    else {
        repr = n00b_cached_ellipsis();
    }

    repr = n00b_string_concat(build_tree_pad(ctx), repr);

    n00b_table_add_cell(ctx->table, repr);
    n00b_table_end_row(ctx->table);

on_exit:

    if (my_position == total_in_group) {
        if (ctx->cycle || !node->num_kids) {
            // If we call no children and are last at our level,
            // we have to clean up the stack, removing all levels that are
            // 'done'.

            int n = n00b_list_len(ctx->depth_positions);

            while (n--) {
                my_position    = (int64_t)n00b_list_pop(ctx->depth_positions);
                total_in_group = (int64_t)n00b_list_pop(ctx->depth_totals);
                if (my_position != total_in_group) {
                    n00b_list_append(ctx->depth_positions, (void *)my_position);
                    n00b_list_append(ctx->depth_totals, (void *)total_in_group);
                    break;
                }
            }
        }
    }

    if (node->num_kids && !ctx->cycle) {
        n00b_list_append(ctx->depth_positions, (void *)0ULL);
        n00b_list_append(ctx->depth_totals, (void *)(int64_t)node->num_kids);
    }

    return result;
}

static bool
cycle_callback(n00b_tree_node_t *node, int depth, tree_fmt_t *ctx)
{
    if (ctx->show_cycles) {
        return false;
    }

    if (!ctx->cycle_nodes) {
        ctx->cycle_nodes = n00b_dict(n00b_type_ref(), n00b_type_int());
    }

    if (!hatrack_dict_get(ctx->cycle_nodes, node, NULL)) {
        hatrack_dict_put(ctx->cycle_nodes,
                         node,
                         (void *)hatrack_dict_len(ctx->cycle_nodes) + 1);
    }

    ctx->cycle = true;
    tree_builder(node, depth, ctx);
    ctx->cycle = false;

    return false;
}

n00b_table_t *
n00b_tree_format(n00b_tree_node_t *tree,
                 void             *callback,
                 void             *thunk,
                 bool              show_cycles)
{
    if (callback == NULL) {
        N00B_CRAISE("Must provide a callback for tree formatting.");
    }
    n00b_tree_props_t *props = n00b_get_current_tree_formatting();

    if (props->vpad < 1) {
        props->vpad = 1;
    }
    if (props->ipad < 0) {
        props->ipad = 1;
    }

    n00b_table_t *result = n00b_table("style", n00b_ka(N00B_TABLE_FLOW));

    tree_fmt_t fmt_info = {
        .table       = result,
        .callback    = callback,
        .thunk       = thunk,
        .props       = props,
        .show_cycles = false,
    };

    n00b_tree_walk_with_cycles(tree,
                               (n00b_walker_fn)tree_builder,
                               (n00b_walker_fn)cycle_callback,
                               &fmt_info);

    if (!show_cycles || !fmt_info.cycle_nodes) {
        return result;
    }

    fmt_info.show_cycles = true;

    uint64_t           n;
    n00b_tree_node_t **nodes;
    n00b_string_t     *s;

    nodes = (void *)hatrack_dict_keys_sort(fmt_info.cycle_nodes, &n);

    fmt_info.cycle_nodes = NULL;

    for (uint64_t i = 0; i < n; i++) {
        reset_builder_ctx(&fmt_info);

        s = n00b_cformat("[«em1»Rooted from Cycle «#»: ", i + 1);
        n00b_table_add_cell(result, s);
        n00b_table_end_row(result);
        n00b_tree_walk_with_cycles(nodes[i],
                                   (n00b_walker_fn)tree_builder,
                                   NULL,
                                   &fmt_info);
    }

    return result;
}

n00b_table_t *
n00b_ordered_list(n00b_list_t *items, n00b_string_t *template)
{
    if (!template) {
        template = n00b_cstring("«#:i».");
    }
    n00b_table_t *result = n00b_table("columns",
                                      n00b_ka(2),
                                      "style",
                                      n00b_ka(N00B_TABLE_OL));

    int n = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        n00b_table_add_cell(result, n00b_format(template, i + 1));
        n00b_table_add_cell(result, n00b_list_get(items, i, NULL));
    }

    n00b_table_end(result);

    return result;
}

n00b_table_t *
n00b_unordered_list(n00b_list_t *items, n00b_string_t *bullet)
{
    if (!bullet) {
        bullet = n00b_cstring("•");
    }

    n00b_table_t *result = n00b_table("columns",
                                      n00b_ka(2),
                                      "style",
                                      n00b_ka(N00B_TABLE_UL));

    int n = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        n00b_table_add_cell(result, bullet);
        n00b_table_add_cell(result, n00b_list_get(items, i, NULL));
    }

    n00b_table_end(result);

    return result;
}
