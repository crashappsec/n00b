#include "n00b.h"

static n00b_type_ctx_t runtime_universe = {
    0,
};
static char                *build_type_name(n00b_type_ctx_t *, n00b_tnode_t *);
static inline n00b_tnode_t *tnode_from_offset(n00b_type_ctx_t *, n00b_ntype_t);

static n00b_tnode_t *
reverse_lookup(n00b_type_ctx_t *ctx, n00b_ntype_t id)
{
    n00b_tnode_t *result = n00b_dict_get(&ctx->reverse_map, id, NULL);

    if (!result) {
        return tnode_from_offset(ctx, N00B_T_TRUE_REF);
    }

    while (result->forward) {
        result = result->forward;
    }

    return result;
}

static void
setup_type_ctx(n00b_type_ctx_t *ctx, int n)
{
    n00b_zarray_t *za = n00b_zarray_new(n, sizeof(n00b_tnode_t));

    ctx->contents = za;

    for (int i = 0; i < N00B_T_NUM_PRIMITIVE_BUILTINS; i++) {
        n00b_tnode_t *tnode;

        uint32_t id = n00b_zarray_new_cell(za, (void **)&tnode);
        assert(id == (uint32_t)i);
        *tnode = (n00b_tnode_t){
            .concrete = true,
            .info     = (n00b_thash_payload_t){
                    .num_params  = 0,
                    .base_typeid = i,
                    .kind        = n00b_thasht_primitive,
            }};
    }
}

void
_n00b_init_type_ctx(n00b_type_ctx_t *ctx, ...)
{
    keywords
    {
        int  starting_entries = 8192;
        bool save_type_names  = true;
    }

    setup_type_ctx(ctx, starting_entries);
    ctx->keep_names = save_type_names;

    // clang-format off
    if (ctx->keep_names) {
        n00b_dict_init(&ctx->name_map,
                       num_entries  : starting_entries * 2,
                       system_dict  : true,
                       hide_from_gc : true);
    }

    n00b_dict_init(&ctx->reverse_map,
                   num_entries  : starting_entries * 2,
                   system_dict  : true,
                   hide_from_gc : true);
    // clang-format on
}

once n00b_type_ctx_t *
n00b_get_runtime_universe(void)
{
    n00b_init_type_ctx(&runtime_universe);

    return &runtime_universe;
}

static inline bool
tnode_is_concrete(n00b_tnode_t *tn)
{
    while (tn->forward) {
        tn = tn->forward;
    }

    if (tn->concrete) {
        return true;
    }

    if (!tn->info.num_params) {
        return false;
    }

    for (int i = 0; i < tn->info.num_params; i++) {
        if (!tnode_is_concrete(tn->params + i)) {
            return false;
        }
    }

    return true;
}

static void
thash_base(n00b_tnode_t *tn, uint64_t *hv)
{
    while (tn->forward) {
        tn = tn->forward;
    }

    *hv = komihash(&tn->info, sizeof(n00b_thash_payload_t), *hv);

    for (int i = 0; i < tn->info.num_params; i++) {
        thash_base(tn->params + i, hv);
    }
}

static inline uint64_t
thash(n00b_tnode_t *tn)
{
    if (tn->info.base_typeid < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return tn->info.base_typeid;
    }
    uint64_t seed = 0;
    thash_base(tn, &seed);

    seed &= ~N00B_THASH_OFFSET_BIT;

    return seed;
}

static inline n00b_ntype_t
t_to_offset(n00b_type_ctx_t *ctx, n00b_tnode_t *tn)
{
    // Unify returns a null pointer when there's no unification.
    if (!tn) {
        return N00B_T_ERROR;
    }

    while (tn->forward) {
        tn = tn->forward;
    }

    int64_t ix = n00b_zarray_get_index(ctx->contents, tn);

    assert(ix != -1);

    return (n00b_ntype_t)(N00B_THASH_OFFSET_BIT | (uint64_t)ix);
}

