#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define write_start(x) n00b_lock_list(x)
#define write_end(x)   n00b_unlock_list(x)
#define read_start(x)  n00b_lock_list_read(x)
#define read_end(x)    n00b_unlock_list(x)

int
n00b_lexical_sort_fn(const n00b_string_t **s1, const n00b_string_t **s2)
{
    return strcmp((*s1)->data, (*s2)->data);
}

void
n00b_list_init(n00b_list_t *list, va_list args)
{
    int64_t length = 16;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);

    list->noscan    = N00B_NOSCAN;
    list->append_ix = 0;
    list->length    = n00b_max(length, 16);

    n00b_rw_lock_init(&list->lock);

    list->data = n00b_gc_array_alloc(uint64_t *, list->length);
}

void
n00b_private_list_resize(n00b_list_t *list, size_t len)
{
    int64_t **old = list->data;
    int64_t **new = n00b_gc_array_alloc(uint64_t *, len);

    for (int i = 0; i < list->length; i++) {
        new[i] = old[i];
    }

    list->data   = new;
    list->length = len;
}

void
n00b_list_resize(n00b_list_t *list, size_t len)
{
    write_start(list);
    n00b_private_list_resize(list, len);
    write_end(list);
}

static inline void
list_auto_resize(n00b_list_t *list)
{
    n00b_private_list_resize(list, list->length << 1);
}
bool
n00b_private_list_set(n00b_list_t *list, int64_t ix, void *item)
{
    if (ix < 0) {
        ix += list->append_ix;
    }

    if (ix < 0) {
        return false;
    }

    if (list->enforce_uniqueness && n00b_list_contains(list, item)) {
        // Just in case.
        write_end(list);
        N00B_CRAISE("Item already exists in list.");
    }

    if (ix >= list->length) {
        n00b_list_resize(list, n00b_max(ix, list->length << 1));
    }

    if (ix >= list->append_ix) {
        list->append_ix = ix + 1;
    }

    list->data[ix] = (int64_t *)item;
    return true;
}
bool
n00b_list_set(n00b_list_t *list, int64_t ix, void *item)
{
    bool result;

    write_start(list);
    result = n00b_private_list_set(list, ix, item);
    write_end(list);

    return result;
}

void
n00b_private_list_append(n00b_list_t *list, void *item)
{
    // Warning; this being in place and taking a user callback is a
    // recipe for danger on the lock.

    if (list->enforce_uniqueness && n00b_list_contains(list, item)) {
        // Just in case.
        write_end(list);
        N00B_CRAISE("Item already exists in list.");
    }

    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    return;
}

void
n00b_list_append(n00b_list_t *l, void *item)
{
    write_start(l);
    n00b_private_list_append(l, item);
    write_end(l);
}

