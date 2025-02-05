#include "n00b.h"

// Different queue/list types.

static void
n00b_flexarray_init(flexarray_t *list, va_list args)
{
    int64_t length = 0;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);

    flexarray_init(list, length);
}

static void
n00b_queue_init(queue_t *list, va_list args)
{
    int64_t length = 0;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);

    queue_init_size(list, (char)(64 - __builtin_clzll(length)));
}

static void
n00b_ring_init(hatring_t *ring, va_list args)
{
    int64_t length = 0;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);

    hatring_init(ring, length);
}

static void
n00b_logring_init(logring_t *ring, va_list args)
{
    int64_t num_entries  = 0;
    int64_t entry_length = 1024;

    n00b_karg_va_init(args);
    n00b_kw_int64("num_entries", num_entries);
    n00b_kw_int64("entry_length", entry_length);

    logring_init(ring, num_entries, entry_length);
}

static void
n00b_stack_init(hatstack_t *stack, va_list args)
{
    int64_t prealloc = 128;

    n00b_karg_va_init(args);
    n00b_kw_int64("prealloc", prealloc);

    hatstack_init(stack, prealloc);
}

bool
n00b_flexarray_can_coerce_to(n00b_type_t *my_type, n00b_type_t *dst_type)
{
    n00b_dt_kind_t base = n00b_type_get_kind(dst_type);

    if (base == (n00b_dt_kind_t)N00B_T_BOOL) {
        return true;
    }

    // clang-format off
    if (base == (n00b_dt_kind_t)N00B_T_FLIST ||
	base == (n00b_dt_kind_t)N00B_T_LIST) {
        // clang-format on
        n00b_type_t *my_item  = n00b_type_get_param(my_type, 0);
        n00b_type_t *dst_item = n00b_type_get_param(dst_type, 0);

        return n00b_can_coerce(my_item, dst_item);
    }

    return false;
}

static n00b_obj_t
n00b_flexarray_coerce_to(flexarray_t *list, n00b_type_t *dst_type)
{
    n00b_dt_kind_t base          = n00b_type_get_kind(dst_type);
    flex_view_t   *view          = flexarray_view(list);
    int64_t        len           = flexarray_view_len(view);
    n00b_type_t   *src_item_type = n00b_type_get_param(n00b_get_my_type(list), 0);
    n00b_type_t   *dst_item_type = n00b_type_get_param(dst_type, 0);

    if (base == (n00b_dt_kind_t)N00B_T_BOOL) {
        return (n00b_obj_t)(int64_t)(flexarray_view_len(view) != 0);
    }

    if (base == (n00b_dt_kind_t)N00B_T_FLIST) {
        flexarray_t *res = n00b_new(dst_type, n00b_kw("length", n00b_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = flexarray_view_next(view, NULL);
            flexarray_set(res,
                          i,
                          n00b_coerce(item, src_item_type, dst_item_type));
        }

        return (n00b_obj_t)res;
    }

    n00b_list_t *res = n00b_new(dst_type, n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = flexarray_view_next(view, NULL);
        n00b_list_set(res, i, n00b_coerce(item, src_item_type, dst_item_type));
    }

    return (n00b_obj_t)res;
}

static flexarray_t *
n00b_flexarray_copy(flexarray_t *list)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    n00b_type_t *myty = n00b_get_my_type(list);

    flexarray_t *res    = n00b_new(myty, n00b_kw("length", n00b_ka(len)));
    n00b_type_t *itemty = n00b_type_get_param(myty, 0);

    // itemty = n00b_resolve_type(itemty);

    for (int i = 0; i < len; i++) {
        n00b_obj_t item = flexarray_view_next(view, NULL);
        flexarray_set(res, i, n00b_copy_object_of_type(item, itemty));
    }

    return res;
}

static void *
n00b_flexarray_get(flexarray_t *list, int64_t index)
{
    n00b_obj_t result;
    int        status;

    if (index < 0) {
        // Thing might get resized, so we have to take a view.
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        index += len;

        if (index < 0) {
            n00b_string_t *err = n00b_cformat(
                "Array index out of bounds "
                "(ix = «#»; size = «#»)",
                (int64_t)index,
                (int64_t)len);

            N00B_RAISE(err);
        }

        result = flexarray_view_get(view, index, &status);
    }
    else {
        result = flexarray_get(list, index, &status);
    }

    if (status == FLEX_OK) {
        return result;
    }

    if (status == FLEX_UNINITIALIZED) {
        N00B_CRAISE("Array access is for uninitialized value.");
    }
    else {
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        n00b_string_t *err = n00b_cformat(
            "Array index out of bounds "
            "(ix = «#»; size = «#»)",
            (int64_t)index,
            (int64_t)len);

        N00B_RAISE(err);
    }
}

static void
n00b_flexarray_set(flexarray_t *list, int64_t ix, void *item)
{
    if (!flexarray_set(list, ix, item)) {
        flex_view_t *view = flexarray_view(list);
        int64_t      len  = flexarray_view_len(view);

        n00b_string_t *err = n00b_cformat(
            "Array index out of bounds (ix = «#»)",
            (int64_t)ix,
            (int64_t)len);
        N00B_RAISE(err);
    }
}