static inline n00b_tnode_t *
tnode_from_offset(n00b_type_ctx_t *ctx, n00b_ntype_t t)
{
    assert((t & N00B_THASH_OFFSET_BIT) || t < N00B_T_NUM_PRIMITIVE_BUILTINS);

    uint32_t      offset = (uint32_t)(t & ~N00B_THASH_OFFSET_BIT);
    n00b_tnode_t *result = n00b_zarray_cell_address(ctx->contents, offset);

    while (result && result->forward) {
        result = result->forward;
    }

    return result;
}

n00b_tnode_t *
_n00b_get_tnode(n00b_ntype_t t, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    if (!(t & N00B_THASH_OFFSET_BIT)) {
        if (t > N00B_T_NUM_PRIMITIVE_BUILTINS) {
            return reverse_lookup(ctx, t);
        }
    }

    t |= N00B_THASH_OFFSET_BIT;

    return tnode_from_offset(ctx, t);
}

static inline n00b_ntype_t
n00b_tnode_to_type(n00b_type_ctx_t *ctx, n00b_tnode_t *tn)
{
    n00b_ntype_t result;

    if (tnode_is_concrete(tn)) {
        result = thash(tn);
    }
    else {
        n00b_ntype_t res = t_to_offset(ctx, tn);
        assert(tn == tnode_from_offset(ctx, res));
        result = res;
    }

    n00b_tnode_t *cache = NULL;

    n00b_dict_cas(&ctx->reverse_map, result, &cache, tn, true, false);

    return result;
}

n00b_ntype_t
_n00b_get_type(n00b_tnode_t *tn, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    return n00b_tnode_to_type(ctx, tn);
}

static n00b_tnunify_result_kind
n00b_tnodes_can_unify(n00b_tnode_t *n1, n00b_tnode_t *n2)
{
    n00b_tnunify_result_kind result = n00b_types_unify;

    while (n1->forward) {
        n1 = n1->forward;
    }

    while (n2->forward) {
        n2 = n2->forward;
    }

    if (n1 == n2) {
        return true;
    }

    if (n2->info.kind == n00b_thasht_typevar) {
        n00b_tnode_t *swap = n1;
        n1                 = n2;
        n2                 = swap;
    }

    if (n1->info.kind == n00b_thasht_typevar) {
        // For now, don't check constraints. Just say yes.
        return n00b_types_require_binding;
    }

    if (n00b_type_is_int_type(n1->info.base_typeid)
        && n00b_type_is_int_type(n2->info.base_typeid)) {
        return n00b_get_promotion_type(n1->info.base_typeid,
                                       n2->info.base_typeid);
    }

    if (n1->info.kind != n2->info.kind) {
        return n00b_types_incompatable;
    }

    if (n1->info.base_typeid != n2->info.base_typeid) {
        return n00b_types_incompatable;
    }

    if (n1->info.kind == n00b_thasht_primitive) {
        return n00b_types_unify;
    }

    if (n1->info.num_params != n2->info.num_params) {
        return n00b_types_incompatable;
    }

    for (int i = 0; i < n1->info.num_params; i++) {
        switch (n00b_tnodes_can_unify(&n1->params[i], &n2->params[i])) {
        case n00b_types_incompatable:
            return n00b_types_incompatable;
        case n00b_types_require_binding:
            result = n00b_types_require_binding;
            continue;
        default:
            continue;
        }
    }

    return result;
}

