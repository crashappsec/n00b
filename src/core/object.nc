#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_string_t *
nil_repr(void *ignored)
{
    return n00b_cstring("0");
}

const n00b_vtable_t n00b_type_nil_vtable = {
    .methods = {
        [N00B_BI_TO_STRING] = (n00b_vtable_entry)nil_repr,
    },

};

typedef uint64_t (*n00b_obj_size_helper)(void *);

#if defined(N00B_ADD_ALLOC_LOC_INFO)
void *
_n00b_new(n00b_heap_t *heap, char *file, int line, n00b_ntype_t type, ...)
#else
void *
_n00b_new(n00b_heap_t *heap, n00b_ntype_t type, ...)
#endif
{
    va_list args;
    va_start(args, type);

    void *obj;

    n00b_dt_info_t *tinfo     = n00b_get_data_type_info(type);
    uint64_t        alloc_len = tinfo->alloc_len;

    if (alloc_len == N00B_VARIABLE_ALLOC_SZ) {
        va_list              argcopy;
        void                *param;
        n00b_obj_size_helper helper;

        va_copy(argcopy, args);
        param = va_arg(argcopy, void *);
        va_end(argcopy);

        helper    = (void *)tinfo->vtable->methods[N00B_BI_ALLOC_SZ];
        alloc_len = (*helper)(param);
    }

    if (!tinfo->vtable) {
        obj = n00b_gc_raw_alloc(alloc_len, N00B_GC_SCAN_ALL);

        va_end(args);
        return obj;
    }

    n00b_vtable_entry init_fn = tinfo->vtable->methods[N00B_BI_CONSTRUCTOR];
    n00b_vtable_entry scan_fn = tinfo->vtable->methods[N00B_BI_GC_MAP];

#if defined(N00B_MPROTECT_GUARD_ALLOCS)
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();

    if (tsi && tsi->add_guard) {
        tsi->add_guard = false;
        obj            = _n00b_heap_alloc(n00b_current_heap(heap),
                               alloc_len,
                               true,
                               (n00b_mem_scan_fn)scan_fn
                                   N00B_ALLOC_XPARAM);
    }
    else {
        obj = _n00b_heap_alloc(heap,
                               alloc_len,
                               false,
                               (n00b_mem_scan_fn)scan_fn
                                   N00B_ALLOC_XPARAM);
    }
#else
    obj = _n00b_heap_alloc(n00b_current_heap(heap),
                           alloc_len,
                           false,
                           (n00b_mem_scan_fn)scan_fn,
                           N00B_ALLOC_XPARAM);
#endif

    n00b_alloc_hdr *hdr = (void *)obj;
    --hdr;
    hdr->n00b_obj = true;
    hdr->type     = type;

    if (tinfo->vtable->methods[N00B_BI_FINALIZER] == NULL) {
        hdr->n00b_finalize = true;
    }

    switch (tinfo->dt_kind) {
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
    case N00B_DT_KIND_object:
    case N00B_DT_KIND_box:
        if (init_fn != NULL) {
            (*init_fn)(obj, args);
            va_end(args);
        }
        break;
    default:

        N00B_CRAISE(
            "Requested type is non-instantiable or not yet "
            "implemented.");
    }

    return obj;
}

void *
n00b_copy(void *obj)
{
    if (!n00b_in_heap(obj)) {
        return obj;
    }
    void           *result;
    n00b_alloc_hdr *hdr = (void *)obj;

    hdr--;

    // If it's a n00b obj, it looks for a copy constructor,
    // or else returns null.
    //
    // If it's in-heap, it makes a deep copy via
    // automarshal.

    if (hdr->guard == n00b_gc_guard) {
        n00b_copy_fn fn = (n00b_copy_fn)n00b_vtable(obj)->methods[N00B_BI_COPY];
        if (fn != NULL) {
            result = (*fn)(obj);
            n00b_restore(result);
            return result;
        }
        return NULL;
    }

    if (!n00b_in_heap(obj)) {
        return obj;
    }

    return n00b_autounmarshal(n00b_automarshal(obj));
}

