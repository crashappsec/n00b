#include "n00b.h"

static void
tree_node_init(n00b_tree_node_t *t, va_list args)
{
    n00b_obj_t contents = NULL;

    n00b_karg_va_init(args);
    n00b_kw_ptr("contents", contents);

    t->children     = n00b_gc_array_alloc(n00b_tree_node_t **, 4);
    t->alloced_kids = 4;
    t->num_kids     = 0;
    t->contents     = contents;
}

void
n00b_tree_adopt_node(n00b_tree_node_t *t, n00b_tree_node_t *kid)
{
    if (!kid->parent) {
        kid->parent = t;
    }

    if (t->num_kids == t->alloced_kids) {
        t->alloced_kids *= 2;
        n00b_tree_node_t **new_kids = n00b_gc_array_alloc(n00b_tree_node_t **,
                                                          t->alloced_kids);
        for (int i = 0; i < t->num_kids; i++) {
            new_kids[i] = t->children[i];
        }
        t->children = new_kids;
    }

    t->children[t->num_kids++] = kid;
}

n00b_tree_node_t *
n00b_tree_add_node(n00b_tree_node_t *t, void *item)
{
    n00b_type_t      *tree_type   = n00b_get_my_type(t);
    n00b_list_t      *type_params = n00b_type_get_params(tree_type);
    n00b_type_t      *item_type   = n00b_list_get(type_params, 0, NULL);
    n00b_tree_node_t *kid         = n00b_new(n00b_type_tree(item_type),
                                     n00b_kw("contents", n00b_ka(item)));

    n00b_tree_adopt_node(t, kid);

    return kid;
}

void
n00b_tree_adopt_and_prepend(n00b_tree_node_t *t, n00b_tree_node_t *kid)
{
    kid->parent = t;

    if (t->num_kids == t->alloced_kids) {
        t->alloced_kids *= 2;
        n00b_tree_node_t **new_kids = n00b_gc_array_alloc(n00b_tree_node_t **,
                                                          t->alloced_kids);
        for (int i = 0; i < t->num_kids; i++) {
            new_kids[i + 1] = t->children[i];
        }
        t->children = new_kids;
    }
    else {
        int i = t->num_kids;

        while (i != 0) {
            t->children[i] = t->children[i - 1];
            i--;
        }
    }

    t->children[0] = kid;
    t->num_kids++;
}

n00b_tree_node_t *
n00b_tree_prepend_node(n00b_tree_node_t *t, void *item)
{
    n00b_type_t      *tree_type   = n00b_get_my_type(t);
    n00b_list_t      *type_params = n00b_type_get_params(tree_type);
    n00b_type_t      *item_type   = n00b_list_get(type_params, 0, NULL);
    n00b_tree_node_t *kid         = n00b_new(n00b_type_tree(item_type),
                                     n00b_kw("contents", n00b_ka(item)));

    n00b_tree_adopt_and_prepend(t, kid);

    return kid;
}

n00b_tree_node_t *
n00b_tree_get_child(n00b_tree_node_t *t, int64_t i)
{
    if (i < 0 || i >= t->num_kids) {
        N00B_CRAISE("Index out of bounds.");
    }

    return t->children[i];
}

n00b_list_t *
n00b_tree_children(n00b_tree_node_t *t)
{
    n00b_type_t *tree_type   = n00b_get_my_type(t);
    n00b_list_t *type_params = n00b_type_get_params(tree_type);
    n00b_type_t *item_type   = n00b_list_get(type_params, 0, NULL);
    n00b_list_t *result;

    result = n00b_new(n00b_type_list(item_type),
                      n00b_kw("length", n00b_ka(t->num_kids)));

    for (int i = 0; i < t->num_kids; i++) {
        n00b_list_append(result, t->children[i]);
    }

    return result;
}

bool print_xform_info = false;

n00b_tree_node_t *
n00b_tree_str_transform(n00b_tree_node_t *t, n00b_str_t *(*fn)(void *))
{
    if (t == NULL) {
        return NULL;
    }

    n00b_str_t       *str    = n00b_to_utf8(fn(n00b_tree_get_contents(t)));
    n00b_tree_node_t *result = n00b_new(n00b_type_tree(n00b_type_utf8()),
                                        n00b_kw("contents", n00b_ka(str)));

    for (int64_t i = 0; i < t->num_kids; i++) {
        n00b_tree_adopt_node(result, n00b_tree_str_transform(t->children[i], fn));
    }

    return result;
}

typedef struct {
    n00b_dict_t   *memos;
    n00b_walker_fn callback;
    n00b_walker_fn cycle_cb;
    void          *thunk;
    uint64_t       depth;
} walk_ctx;

static void
internal_walker(walk_ctx *ctx, n00b_tree_node_t *cur)
{
    if (cur == NULL) {
        return;
    }

    // Do not walk the same node twice if there's a cycle, unless
    // a callback overrides.
    if (!hatrack_dict_add(ctx->memos, cur, NULL)) {
        if (ctx->cycle_cb) {
            (*ctx->cycle_cb)(cur, ctx->depth, ctx->thunk);
        }

        return;
    }

    // Callback returns 'true' to tell us to descend into children.
    if (!(*ctx->callback)(cur, ctx->depth, ctx->thunk)) {
        return;
    }
    ctx->depth++;
    n00b_assert(!cur->num_kids || cur->children);

    for (int64_t i = 0; i < cur->num_kids; i++) {
        internal_walker(ctx, cur->children[i]);
    }
    --ctx->depth;

    hatrack_dict_remove(ctx->memos, cur);
}

void
n00b_tree_walk_with_cycles(n00b_tree_node_t *root,
                           n00b_walker_fn    cb1,
                           n00b_walker_fn    cb2,
                           void             *thunk)
{
    walk_ctx ctx = {
        .memos    = n00b_dict(n00b_type_ref(), n00b_type_ref()),
        .callback = cb1,
        .cycle_cb = cb2,
        .thunk    = thunk,
        .depth    = 0,
    };

    internal_walker(&ctx, root);
}

void
n00b_tree_walk(n00b_tree_node_t *root, n00b_walker_fn callback, void *thunk)
{
    n00b_tree_walk_with_cycles(root, callback, NULL, thunk);
}

void
n00b_tree_node_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_tree_node_t *n = (n00b_tree_node_t *)alloc;
    n00b_mark_raw_to_addr(bitfield, alloc, &n->contents);
}

const n00b_vtable_t n00b_tree_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)tree_node_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)n00b_tree_node_set_gc_bits,
        [N00B_BI_FINALIZER]   = NULL,
    },
};
