#include "n00b.h"

static void
n00b_list_init(n00b_list_t *list, va_list args)
{
    int64_t length = 16;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);

    list->append_ix    = 0;
    list->length       = n00b_max(length, 16);
    list->dont_acquire = false;
    pthread_rwlock_init(&list->lock, NULL);

    n00b_type_t *t = n00b_get_my_type(list);

    if (n00b_type_requires_gc_scan(n00b_type_get_param(t, 0))) {
        list->data = n00b_gc_array_alloc(uint64_t *, list->length);
    }
    else {
        list->data = n00b_gc_array_value_alloc(uint64_t *, list->length);
    }
}

static void
n00b_list_restore(n00b_list_t *list)
{
    list->dont_acquire = false;
    pthread_rwlock_init(&list->lock, NULL);
}

void
n00b_list_resize(n00b_list_t *list, size_t len)
{
    if (!list->dont_acquire) {
        pthread_rwlock_wrlock(&list->lock);
    }

    int64_t **old = list->data;
    int64_t **new = n00b_gc_array_alloc(uint64_t *, len);

    for (int i = 0; i < list->length; i++) {
        new[i] = old[i];
    }

    list->data   = new;
    list->length = len;
    if (!list->dont_acquire) {
        pthread_rwlock_unlock(&list->lock);
    }
}

static inline void
list_auto_resize(n00b_list_t *list)
{
    n00b_list_resize(list, list->length << 1);
}

#define lock_list(x)                 \
    pthread_rwlock_wrlock(&x->lock); \
    x->dont_acquire = true

#define unlock_list(x)       \
    x->dont_acquire = false; \
    pthread_rwlock_unlock(&x->lock)

#define read_start(x) pthread_rwlock_rdlock(&x->lock);
#define read_end(x)   pthread_rwlock_unlock(&x->lock)

bool
n00b_list_set(n00b_list_t *list, int64_t ix, void *item)
{
    if (ix < 0) {
        ix += list->append_ix;
    }

    if (ix < 0) {
        return false;
    }

    lock_list(list);

    if (ix >= list->length) {
        n00b_list_resize(list, n00b_max(ix, list->length << 1));
    }

    if (ix >= list->append_ix) {
        list->append_ix = ix + 1;
    }

    list->data[ix] = (int64_t *)item;
    unlock_list(list);
    return true;
}

void
n00b_list_append(n00b_list_t *list, void *item)
{
    // Warning; this being in place and taking a user callback is a
    // recipe for danger on the lock.
    lock_list(list);
    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    unlock_list(list);
    return;
}

void
n00b_list_sort(n00b_list_t *list, n00b_sort_fn f)
{
    lock_list(list);
    qsort(list->data, list->append_ix, sizeof(int64_t *), f);
    unlock_list(list);
}

static inline void *
n00b_list_get_base(n00b_list_t *list, int64_t ix, bool *err)
{
    if (!list) {
        if (err) {
            *err = true;
        }

        return NULL;
    }

    if (ix < 0 || ix >= list->append_ix) {
        if (err) {
            *err = true;
        }
        return NULL;
    }

    if (err) {
        *err = false;
    }

    return (void *)list->data[ix];
}

void *
n00b_list_get(n00b_list_t *list, int64_t ix, bool *err)
{
    lock_list(list);

    if (ix < 0) {
        ix += list->append_ix;
    }

    void *result = n00b_list_get_base(list, ix, err);
    unlock_list(list);

    return result;
}

void
n00b_list_add_if_unique(n00b_list_t *list,
                       void       *item,
                       bool (*fn)(void *, void *))
{
    lock_list(list);
    // Really meant to be internal for debugging sets; use sets instead.
    for (int i = 0; i < n00b_list_len(list); i++) {
        void *x = n00b_list_get_base(list, i, NULL);

        if ((*fn)(x, item)) {
            unlock_list(list);
            return;
        }
    }

    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    unlock_list(list);
}