void *
n00b_shallow(void *obj)
{
    if (!n00b_in_heap(obj)) {
        return obj;
    }

    n00b_copy_fn fn = (void *)n00b_vtable(obj)->methods[N00B_BI_SHALLOW_COPY];

    if (fn == NULL) {
        N00B_CRAISE("Object type currently does not have a shallow copy fn.");
    }
    return (*fn)(obj);
}

void *
n00b_copy_object_of_type(void *obj, n00b_ntype_t t)
{
    if (n00b_type_is_value_type(t)) {
        return obj;
    }

    n00b_copy_fn ptr = (n00b_copy_fn)n00b_vtable(obj)->methods[N00B_BI_COPY];

    if (ptr == NULL) {
        N00B_CRAISE("Copying for this object type not currently supported.");
    }

    return (*ptr)(obj);
}

void *
n00b_add(void *lhs, void *rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_ADD];

    if (ptr == NULL) {
        N00B_CRAISE("Addition not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

void *
n00b_sub(void *lhs, void *rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_SUB];

    if (ptr == NULL) {
        N00B_CRAISE("Subtraction not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

void *
n00b_mul(void *lhs, void *rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_MUL];

    if (ptr == NULL) {
        N00B_CRAISE("Multiplication not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

void *
n00b_div(void *lhs, void *rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_DIV];

    if (ptr == NULL) {
        N00B_CRAISE("Division not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

void *
n00b_mod(void *lhs, void *rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_MOD];

    if (ptr == NULL) {
        N00B_CRAISE("Modulus not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

int64_t
n00b_len(void *container)
{
    if (!container) {
        return 0;
    }

    n00b_len_fn ptr = (n00b_len_fn)n00b_vtable(container)->methods[N00B_BI_LEN];

    if (ptr == NULL) {
        N00B_CRAISE("Cannot call len on a non-container.");
    }

    return (*ptr)(container);
}

void *
n00b_index_get(void *container, void *index)
{
    n00b_index_get_fn ptr;

    ptr = (n00b_index_get_fn)n00b_vtable(container)->methods[N00B_BI_INDEX_GET];

    if (ptr == NULL) {
        N00B_CRAISE("No index operation available.");
    }

    return (*ptr)(container, index);
}

void
n00b_index_set(void *container, void *index, void *value)
{
    n00b_index_set_fn ptr;

    ptr = (n00b_index_set_fn)n00b_vtable(container)->methods[N00B_BI_INDEX_SET];

    if (ptr == NULL) {
        N00B_CRAISE("No index assignment operation available.");
    }

    (*ptr)(container, index, value);
}

void *
n00b_slice_get(void *container, int64_t start, int64_t end)
{
    n00b_slice_get_fn ptr;

    ptr = (n00b_slice_get_fn)n00b_vtable(container)->methods[N00B_BI_SLICE_GET];

    if (ptr == NULL) {
        N00B_CRAISE("No slice operation available.");
    }

    return (*ptr)(container, start, end);
}

void
n00b_slice_set(void *container, int64_t start, int64_t end, void *o)
{
    n00b_slice_set_fn ptr;

    ptr = (n00b_slice_set_fn)n00b_vtable(container)->methods[N00B_BI_SLICE_SET];

    if (ptr == NULL) {
        N00B_CRAISE("No slice assignment operation available.");
    }

    (*ptr)(container, start, end, o);
}

bool
n00b_can_coerce(n00b_ntype_t t1, n00b_ntype_t t2)
{
    if (n00b_types_are_compat(t1, t2, NULL)) {
        return true;
    }

    n00b_dt_info_t    *info = n00b_get_data_type_info(t1);
    n00b_vtable_t     *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_can_coerce_fn ptr  = (n00b_can_coerce_fn)vtbl->methods[N00B_BI_COERCIBLE];

    if (ptr == NULL) {
        return false;
    }

    return (*ptr)(t1, t2);
}

void *
n00b_coerce(void *data, n00b_ntype_t t1, n00b_ntype_t t2)
{
    t1 = n00b_type_unbox(t1);
    t2 = n00b_type_unbox(t2);

    n00b_dt_info_t *info = n00b_get_data_type_info(t1);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;

    n00b_coerce_fn ptr = (n00b_coerce_fn)vtbl->methods[N00B_BI_COERCE];

    if (ptr == NULL) {
        N00B_CRAISE("Invalid conversion between types.");
    }

    return (*ptr)(data, t2);
}

void *
n00b_coerce_object(const void *obj, n00b_ntype_t to_type)
{
    n00b_ntype_t    from_type = n00b_get_my_type((void *)obj);
    n00b_dt_info_t *info      = n00b_get_data_type_info(from_type);
    uint64_t        value;

    if (!info->by_value) {
        return n00b_coerce((void *)obj, from_type, to_type);
    }

    switch (info->alloc_len) {
    case 8:
        value = (uint64_t) * (uint8_t *)obj;
        break;
    case 32:
        value = (uint64_t) * (uint32_t *)obj;
        break;
    default:
        value = *(uint64_t *)obj;
    }

    value        = (uint64_t)n00b_coerce((void *)value, from_type, to_type);
    info         = n00b_get_data_type_info(to_type);
    void *result = n00b_new(to_type);

    if (info->alloc_len == 8) {
        *(uint8_t *)result = (uint8_t)value;
    }
    else {
        if (info->alloc_len == 32) {
            *(uint32_t *)result = (uint32_t)value;
        }
        else {
            *(uint64_t *)result = value;
        }
    }

    return result;
}

bool
n00b_eq(n00b_ntype_t t, void *o1, void *o2)
{
    n00b_dt_info_t *info = n00b_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_EQ];

    if (!ptr) {
        return o1 == o2;
    }

    return (*ptr)(o1, o2);
}

// Will mostly replace n00b_eq at some point soon.
bool
n00b_equals(void *o1, void *o2)
{
    n00b_ntype_t t1 = n00b_get_my_type(o1);
    n00b_ntype_t t2 = n00b_get_my_type(o2);
    n00b_ntype_t m  = n00b_unify(t1, t2);

    if (n00b_type_is_error(m)) {
        return false;
    }
    if (!n00b_in_heap(o1) || !n00b_in_heap(o2)) {
        return o1 == o2;
    }

    n00b_dt_info_t *info = n00b_get_data_type_info(m);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_EQ];

    if (!ptr) {
        return o1 == o2;
    }

    return (*ptr)(o1, o2);
}

bool
n00b_lt(n00b_ntype_t t, void *o1, void *o2)
{
    n00b_dt_info_t *info = n00b_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_LT];

    if (!ptr) {
        return o1 < o2;
    }

    return (*ptr)(o1, o2);
}

bool
n00b_gt(n00b_ntype_t t, void *o1, void *o2)
{
    n00b_dt_info_t *info = n00b_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_GT];

    if (!ptr) {
        return o1 > o2;
    }

    return (*ptr)(o1, o2);
}

n00b_ntype_t
n00b_get_item_type(void *obj)
{
    n00b_ntype_t       t    = n00b_get_my_type(obj);
    n00b_dt_info_t    *info = n00b_get_data_type_info(t);
    n00b_vtable_t     *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_ix_item_ty_fn ptr  = (n00b_ix_item_ty_fn)vtbl->methods[N00B_BI_ITEM_TYPE];

    if (!ptr) {
        if (n00b_type_get_num_params(t) == 1) {
            return n00b_type_get_param(t, 0);
        }

        n00b_string_t *err = n00b_cformat(
            "Type «#» is not an indexible container type.",
            t);
        N00B_RAISE(err);
    }

    return (*ptr)(t);
}

void *
n00b_get_view(void *obj, int64_t *n_items)
{
    n00b_ntype_t    t         = n00b_get_my_type(obj);
    n00b_dt_info_t *info      = n00b_get_data_type_info(t);
    n00b_vtable_t  *vtbl      = (n00b_vtable_t *)info->vtable;
    void           *itf       = vtbl->methods[N00B_BI_ITEM_TYPE];
    n00b_view_fn    ptr       = (n00b_view_fn)vtbl->methods[N00B_BI_VIEW];
    uint64_t        size_bits = 0x3;

    // If no item type callback is provided, we just assume 8 bytes
    // are produced.  In fact, I currently am not providing callbacks
    // for list types or dicts, since their views are always 64 bits;
    // probably should change the builtin to use an interface that
    // gives back bits, not types (and delete the internal one-bit
    // type).

    if (itf) {
        n00b_ntype_t    item_type = n00b_get_item_type(obj);
        n00b_dt_info_t *base      = n00b_get_data_type_info(item_type);

        if (base->typeid == N00B_T_BIT) {
            size_bits = 0x7;
        }
        else {
            size_bits = n00b_int_log2((uint64_t)base->alloc_len);
        }
    }

    uint64_t ret_as_int = (uint64_t)(*ptr)(obj, (uint64_t *)n_items);

    ret_as_int |= size_bits;
    return (void *)ret_as_int;
}

void *
n00b_container_literal(n00b_ntype_t t, n00b_list_t *items, n00b_string_t *mod)
{
    n00b_dt_info_t       *info = n00b_get_data_type_info(t);
    n00b_vtable_t        *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_container_lit_fn ptr;

    ptr = (n00b_container_lit_fn)vtbl->methods[N00B_BI_CONTAINER_LIT];

    if (ptr == NULL) {
        n00b_string_t *err = n00b_cformat(
            "Improper implementation; no literal fn "
            "defined for type '«#»'.",
            n00b_cstring(info->name));
        N00B_RAISE(err);
    }

    void *result = (*ptr)(t, items, mod);

    if (result == NULL) {
        n00b_string_t *err = n00b_cformat(
            "Improper implementation; type '«#»' did not instantiate "
            "a literal for the literal modifier '«#»'",
            n00b_cstring(info->name),
            mod);
        N00B_RAISE(err);
    }

    return result;
}

void
n00b_finalize_allocation(void *object)
{
    n00b_system_finalizer_fn fn;
    return;

    fn = (void *)n00b_vtable(object)->methods[N00B_BI_FINALIZER];
    if (fn == NULL) {
    }
    n00b_assert(fn != NULL);
    (*fn)(object);
}

void *
n00b_autobox(void *ptr)
{
    if (!n00b_in_heap(ptr)) {
        return n00b_box_u64((int64_t)ptr);
    }

    n00b_alloc_hdr *h = ptr;
    --h;

    if (h->guard != n00b_gc_guard) {
        // It's a pointer to the middle of some data structure.
        // TODO here is to check.
        //
        // to see if it seems to be a pointer to something else,
        // and if so, we should wrap it as a reference.

        void **result = n00b_new(n00b_type_ref());
        *result       = ptr;
        return result;
    }
    else {
        return ptr;
    }
}

n00b_ntype_t
n00b_get_my_type(void *user_object)
{
    if (n00b_in_heap(user_object)) {
        n00b_alloc_hdr *h = (void *)user_object;
        h -= 1;
        if (h->guard == n00b_gc_guard) {
            return h->type;
        }
        else {
            return n00b_type_internal();
        }
    }
    if (user_object == NULL) {
        return n00b_type_nil();
    }
    return n00b_type_i64();
}

n00b_list_t *
n00b_render(void *object, int64_t width, int64_t height)
{
    if (!n00b_is_renderable(object)) {
        N00B_CRAISE("Object cannot be rendered directly.");
    }

    n00b_vtable_t *vt = n00b_vtable(object);
    n00b_render_fn fn = (void *)vt->methods[N00B_BI_RENDER];

    return (*fn)(object, width, height);
}
