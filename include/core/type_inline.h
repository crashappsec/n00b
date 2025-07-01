#include "n00b.h"

static inline n00b_ntype_t
n00b_type_resolve(n00b_ntype_t t)
{
    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return t;
    }

    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    if (!tn) {
        return N00B_T_ERROR;
    }

    while (tn->forward) {
        tn = tn->forward;
    }

    return _n00b_get_type(tn, NULL);
}

static inline n00b_dt_kind_t
n00b_type_get_kind(n00b_ntype_t t)
{
    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);

    while (tn->forward) {
        tn = tn->forward;
    }

    return n00b_base_type_info[tn->info.base_typeid].dt_kind;
}

static inline int
n00b_type_get_num_params(n00b_ntype_t t)
{
    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return 0;
    }

    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);

    return tn->info.num_params;
}

static inline n00b_ntype_t
n00b_type_get_last_param(n00b_ntype_t t)
{
    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS || t < 0) {
        return N00B_T_ERROR;
    }

    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    if (!tn || !tn->info.num_params) {
        return N00B_T_ERROR;
    }

    n00b_tnode_t *result = &tn->params[tn->info.num_params - 1];
    while (result->forward) {
        result = result->forward;
    }

    return n00b_get_type(result);
}

static inline n00b_ntype_t
n00b_type_get_param(n00b_ntype_t t, int n)
{
    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS || n < 0) {
        return N00B_T_ERROR;
    }

    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);

    if (!tn || n >= tn->info.num_params) {
        return N00B_T_ERROR;
    }

    n00b_tnode_t *result = &tn->params[n];
    while (result->forward) {
        result = result->forward;
    }

    return n00b_get_type(result);
}

static inline n00b_list_t *
n00b_type_get_params(n00b_ntype_t t)
{
    n00b_list_t  *result = n00b_list(N00B_T_TYPESPEC);
    n00b_tnode_t *tn     = _n00b_get_tnode(t, NULL);

    if (!tn) {
        return result;
    }

    while (tn->forward) {
        tn = tn->forward;
    }

    for (int i = 0; i < tn->info.num_params; i++) {
        n00b_tnode_t *item = &tn->params[i];

        while (item->forward) {
            item = item->forward;
        }

        n00b_list_append(result, item);
    }

    return result;
}

static inline n00b_ntype_t
n00b_tprimitive(int32_t id)
{
    assert(id >= 0 && id < N00B_T_NUM_PRIMITIVE_BUILTINS);
    return (n00b_ntype_t)id;
}

static inline bool
n00b_runtime_type_check(n00b_ntype_t t1, n00b_ntype_t t2)
{
    return t1 == t2;
}

static inline bool
n00b_type_is_int_type(n00b_ntype_t t1)
{
    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }

    return n00b_base_type_info[t1].int_bits != 0;
}

static inline bool
n00b_type_is_float_type(n00b_ntype_t t)
{
    if (t == N00B_T_F32 || t == N00B_T_F64) {
        return true;
    }

    return false;
}

static inline bool
n00b_type_is_signed(n00b_ntype_t t1)
{
    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }

    return n00b_base_type_info[t1].int_bits & 1;
}

static inline bool
n00b_type_is_box(n00b_ntype_t t1)
{
    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }

    return n00b_base_type_info[t1].unbox_id != N00B_T_ERROR;
}

static inline bool
n00b_type_is_mutable(n00b_ntype_t t1)
{
    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return true;
    }

    return n00b_base_type_info[t1].mutable;
}

static inline bool
n00b_type_is_value_type(n00b_ntype_t t1)
{
    if (t1 >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }

    return n00b_base_type_info[t1].by_value;
}

static inline bool
n00b_obj_item_type_is_value(void *obj)
{
    n00b_ntype_t t = n00b_get_my_type(obj);
    t              = n00b_type_get_param(t, 0);

    return n00b_type_is_value_type(t);
}

static inline bool
n00b_obj_is_int_type(void *obj)
{
    return n00b_type_is_int_type(n00b_get_my_type(obj));
}

static inline int
n00b_type_alloc_len(n00b_ntype_t t)
{
    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    while (tn->forward) {
        tn = tn->forward;
    }

    return n00b_base_type_info[tn->info.base_typeid].alloc_len;
}

static inline n00b_dt_kind_t
n00b_type_get_container_kind(n00b_ntype_t t)
{
    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    while (tn->forward) {
        tn = tn->forward;
    }

    assert(t > N00B_T_NUM_PRIMITIVE_BUILTINS && t < N00B_NUM_BUILTIN_DTS);

    return n00b_base_type_info[tn->info.base_typeid].dt_kind;
}

static inline n00b_ntype_t
n00b_get_base_type(n00b_ntype_t t)
{
    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    while (tn->forward) {
        tn = tn->forward;
    }

    return tn->info.base_typeid;
}

static inline int
n00b_type_num_params(n00b_ntype_t t)
{
    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }
    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
    return tn->info.num_params;
}

static inline n00b_list_t *
n00b_type_all_params(n00b_ntype_t t)
{
    n00b_list_t *result = n00b_list(n00b_type_ref());

    if (t <= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return result;
    }

    n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);

    for (int i = 0; i < tn->info.num_params; i++) {
        n00b_tnode_t *pnode = tn->params + 1;
        while (pnode->forward) {
            pnode = pnode->forward;
        }

        n00b_private_list_append(result, pnode);
    }

    return result;
}

static inline n00b_ntype_t
n00b_type_box(n00b_ntype_t t)
{
    if (t >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return N00B_T_ERROR;
    }

    n00b_ntype_t base = n00b_base_type_info[t].box_id;

    if (base) {
        return base;
    }

    return t;
}

static inline n00b_ntype_t
n00b_type_unbox(n00b_ntype_t t)
{
    if (t >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        n00b_tnode_t *tn = _n00b_get_tnode(t, NULL);
        if (!tn) {
            return N00B_T_ERROR;
        }
        while (tn->forward) {
            tn = tn->forward;
        }

        t = tn->info.base_typeid;

        if (t >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
            return t;
        }
    }

    n00b_ntype_t base = n00b_base_type_info[t].unbox_id;

    if (base) {
        return base;
    }

    return t;
}

static inline bool
n00b_type_requires_gc_scan(n00b_ntype_t t)
{
    if (t >= N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return true;
    }

    return !n00b_base_type_info[t].by_value;
}

static inline bool
n00b_type_is_tvar(n00b_ntype_t t)
{
    if (t < N00B_T_NUM_PRIMITIVE_BUILTINS) {
        return false;
    }

    n00b_tnode_t *n = _n00b_get_tnode(t, NULL);
    return n->info.kind == n00b_thasht_typevar;
}

static inline bool
n00b_types_are_compat(n00b_ntype_t t1, n00b_ntype_t t2, int *warn)
{
    // TODO: Coercion
    return n00b_types_can_unify(t1, t2);
}

#include "core/builtin_tinfo_gen.h"