void *
n00b_list_pop(n00b_list_t *list)
{
    if (list->append_ix == 0) {
        N00B_CRAISE("Pop called on empty list.");
    }

    lock_list(list);
    void *ret = n00b_list_get_base(list, list->append_ix - 1, NULL);
    list->append_ix--;
    unlock_list(list);
    return ret;
}

void
n00b_list_plus_eq(n00b_list_t *l1, n00b_list_t *l2)
{
    if (l1 == NULL || l2 == NULL) {
        return;
    }

    lock_list(l1);
    read_start(l2);

    int needed = l1->append_ix + l2->append_ix;

    if (needed > l1->length) {
        n00b_list_resize(l1, needed);
    }

    for (int i = 0; i < l2->append_ix; i++) {
        l1->data[l1->append_ix++] = l2->data[i];
    }

    read_end(l2);
    unlock_list(l1);
}

n00b_list_t *
n00b_list_plus(n00b_list_t *l1, n00b_list_t *l2)
{
    // This assumes type checking already happened statically.
    // You can make mistakes manually.
    n00b_list_t *result;

    if (l1 == NULL && l2 == NULL) {
        return NULL;
    }
    if (l1 == NULL) {
        result = n00b_list_shallow_copy(l2);
        return result;
    }
    if (l2 == NULL) {
        result = n00b_list_shallow_copy(l1);
        return result;
    }

    read_start(l1);
    read_start(l2);
    n00b_type_t *t      = n00b_get_my_type(l1);
    size_t      needed = l1->append_ix + l2->append_ix;
    result             = n00b_new(t, n00b_kw("length", n00b_ka(needed)));

    for (int i = 0; i < l1->append_ix; i++) {
        result->data[i] = l1->data[i];
    }

    result->append_ix = l1->append_ix;

    for (int i = 0; i < l2->append_ix; i++) {
        result->data[result->append_ix++] = l2->data[i];
    }
    read_end(l2);
    read_end(l1);

    return result;
}

int64_t
n00b_list_len(const n00b_list_t *list)
{
    if (list == NULL) {
        return 0;
    }
    return (int64_t)list->append_ix;
}

n00b_list_t *
n00b_list(n00b_type_t *x)
{
    return n00b_new(n00b_type_list(x));
}

static n00b_str_t *
n00b_list_repr(n00b_list_t *list)
{
    read_start(list);

    int64_t     len   = n00b_list_len(list);
    n00b_list_t *items = n00b_new(n00b_type_list(n00b_type_utf32()));

    for (int i = 0; i < len; i++) {
        bool       err  = false;
        void      *item = n00b_list_get_base(list, i, &err);
        n00b_str_t *s;

        if (err) {
            continue;
        }

        s = n00b_repr(item, n00b_get_my_type(item));

        n00b_list_append(items, s);
    }

    n00b_str_t *sep    = n00b_get_comma_const();
    n00b_str_t *result = n00b_str_join(items, sep);

    result = n00b_str_concat(n00b_get_lbrak_const(),
                            n00b_str_concat(result, n00b_get_rbrak_const()));

    read_end(list);

    return result;
}