static n00b_tnode_t *
n00b_tnodes_unify(n00b_tnode_t *n1, n00b_tnode_t *n2)
{
    while (n1->forward) {
        n1 = n1->forward;
    }

    while (n2->forward) {
        n2 = n2->forward;
    }

    if (n1 == n2) {
        return n1;
    }

    if (n2->info.kind == n00b_thasht_typevar) {
        n00b_tnode_t *swap = n1;
        n1                 = n2;
        n2                 = swap;
    }

    if (n1->info.kind == n00b_thasht_typevar) {
        // For now, don't check constraints. Just unify.
        n1->forward = n2;
        return n2;
    }

    // Cast to the promotion type if both are ints.
    if (n00b_type_is_int_type(n1->info.base_typeid)
        && n00b_type_is_int_type(n2->info.base_typeid)) {
        return n00b_get_tnode(n00b_get_promotion_type(n1->info.base_typeid,
                                                      n2->info.base_typeid));
    }

    if (n1->info.kind != n2->info.kind) {
        return NULL;
    }

    if (n1->info.base_typeid != n2->info.base_typeid) {
        return NULL;
    }

    if (n1->info.kind == n00b_thasht_primitive) {
        return n1;
    }

    if (n1->info.num_params != n2->info.num_params) {
        return NULL;
    }

    bool concrete_possible = true;

    for (int i = 0; i < n1->info.num_params; i++) {
        n00b_tnode_t *sub = n00b_tnodes_unify(&n1->params[i], &n2->params[i]);

        if (!sub) {
            return NULL;
        }

        if (!sub->concrete) {
            concrete_possible = false;
        }
    }

    // These types are being merged, but they're different physical nodes.
    // We want future merges for both sides to stay in sync, so we
    // go ahead and bind them together.

    n00b_tnode_t *result;

    if (n1->info.base_typeid < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        n2->forward = n1;

        result = n1;
    }
    else {
        n1->forward = n2;
        result      = n2;
    }

    if (concrete_possible) {
        result->concrete = true;
    }

    return result;
}

// Checks for equality, without unification. This is meant primarily
// for runtime tests.
n00b_tnunify_result_kind
_n00b_types_can_unify(n00b_ntype_t n1, n00b_ntype_t n2, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    if (n1 == n2) {
        return n00b_types_unify;
    }

    // If we don't have the type node locally, then the types aren't
    // compatable; they were both concrete typpes.
    n00b_tnode_t *t1 = n00b_get_tnode(n1, ctx : ctx);
    n00b_tnode_t *t2 = n00b_get_tnode(n2, ctx : ctx);

    if (!t1 || !t2) {
        return n00b_types_incompatable;
    }

    return n00b_tnodes_can_unify(t1, t2);
}

static char *
build_type_name(n00b_type_ctx_t *ctx, n00b_tnode_t *node)
{
    n00b_string_t *args;
    n00b_string_t *final = NULL;

    assert(node);

    while (node->forward) {
        node = node->forward;
    }

    // We're going to use our string API internally to construct the name,
    // but when we have the name, we will copy it out into regular ol'
    // malloc()'d memory, since we want to hide it from the GC.

    int32_t base = node->info.base_typeid;

    assert(base < N00B_NUM_BUILTIN_DTS);

    if (node->info.base_typeid < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return (char *)n00b_base_type_info[base].name;
    }

    n00b_list_t *pnames = n00b_internal_list();

    for (int i = 0; i < node->info.num_params; i++) {
        char *s = build_type_name(ctx, node->params + i);
        n00b_private_list_append(pnames, n00b_cstring(s));
    }

    n00b_tnode_kind kind = node->info.kind;

    if (kind == n00b_thasht_typevar) {
        uint32_t offset = t_to_offset(ctx, node) & 0xffffffff;
        final           = n00b_cformat("`tv_[=#:x=]", offset);
    }
    if (kind == n00b_thasht_fn || kind == n00b_thasht_varargs_fn) {
        n00b_string_t *rtype = n00b_private_list_pop(pnames);

        if (kind == n00b_thasht_varargs_fn) {
            n00b_string_t *var_type = NULL;

            var_type = n00b_cformat("*[=#=]", n00b_private_list_pop(pnames));
            n00b_private_list_append(pnames, var_type);
        }

        args  = n00b_string_join(pnames, n00b_cached_comma_padded());
        final = n00b_cformat("([=#=]) -> [=#=]", args, rtype);
    }
    if (!final) {
        n00b_string_t *root_name = n00b_cstring(n00b_base_type_info[base].name);

        args  = n00b_string_join(pnames, n00b_cached_comma_padded());
        final = n00b_cformat("[=#=][[=#=]]", root_name, args);
    }

    if (ctx->keep_names && !n00b_dict_contains(&ctx->name_map, node)) {
        n00b_dict_put(&ctx->name_map, node, final->data);
    }

    return strdup(final->data);
}