static flexarray_t *
n00b_flexarray_get_slice(flexarray_t *list, int64_t start, int64_t end)
{
    flex_view_t *view = flexarray_view(list);
    int64_t      len  = flexarray_view_len(view);
    flexarray_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(0)));
        }
    }
    if (end < 0) {
        end += len;
    }
    else {
        if (end > len) {
            end = len;
        }
    }

    if ((start | end) < 0 || start >= end) {
        return n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(0)));
    }

    len = end - start;
    res = n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = flexarray_view_get(view, start + i, NULL);
        flexarray_set(res, i, item);
    }

    return res;
}

// Semantics are, take a moment-in time view of each list (not the same moment),
// Compute the result, then replace the list. Definitely not atomic.
static void
n00b_flexarray_set_slice(flexarray_t *list, int64_t start, int64_t end, flexarray_t *new)
{
    flex_view_t *view1 = flexarray_view(list);
    flex_view_t *view2 = flexarray_view(new);
    int64_t      len1  = flexarray_view_len(view1);
    int64_t      len2  = flexarray_view_len(view2);
    flexarray_t *tmp;

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            N00B_CRAISE("Out of bounds slice.");
        }
    }
    if (end < 0) {
        end += len1;
    }
    else {
        if (end > len1) {
            end = len1;
        }
    }

    if ((start | end) < 0 || start >= end) {
        N00B_CRAISE("Out of bounds slice.");
    }

    int64_t slicelen = end - start;
    int64_t newlen   = len1 + len2 - slicelen;

    tmp = n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(newlen)));

    if (start > 0) {
        for (int i = 0; i < start; i++) {
            void *item = flexarray_view_get(view1, i, NULL);
            flexarray_set(tmp, i, item);
        }
    }

    for (int i = 0; i < len2; i++) {
        void *item = flexarray_view_get(view2, i, NULL);
        flexarray_set(tmp, start++, item);
    }

    for (int i = end; i < len1; i++) {
        void *item = flexarray_view_get(view1, i, NULL);
        flexarray_set(tmp, start++, item);
    }

    atomic_store(&list->store, atomic_load(&tmp->store));
}

static n00b_string_t *
n00b_flexarray_repr(flexarray_t *list)
{
    flex_view_t *view  = flexarray_view(list);
    int64_t      len   = flexarray_view_len(view);
    n00b_list_t *items = n00b_new(n00b_type_list(n00b_type_string()));

    for (int i = 0; i < len; i++) {
        int   err  = 0;
        void *item = flexarray_view_get(view, i, &err);
        if (err) {
            continue;
        }

        n00b_string_t *s = n00b_repr(item);
        n00b_list_append(items, s);
    }

    n00b_string_t *sep    = n00b_cached_comma();
    n00b_string_t *result = n00b_string_join(items, sep);

    result = n00b_string_concat(n00b_cached_lbracket(),
                                n00b_string_concat(result, n00b_cached_rbracket()));

    return result;
}

static flexarray_t *
n00b_to_flexarray_lit(n00b_type_t *objtype, n00b_list_t *items, n00b_string_t *litmod)
{
    uint64_t     n      = n00b_list_len(items);
    flexarray_t *result = n00b_new(objtype, n00b_kw("length", n00b_ka(n)));

    for (unsigned int i = 0; i < n; i++) {
        n00b_flexarray_set(result, i, n00b_list_get(items, i, NULL));
    }

    return result;
}

const n00b_vtable_t n00b_flexarray_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)n00b_flexarray_init,
        [N00B_BI_FINALIZER]     = (n00b_vtable_entry)flexarray_cleanup,
        [N00B_BI_TO_STR]        = (n00b_vtable_entry)n00b_flexarray_repr,
        [N00B_BI_COERCIBLE]     = (n00b_vtable_entry)n00b_flexarray_can_coerce_to,
        [N00B_BI_COERCE]        = (n00b_vtable_entry)n00b_flexarray_coerce_to,
        [N00B_BI_COPY]          = (n00b_vtable_entry)n00b_flexarray_copy,
        [N00B_BI_ADD]           = (n00b_vtable_entry)flexarray_add,
        [N00B_BI_LEN]           = (n00b_vtable_entry)flexarray_len,
        [N00B_BI_INDEX_GET]     = (n00b_vtable_entry)n00b_flexarray_get,
        [N00B_BI_INDEX_SET]     = (n00b_vtable_entry)n00b_flexarray_set,
        [N00B_BI_SLICE_GET]     = (n00b_vtable_entry)n00b_flexarray_get_slice,
        [N00B_BI_SLICE_SET]     = (n00b_vtable_entry)n00b_flexarray_set_slice,
        [N00B_BI_VIEW]          = (n00b_vtable_entry)flexarray_view,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)n00b_to_flexarray_lit,
        [N00B_BI_GC_MAP]        = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};

const n00b_vtable_t n00b_queue_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_queue_init,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)queue_cleanup,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};

const n00b_vtable_t n00b_ring_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_ring_init,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)hatring_cleanup,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};

const n00b_vtable_t n00b_logring_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_logring_init,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)logring_cleanup,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};

const n00b_vtable_t n00b_stack_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_stack_init,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)hatstack_cleanup,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};