static n00b_obj_t
n00b_list_coerce_to(n00b_list_t *list, n00b_type_t *dst_type)
{
    read_start(list);

    n00b_obj_t     result;
    n00b_dt_kind_t base          = n00b_type_get_kind(dst_type);
    n00b_type_t   *src_item_type = n00b_type_get_param(n00b_get_my_type(list), 0);
    n00b_type_t   *dst_item_type = n00b_type_get_param(dst_type, 0);
    int64_t       len           = n00b_list_len(list);

    if (base == (n00b_dt_kind_t)N00B_T_BOOL) {
        result = (n00b_obj_t)(int64_t)(n00b_list_len(list) != 0);

        read_end(list);

        return result;
    }

    if (base == (n00b_dt_kind_t)N00B_T_LIST) {
        n00b_list_t *res = n00b_new(dst_type, n00b_kw("length", n00b_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = n00b_list_get_base(list, i, NULL);
            n00b_list_set(res,
                         i,
                         n00b_coerce(item, src_item_type, dst_item_type));
        }

        read_end(list);
        return (n00b_obj_t)res;
    }

    if (base == (n00b_dt_kind_t)N00B_T_FLIST) {
        flexarray_t *res = n00b_new(dst_type, n00b_kw("length", n00b_ka(len)));

        for (int i = 0; i < len; i++) {
            void *item = n00b_list_get_base(list, i, NULL);
            flexarray_set(res,
                          i,
                          n00b_coerce(item, src_item_type, dst_item_type));
        }

        return (n00b_obj_t)res;
    }
    n00b_unreachable();
}

n00b_list_t *
n00b_list_copy(n00b_list_t *list)
{
    read_start(list);

    int64_t     len     = n00b_list_len(list);
    n00b_type_t *my_type = n00b_get_my_type((n00b_obj_t)list);
    n00b_list_t *res     = n00b_new(my_type, n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        n00b_obj_t item = n00b_list_get_base(list, i, NULL);
        n00b_list_set(res, i, n00b_copy(item));
    }

    read_end(list);

    return res;
}

n00b_list_t *
n00b_list_shallow_copy(n00b_list_t *list)
{
    read_start(list);

    int64_t     len     = n00b_list_len(list);
    n00b_type_t *my_type = n00b_get_my_type((n00b_obj_t)list);
    n00b_list_t *res     = n00b_new(my_type, n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        n00b_list_set(res, i, n00b_list_get_base(list, i, NULL));
    }

    read_end(list);

    return res;
}

static n00b_obj_t
n00b_list_safe_get(n00b_list_t *list, int64_t ix)
{
    bool err = false;

    read_start(list);

    n00b_obj_t result = n00b_list_get_base(list, ix, &err);

    if (err) {
        n00b_utf8_t *msg = n00b_cstr_format(
            "Array index out of bounds "
            "(ix = {}; size = {})",
            n00b_box_i64(ix),
            n00b_box_i64(n00b_list_len(list)));

        read_end(list);
        N00B_RAISE(msg);
    }

    read_end(list);
    return result;
}

n00b_list_t *
n00b_list_get_slice(n00b_list_t *list, int64_t start, int64_t end)
{
    read_start(list);

    int64_t     len = n00b_list_len(list);
    n00b_list_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            read_end(list);
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
        read_end(list);
        return n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(0)));
    }

    len = end - start;
    res = n00b_new(n00b_get_my_type(list), n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        void *item = n00b_list_get_base(list, start + i, NULL);
        n00b_list_set(res, i, item);
    }

    read_end(list);
    return res;
}

void
n00b_list_set_slice(n00b_list_t *list,
                   int64_t     start,
                   int64_t     end,
                   n00b_list_t *new)
{
    lock_list(list);
    read_start(new);
    int64_t len1 = n00b_list_len(list);
    int64_t len2 = n00b_list_len(new);

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            read_end(new);
            unlock_list(list);
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
        read_end(new);
        unlock_list(list);
        N00B_CRAISE("Out of bounds slice.");
    }

    int64_t slicelen = end - start;
    int64_t newlen   = len1 + len2 - slicelen;

    void **newdata = n00b_gc_array_alloc(void *, newlen);

    if (start > 0) {
        for (int i = 0; i < start; i++) {
            void *item = n00b_list_get_base(list, i, NULL);
            newdata[i] = item;
        }
    }

    for (int i = 0; i < len2; i++) {
        void *item       = n00b_list_get_base(new, i, NULL);
        newdata[start++] = item;
    }

    for (int i = end; i < len1; i++) {
        void *item       = n00b_list_get_base(list, i, NULL);
        newdata[start++] = item;
    }

    list->data      = (int64_t **)newdata;
    list->append_ix = start;

    read_end(new);
    unlock_list(list);
}