n00b_string_t *
_n00b_ntype_get_name(n00b_ntype_t id, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    char *name = NULL;

    if (ctx->keep_names) {
        name = n00b_dict_get(&ctx->name_map, id, NULL);
    }

    if (!name) {
        name = build_type_name(ctx, n00b_get_tnode(id));
    }

    return n00b_cstring(name);
}

n00b_ntype_t
_n00b_unify(n00b_ntype_t n1, n00b_ntype_t n2, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    if (n1 == n2) {
        return n1;
    }

    n00b_tnode_t *t1 = n00b_get_tnode(n1, ctx : ctx);
    n00b_tnode_t *t2 = n00b_get_tnode(n2, ctx : ctx);

    // If we don't have the type node locally, then the types aren't
    // compatable; they were both concrete typpes.
    if (!t1 || !t2) {
        return N00B_T_ERROR;
    }

    n00b_tnode_t *uresult = n00b_tnodes_unify(t1, t2);

    if (!uresult) {
        return N00B_T_ERROR;
    }

    n00b_dict_put(&ctx->reverse_map, n1, uresult);
    n00b_dict_put(&ctx->reverse_map, n2, uresult);

    if (uresult->concrete) {
        n00b_ntype_t id = n00b_tnode_to_type(ctx, uresult);

        if (ctx->keep_names && !n00b_dict_contains(&ctx->name_map, (void *)id)) {
            char *name = build_type_name(ctx, uresult);
            n00b_dict_put(&ctx->name_map, id, name);
        }

        return id;
    }

    return t_to_offset(ctx, uresult);
}

n00b_string_t *
_n00b_base_type_name(n00b_ntype_t id, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    if (id & N00B_THASH_OFFSET_BIT) {
        return n00b_cstring(build_type_name(ctx, tnode_from_offset(ctx, id)));
    }

    if (ctx->keep_names) {
        char *name = n00b_dict_get(&ctx->name_map, id, NULL);

        if (name) {
            return n00b_cstring(name);
        }
    }

    if (id < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return n00b_cstring(n00b_base_type_info[id].name);
    }

    assert(id > N00B_NUM_BUILTIN_DTS);

    return n00b_cstring("?(unrecorded type name)?");
}

n00b_ntype_t
n00b_type_instantiate(int32_t base, n00b_karg_info_t *kargs)
{
    keywords
    {
        n00b_ntype_t    *params     = NULL;
        uint32_t         num_params = 0;
        bool             varargs    = false;
        n00b_tnode_t   **node_ptr   = NULL;
        char            *name       = NULL;
        n00b_type_ctx_t *ctx        = n00b_get_runtime_universe();
    }

    assert(ctx);

    if (base < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return (n00b_ntype_t)base;
    }

    n00b_tnode_kind kind;

    switch (base) {
    case N00B_T_LIST:
    case N00B_T_TREE:
        kind = n00b_thasht_list;
        break;
    case N00B_T_TUPLE:
        kind = n00b_thasht_tuple;
        break;
    case N00B_T_DICT:
        kind = n00b_thasht_dict;
        break;
    case N00B_T_SET:
        kind = n00b_thasht_set;
        break;
    case N00B_T_FUNCDEF:
        if (varargs) {
            kind = n00b_thasht_varargs_fn;
        }
        else {
            kind = n00b_thasht_fn;
        }
        break;
    case N00B_T_TRUE_REF:
    case N00B_T_GENERIC:
        kind = n00b_thasht_typevar;
        break;
    default:
        abort();
    }

    assert((!num_params && !params) || (params && num_params));

    n00b_tnode_t *n;
    uint32_t      count = num_params + 1;

    n00b_zarray_new_cells(ctx->contents, (void **)&n, count);

    n00b_tnode_t *pptr = num_params ? n + 1 : NULL;

    if (node_ptr) {
        *node_ptr = n;
    }

    n[0] = (n00b_tnode_t){
        .params = pptr,
        .name   = name,
        .info   = (n00b_thash_payload_t){
              .num_params  = num_params,
              .base_typeid = base,
              .kind        = kind,
        }};

    if (params) {
        n00b_tnode_t *sub_node;

        for (uint32_t i = 0; i < num_params; i++) {
            n00b_ntype_t t = params[i];

            if (t < N00B_T_NUM_PRIMITIVE_BUILTINS
                || (t & N00B_THASH_OFFSET_BIT)) {
                sub_node = tnode_from_offset(ctx, t);
            }
            else {
                sub_node = reverse_lookup(ctx, t);
            }
            assert(sub_node);

            while (sub_node->forward) {
                sub_node = sub_node->forward;
            }

            pptr[i].forward = sub_node;
        }
    }

    return n00b_tnode_to_type(ctx, n);
}