int
n00b_lexical_sort(const n00b_string_t **s1, const n00b_string_t **s2)
{
    return strcmp((*s1)->data, (*s2)->data);
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
n00b_private_list_get(n00b_list_t *list, int64_t ix, bool *err)
{
    if (ix < 0) {
        ix += list->append_ix;
    }

    void *result = n00b_list_get_base(list, ix, err);

    return result;
}

void *
n00b_list_get(n00b_list_t *list, int64_t ix, bool *err)
{
    read_start(list);

    if (ix < 0) {
        ix += list->append_ix;
    }

    void *result = n00b_list_get_base(list, ix, err);

    read_end(list);

    return result;
}

void
n00b_list_add_if_unique(n00b_list_t *list,
                        void        *item,
                        bool (*fn)(void *, void *))
{
    write_start(list);
    // Really meant to be internal for debugging sets; use sets instead.
    for (int i = 0; i < n00b_list_len(list); i++) {
        void *x = n00b_list_get_base(list, i, NULL);

        if ((*fn)(x, item)) {
            write_end(list);
            return;
        }
    }

    if (list->append_ix >= list->length) {
        list_auto_resize(list);
    }

    list->data[list->append_ix++] = item;

    write_end(list);
}

void *
n00b_private_list_pop(n00b_list_t *list)
{
    if (list->append_ix == 0) {
        return NULL;
    }

    void *ret = n00b_list_get_base(list, list->append_ix - 1, NULL);
    list->append_ix--;

    return ret;
}

void *
n00b_list_pop(n00b_list_t *list)
{
    write_start(list);
    void *result = n00b_private_list_pop(list);
    write_end(list);

    return result;
}

void
n00b_private_list_plus_eq(n00b_list_t *l1, n00b_list_t *l2)
{
    if (l1 == NULL || l2 == NULL) {
        return;
    }

    int needed = l1->append_ix + l2->append_ix;

    if (needed > l1->length) {
        n00b_list_resize(l1, needed);
    }

    for (int i = 0; i < l2->append_ix; i++) {
        void *item = l2->data[i];

        if (l1->enforce_uniqueness && n00b_list_contains(l2, item)) {
            continue;
        }

        l1->data[l1->append_ix++] = item;
    }
}

void
n00b_list_plus_eq(n00b_list_t *l1, n00b_list_t *l2)
{
    if (l1 && l2) {
        write_start(l1);
        read_start(l2);
        n00b_private_list_plus_eq(l1, l2);
        read_end(l2);
        write_end(l1);
    }
}

n00b_list_t *
n00b_private_list_plus(n00b_list_t *l1, n00b_list_t *l2)
{
    // This assumes type checking already happened statically.
    // You can make mistakes manually.
    n00b_list_t *result;

    if (l1 == NULL && l2 == NULL) {
        return NULL;
    }
    if (l1 == NULL) {
        result = n00b_private_list_shallow_copy(l2);
        return result;
    }
    if (l2 == NULL) {
        result = n00b_private_list_shallow_copy(l1);
        return result;
    }

    n00b_type_t *t      = n00b_get_my_type(l1);
    size_t       needed = l1->append_ix + l2->append_ix;
    result              = n00b_new(t, n00b_kw("length", n00b_ka(needed)));

    for (int i = 0; i < l1->append_ix; i++) {
        result->data[i] = l1->data[i];
    }

    result->append_ix = l1->append_ix;

    for (int i = 0; i < l2->append_ix; i++) {
        result->data[result->append_ix++] = l2->data[i];
    }

    return result;
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
    size_t       needed = l1->append_ix + l2->append_ix;
    result              = n00b_new(t, n00b_kw("length", n00b_ka(needed)));

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
_n00b_list(n00b_type_t *x)
{
    return n00b_new(n00b_type_list(x), 0ULL);
}

static n00b_string_t *
n00b_list_to_literal(n00b_list_t *list)
{
    read_start(list);

    int64_t      len   = n00b_list_len(list);
    n00b_list_t *items = n00b_new(n00b_type_list(n00b_type_string()));

    for (int i = 0; i < len; i++) {
        bool           err  = false;
        void          *item = n00b_list_get_base(list, i, &err);
        n00b_string_t *s;

        if (err) {
            continue;
        }

        s = n00b_to_literal(item);
        if (!s || !s->codepoints) {
            s = n00b_to_string(item);
        }

        n00b_list_append(items, s);
    }

    n00b_string_t *sep    = n00b_cached_comma_padded();
    n00b_string_t *result = n00b_string_join(items, sep);

    result = n00b_string_concat(n00b_cached_lbracket(),
                                n00b_string_concat(result,
                                                   n00b_cached_rbracket()));

    read_end(list);

    return result;
}

static n00b_obj_t
n00b_list_coerce_to(n00b_list_t *list, n00b_type_t *dst_type)
{
    read_start(list);

    n00b_obj_t     result;
    n00b_dt_kind_t base          = n00b_type_get_kind(dst_type);
    n00b_type_t   *src_item_type = n00b_type_get_param(n00b_get_my_type(list),
                                                     0);
    n00b_type_t   *dst_item_type = n00b_type_get_param(dst_type, 0);
    int64_t        len           = n00b_list_len(list);

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
n00b_private_list_copy(n00b_list_t *list)
{
    int64_t      len     = n00b_list_len(list);
    n00b_type_t *my_type = n00b_get_my_type((n00b_obj_t)list);
    n00b_list_t *res     = n00b_new(my_type, n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        n00b_obj_t item = n00b_list_get_base(list, i, NULL);
        n00b_private_list_set(res, i, n00b_copy(item));
    }

    return res;
}

n00b_list_t *
n00b_list_copy(n00b_list_t *list)
{
    read_start(list);
    n00b_list_t *result = n00b_private_list_copy(list);
    read_end(list);
    return result;
}

n00b_list_t *
n00b_private_list_shallow_copy(n00b_list_t *list)
{
    int64_t      len     = n00b_list_len(list);
    n00b_type_t *my_type = n00b_get_my_type((n00b_obj_t)list);
    n00b_list_t *res     = n00b_new(my_type, n00b_kw("length", n00b_ka(len)));

    for (int i = 0; i < len; i++) {
        n00b_private_list_set(res, i, n00b_list_get_base(list, i, NULL));
    }

    return res;
}

n00b_list_t *
n00b_list_shallow_copy(n00b_list_t *list)
{
    read_start(list);
    n00b_list_t *result = n00b_private_list_shallow_copy(list);
    read_end(list);
    return result;
}

static n00b_obj_t
n00b_list_safe_get(n00b_list_t *list, int64_t ix)
{
    bool err = false;

    read_start(list);

    n00b_obj_t result = n00b_list_get_base(list, ix, &err);

    if (err) {
        n00b_string_t *msg = n00b_cformat(
            "Array index out of bounds "
            "(ix = «#»; size = «#»)",
            ix,
            n00b_list_len(list));

        read_end(list);
        N00B_RAISE(msg);
    }

    read_end(list);
    return result;
}

n00b_list_t *
n00b_private_list_get_slice(n00b_list_t *list, int64_t start, int64_t end)
{
    int64_t      len = n00b_list_len(list);
    n00b_list_t *res;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return n00b_new(n00b_get_my_type(list),
                            n00b_kw("length", n00b_ka(0)));
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
        void *item = n00b_list_get_base(list, start + i, NULL);
        n00b_list_set(res, i, item);
    }

    return res;
}

n00b_list_t *
n00b_list_get_slice(n00b_list_t *list, int64_t start, int64_t end)
{
    read_start(list);
    n00b_list_t *result = n00b_private_list_get_slice(list, start, end);
    read_end(list);

    return result;
}

void
n00b_private_list_set_slice(n00b_list_t *list,
                            int64_t      start,
                            int64_t      end,
                            n00b_list_t *new,
                            bool private_new)
{
    if (!private_new) {
        read_start(new);
    }

    int64_t len1 = n00b_list_len(list);
    int64_t len2 = n00b_list_len(new);

    if (start < 0) {
        start += len1;
    }
    else {
        if (start >= len1) {
            if (!private_new) {
                read_end(new);
            }
            write_end(list);
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
        if (!private_new) {
            read_end(new);
        }
        write_end(list);
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

    if (!private_new) {
        read_end(new);
    }
}

void
n00b_list_set_slice(n00b_list_t *list,
                    int64_t      start,
                    int64_t      end,
                    n00b_list_t *new)
{
    write_start(list);

    n00b_private_list_set_slice(list, start, end, new, false);

    write_end(list);
}

static void
list_remove_base(n00b_list_t *list, int64_t index)
{
    int64_t nitems = list->append_ix;

    --list->append_ix;

    if (index + 1 == nitems) {
        list->data[index] = NULL;
        return;
    }

    char *end     = (char *)&list->data[nitems];
    char *dst     = (char *)&list->data[index];
    char *src     = (char *)&list->data[index + 1];
    int   bytelen = end - src;

    memmove(dst, src, bytelen);

    list->data[list->append_ix] = NULL;
    return;
}

bool
n00b_private_list_remove(n00b_list_t *list, int64_t index)
{
    int64_t nitems = list->append_ix;

    if (index < 0) {
        index += nitems;
    }
    else {
        if (index >= nitems) {
            return false;
        }
    }
    if (index < 0) {
        return false;
    }

    list_remove_base(list, index);

    return true;
}

bool
n00b_list_remove(n00b_list_t *list, int64_t index)
{
    write_start(list);

    int64_t nitems = list->append_ix;

    if (index < 0) {
        index += nitems;
    }
    else {
        if (index >= nitems) {
            write_end(list);
            return false;
        }
    }
    if (index < 0) {
        write_end(list);
        return false;
    }

    list_remove_base(list, index);
    write_end(list);

    return true;
}

bool
n00b_private_list_remove_item(n00b_list_t *list, void *item)
{
    bool result = false;

    int n = list->append_ix;

    for (int i = 0; i < n; i++) {
        if (item == (void *)list->data[i]) {
            list_remove_base(list, i);
            result = true;
        }
    }

    return result;
}

bool
n00b_list_remove_item(n00b_list_t *list, void *item)
{
    bool result = false;

    write_start(list);

    int n = list->append_ix;

    for (int i = 0; i < n; i++) {
        if (item == (void *)list->data[i]) {
            list_remove_base(list, i);
            result = true;
        }
    }

    write_end(list);
    return result;
}

void *
n00b_private_list_dequeue(n00b_list_t *list)
{
    if (list->append_ix == 0) {
        return NULL;
    }

    void *ret = n00b_list_get_base(list, 0, NULL);
    list_remove_base(list, 0);

    return ret;
}

void *
n00b_list_dequeue(n00b_list_t *list)
{
    write_start(list);
    if (list->append_ix == 0) {
        write_end(list);
        return NULL;
    }

    void *ret = n00b_list_get_base(list, 0, NULL);
    list_remove_base(list, 0);

    write_end(list);

    return ret;
}

int64_t
n00b_private_list_find(n00b_list_t *list, void *item)
{
    int n = n00b_list_len(list);
    for (int i = 0; i < n; i++) {
        void *candidate = n00b_private_list_get(list, i, NULL);
        if (n00b_equals(candidate, item)) {
            return i;
        }
    }

    return -1;
}

int64_t
n00b_list_find(n00b_list_t *list, void *item)
{
    int64_t result;
    read_start(list);
    result = n00b_private_list_find(list, item);
    read_end(list);
    return result;
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
n00b_private_list_reverse(n00b_list_t *l)
{
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
}

void
n00b_list_reverse(n00b_list_t *l)
{
    write_start(l);
    n00b_private_list_reverse(l);
    write_end(l);
}

static n00b_list_t *
n00b_list_create(n00b_type_t   *objtype,
                 n00b_list_t   *items,
                 n00b_string_t *litmod)
{
    n00b_mem_ptr p = {.v = items};
    p.alloc -= 1;

    p.alloc->type = objtype;
    return items;
}

n00b_list_t *
_n00b_to_list(n00b_type_t *t, int nargs, ...)
{
    n00b_list_t *result = n00b_list(t);

    va_list args;

    va_start(args, nargs);

    for (int i = 0; i < nargs; i++) {
        n00b_private_list_append(result, va_arg(args, void *));
    }

    return result;
}

n00b_list_t *
_n00b_c_map(char *s, ...)
{
    n00b_list_t *result = n00b_list(n00b_type_string());
    va_list      args;

    va_start(args, s);

    while (s != NULL) {
        n00b_private_list_append(result, n00b_cstring(s));
        s = va_arg(args, char *);
    }

    return result;
}

n00b_list_t *
n00b_from_cstr_list(char **arr, int64_t n)
{
    n00b_list_t *result = n00b_new(n00b_type_list(n00b_type_string()),
                                   n00b_kw("length", n));

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_cstring(arr[i]);
        n00b_private_list_append(result, s);
    }

    return result;
}

extern bool n00b_flexarray_can_coerce_to(n00b_type_t *, n00b_type_t *);

const n00b_vtable_t n00b_list_vtable = {
    .methods = {
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
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)n00b_list_create,
        [N00B_BI_TO_LITERAL]    = (n00b_vtable_entry)n00b_list_to_literal,
        [N00B_BI_TO_STRING]     = (n00b_vtable_entry)n00b_list_to_literal,
        [N00B_BI_FINALIZER]     = NULL,
    },
};