bool
n00b_list_remove(n00b_list_t *list, int64_t index)
{
    lock_list(list);
    int64_t nitems = list->append_ix;

    if (index < 0) {
        index += nitems;
    }
    else {
        if (index >= nitems) {
            unlock_list(list);
            return false;
        }
    }
    if (index < 0) {
        unlock_list(list);
        return false;
    }

    --list->append_ix;

    if (index + 1 == nitems) {
        unlock_list(list);
        return true;
    }

    char *end     = (char *)&list->data[nitems];
    char *dst     = (char *)&list->data[index];
    char *src     = (char *)&list->data[index + 1];
    int   bytelen = end - src;

    memmove(dst, src, bytelen);
    unlock_list(list);

    return true;
}

bool
n00b_list_contains(n00b_list_t *list, n00b_obj_t item)
{
    read_start(list);

    int64_t     len       = n00b_list_len(list);
    n00b_type_t *list_type = n00b_get_my_type(list);
    n00b_type_t *item_type = n00b_type_get_param(list_type, 0);

    for (int i = 0; i < len; i++) {
        if (n00b_type_is_ref(item_type)) {
            read_end(list);
            return item == n00b_list_get_base(list, i, NULL);
        }

        if (n00b_eq(item_type, item, n00b_list_get_base(list, i, NULL))) {
            read_end(list);
            return true;
        }
    }

    read_end(list);
    return false;
}

void *
n00b_list_view(n00b_list_t *list, uint64_t *n)
{
    read_start(list);

    void **view;
    int    len = n00b_list_len(list);

    if (n00b_obj_item_type_is_value(list)) {
        view = n00b_gc_array_value_alloc(void *, len);
    }
    else {
        view = n00b_gc_array_alloc(void *, len);
    }

    for (int i = 0; i < len; i++) {
        view[i] = n00b_list_get_base(list, i, NULL);
    }

    read_end(list);

    *n = len;

    return view;
}

void
n00b_list_reverse(n00b_list_t *l)
{
    lock_list(l);

    int64_t **start = l->data;
    int64_t **end   = &l->data[l->append_ix - 1];
    int64_t  *swap;

    while (start < end) {
        swap   = *end;
        *end   = *start;
        *start = swap;
        start++;
        end--;
    }

    unlock_list(l);
}

static n00b_list_t *
n00b_to_list_lit(n00b_type_t *objtype, n00b_list_t *items, n00b_utf8_t *litmod)
{
    n00b_mem_ptr p = {.v = items};
    p.alloc -= 1;

    p.alloc->type = objtype;
    return items;
}

extern bool n00b_flexarray_can_coerce_to(n00b_type_t *, n00b_type_t *);

void
n00b_list_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    // TODO: Do this up like dicts.
}

const n00b_vtable_t n00b_list_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)n00b_list_init,
        [N00B_BI_COERCIBLE]     = (n00b_vtable_entry)n00b_flexarray_can_coerce_to,
        [N00B_BI_COERCE]        = (n00b_vtable_entry)n00b_list_coerce_to,
        [N00B_BI_COPY]          = (n00b_vtable_entry)n00b_list_copy,
        [N00B_BI_SHALLOW_COPY]  = (n00b_vtable_entry)n00b_list_shallow_copy,
        [N00B_BI_ADD]           = (n00b_vtable_entry)n00b_list_plus,
        [N00B_BI_LEN]           = (n00b_vtable_entry)n00b_list_len,
        [N00B_BI_INDEX_GET]     = (n00b_vtable_entry)n00b_list_safe_get,
        [N00B_BI_INDEX_SET]     = (n00b_vtable_entry)n00b_list_set,
        [N00B_BI_SLICE_GET]     = (n00b_vtable_entry)n00b_list_get_slice,
        [N00B_BI_SLICE_SET]     = (n00b_vtable_entry)n00b_list_set_slice,
        [N00B_BI_VIEW]          = (n00b_vtable_entry)n00b_list_view,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)n00b_to_list_lit,
        [N00B_BI_REPR]          = (n00b_vtable_entry)n00b_list_repr,
        [N00B_BI_GC_MAP]        = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_RESTORE]       = (n00b_vtable_entry)n00b_list_restore,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `n00b_stream_t` on my mac).
        [N00B_BI_FINALIZER]     = NULL,
    },
};