n00b_ntype_t
n00b_type_list(n00b_ntype_t item_type)
{
    return n00b_type_instantiate(N00B_T_LIST,
                                 params : &item_type,
                                          num_params : 1);
}

n00b_ntype_t
n00b_type_set(n00b_ntype_t item_type)
{
    return n00b_type_instantiate(N00B_T_SET,
                                 params : &item_type,
                                          num_params : 1);
}

n00b_ntype_t
n00b_type_dict(n00b_ntype_t key_type, n00b_ntype_t val_type)
{
    n00b_ntype_t *p = n00b_gc_array_value_alloc(n00b_ntype_t, 2);
    p[0]            = key_type;
    p[1]            = val_type;

    return n00b_type_instantiate(N00B_T_DICT, params : p, num_params : 2);
}

n00b_ntype_t
n00b_type_tuple(n00b_list_t *type_list)
{
    return n00b_type_instantiate(N00B_T_TUPLE,
                                 params : type_list->data,
                                          num_params : type_list->length);
}

n00b_ntype_t
n00b_type_tree(n00b_ntype_t item_type)
{
    return n00b_type_instantiate(N00B_T_TREE,
                                 params : &item_type,
                                          num_params : 1);
}

n00b_ntype_t
n00b_type_fn(n00b_ntype_t retval, n00b_list_t *params, bool varargs)
{
    int           l = n00b_list_len(params) + 1;
    n00b_ntype_t *p = n00b_gc_array_value_alloc(n00b_ntype_t, l);

    for (int i = 0; i < l - 1; i++) {
        p[i] = (n00b_ntype_t)n00b_list_get(params, i, NULL);
    }
    p[l - 1] = retval;

    // clang-format off
    return n00b_type_instantiate(N00B_T_FUNCDEF,
				 params     : p,
				 num_params : l,
				 varargs    : varargs);
    // clang-format on
}

n00b_ntype_t
n00b_new_typevar(void)
{
    return n00b_type_instantiate(N00B_T_GENERIC, NULL);
}

n00b_ntype_t
n00b_new_named_typevar(char *name)
{
    n00b_ntype_t result          = n00b_new_typevar();
    n00b_get_tnode(result)->name = name;

    return result;
}

bool
_n00b_type_is_concrete(n00b_ntype_t t, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx     = n00b_get_runtime_universe();
        n00b_tnode_t   **nodeptr = NULL;
    }

    if ((t & N00B_THASH_OFFSET_BIT) || (t < N00B_T_NUM_PRIMITIVE_BUILTINS)) {
        return true;
    }

    n00b_tnode_t *n = reverse_lookup(ctx, t);

    assert(n);

    if (nodeptr) {
        *nodeptr = n;
    }

    return n->concrete;
}

static n00b_tnode_t *
node_copy(n00b_type_ctx_t *ctx, n00b_tnode_t *old)
{
    n00b_tnode_t *result;

    while (old->forward) {
        old = old->forward;
    }

    if (old->concrete) {
        return old;
    }

    bool va = old->info.kind == n00b_thasht_varargs_fn ? true : false;
    // clang-format off
    n00b_type_instantiate(old->info.base_typeid,
                          node_ptr : &result,
			  ctx : ctx,
			  num_params : old->info.num_params,
			  varargs : va);
    // clang-format on

    for (int i = 0; i < old->info.num_params; i++) {
        n00b_tnode_t *sub = &old->params[i];
        while (sub->forward) {
            sub = sub->forward;
        }
        if (sub->concrete) {
            memcpy(&result->params[i], sub, sizeof(n00b_tnode_t));
        }
        else {
            n00b_tnode_t *subcopy     = node_copy(ctx, sub);
            result->params[i].forward = subcopy;
        }
    }

    return result;
}

n00b_ntype_t
_n00b_type_copy(n00b_ntype_t t, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    n00b_tnode_t *oldt;

    if (n00b_type_is_concrete(t, ctx : ctx, nodeptr : &oldt)) {
        return t;
    }

    if (!oldt->info.num_params) {
        return n00b_type_instantiate(oldt->info.base_typeid, ctx : ctx);
    }

    n00b_tnode_t *new_node = node_copy(ctx, oldt);
    return n00b_tnode_to_type(ctx, new_node);
}

bool
_n00b_has_base_container_type(n00b_ntype_t t, n00b_builtin_t base, ...)
{
    keywords
    {
        n00b_type_ctx_t *ctx = n00b_get_runtime_universe();
    }

    n00b_tnode_t *tn = n00b_get_tnode(t, ctx : ctx);
    if (!tn) {
        return false;
    }

    return tn->info.base_typeid == base;
}

n00b_ntype_t
_n00b_get_promotion_type(n00b_ntype_t t1, n00b_ntype_t t2, ...)
{
    keywords
    {
        bool *warning = NULL;
    }

    if (warning) {
        *warning = false;
    }

    t1 = n00b_type_unbox(t1);
    t2 = n00b_type_unbox(t2);

    if (t1 == t2) {
        return t1;
    }

    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS
        || t2 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return N00B_T_ERROR;
    }

    n00b_dt_info_t *info1 = (void *)&n00b_base_type_info[t1];
    n00b_dt_info_t *info2 = (void *)&n00b_base_type_info[t2];

    if (!info1->int_bits || !info2->int_bits) {
        return N00B_T_ERROR;
    }

    if (info1->sign == info2->sign) {
        if (info1->int_bits > info2->int_bits) {
            return t2;
        }
        return t1;
    }

    n00b_ntype_t   *signed_input;
    n00b_dt_info_t *signed_info;
    n00b_dt_info_t *unsigned_info;

    if (info1->sign) {
        signed_input  = &t1;
        signed_info   = info1;
        unsigned_info = info2;
    }
    else {
        signed_input  = &t2;
        signed_info   = info2;
        unsigned_info = info1;
    }

    // If we have a signed + unsigned, we want to always auto-promote
    // to signed. If the unsigned value is the same size or larger
    // than the unsigned value, we'll want to go up a size, to make
    // sure there's no loss of information.
    //
    // The only issue is when we reach our max size, and can no
    // longer promote up. There, if we have a sign mismatch, we
    // will go w/ 'unsigned', but that merits a warning because
    // our value might truncate.

    if (signed_info->int_bits > unsigned_info->int_bits) {
        return *signed_input;
    }

    while (signed_info->int_bits <= unsigned_info->int_bits) {
        if (!signed_info->promote_to) {
            if (warning) {
                *warning = true;
            }
            return *signed_input;
        }
        *signed_input = signed_info->promote_to;
        signed_info   = (void *)&n00b_base_type_info[*signed_input];
    }

    return *signed_input;
}

n00b_type_exact_result_t
n00b_type_cmp_exact(n00b_ntype_t t1, n00b_ntype_t t2)
{
    if (n00b_type_is_box(t1)) {
        t1 = n00b_type_unbox(t1);
    }
    if (n00b_type_is_box(t2)) {
        t2 = n00b_type_unbox(t2);
    }

    if (t1 == t2) {
        return n00b_type_match_exact;
    }

    if (t1 == N00B_T_ERROR || t2 == N00B_T_ERROR) {
        return n00b_type_cant_match;
    }

    n00b_tnode_t *node1 = n00b_get_tnode(t1);
    n00b_tnode_t *node2 = n00b_get_tnode(t2);

    if (node1->info.base_typeid != node2->info.base_typeid) {
        if (node1->info.kind == n00b_thasht_typevar) {
            return n00b_type_match_right_more_specific;
        }
        if (node2->info.kind == n00b_thasht_typevar) {
            return n00b_type_match_left_more_specific;
        }
        return n00b_type_cant_match;
    }

    if (n00b_type_is_int_type(t1) && n00b_type_is_int_type(t2)) {
        return n00b_type_match_promoted;
    }

    if (node1->info.kind != node2->info.kind) {
        return n00b_type_cant_match;
    }

    int  n1    = n00b_type_get_num_params(t1);
    int  n2    = n00b_type_get_num_params(t2);
    bool err   = false;
    bool left  = false;
    bool right = false;

    if (n1 != n2) {
        return n00b_type_cant_match;
    }

    for (int i = 0; i < n1; i++) {
        n00b_list_t *p1;
        n00b_list_t *p2;
        n00b_ntype_t sub1;
        n00b_ntype_t sub2;

        p1   = n00b_type_get_params(t1);
        p2   = n00b_type_get_params(t2);
        sub1 = n00b_get_type(n00b_list_get(p1, i, NULL));
        sub2 = n00b_get_type(n00b_list_get(p2, i, NULL));

        switch (n00b_type_cmp_exact(sub1, sub2)) {
        case n00b_type_match_exact:
        case n00b_type_match_promoted:
            continue;
        case n00b_type_cant_match:
            return n00b_type_cant_match;
        case n00b_type_match_left_more_specific:
            err  = true;
            left = true;
            continue;
        case n00b_type_match_right_more_specific:
            err   = true;
            right = true;
            continue;
        case n00b_type_match_both_have_more_generic_bits:
            err   = true;
            left  = true;
            right = true;
            continue;
        }

        if (!err) {
            return n00b_type_match_exact;
        }
        if (left && right) {
            return n00b_type_match_both_have_more_generic_bits;
        }
        if (left) {
            return n00b_type_match_left_more_specific;
        }
        else { // right only
            return n00b_type_match_right_more_specific;
        }
    }

    return n00b_type_match_exact;
}

static n00b_string_t *
n00b_typespec_to_lit(void *ts)
{
    n00b_ntype_t t;

    if (n00b_in_heap(ts)) {
        t = *(n00b_ntype_t *)ts;
    }
    else {
        t = (n00b_ntype_t)ts;
    }

    return n00b_ntype_get_name(t);
}

#if 0
static n00b_ntype_t
n00b_typespec_from_lit(n00b_string_t        *s,
                       n00b_lit_syntax_t     st,
                       n00b_string_t        *litmod,
                       n00b_compile_error_t *err)
{
    // TODO: call the parsing function.
}
#endif

static inline void
n00b_typespec_init(n00b_ntype_t *typeptr, va_list args)
{
    *typeptr = va_arg(args, n00b_ntype_t);
}

const n00b_vtable_t n00b_typespec_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_typespec_init,
        [N00B_BI_TO_LITERAL]  = (n00b_vtable_entry)n00b_typespec_to_lit,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_typespec_to_lit,
        //[N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)n00b_typespec_from_lit,
    }};
