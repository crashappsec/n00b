#include "n00b.h"

#ifndef NO___INT128_T

static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv;
}

#else
static inline bool
hatrack_bucket_unreserved(hatrack_hash_t hv)
{
    return !hv.w1 && !hv.w2;
}

#endif

static inline hatrack_hash_t
new_string_hash(n00b_string_t *s)
{
    union {
        hatrack_hash_t local_hv;
        XXH128_hash_t  xxh_hv;
    } hv;

    if (!s || !s->codepoints) {
        s = n00b_cached_empty_string();
    }

    hatrack_hash_t *cache = (void *)(((char *)s) + N00B_HASH_CACHE_OBJ_OFFSET);
    hv.local_hv           = *cache;

    // This isn't really testing a bucket, just seeing if the hash
    // value is 0.
    if (hatrack_bucket_unreserved(hv.local_hv)) {
        hv.xxh_hv = XXH3_128bits(s->data, s->u8_bytes);
        *cache    = hv.local_hv;
    }

    return *cache;
}

static inline hatrack_hash_t
old_string_hash(n00b_string_t *s)
{
    union {
        hatrack_hash_t local_hv;
        XXH128_hash_t  xxh_hv;
    } hv;

    static n00b_string_t *n00b_null_string = NULL;

    if (n00b_null_string == NULL) {
        n00b_null_string = n00b_cached_empty_string();
        n00b_gc_register_root(&n00b_null_string, 1);
    }

    hatrack_hash_t *cache = (void *)(((char *)s) + N00B_HASH_CACHE_OBJ_OFFSET);

    hv.local_hv = *cache;

    if (!n00b_string_codepoint_len(s)) {
        s = n00b_null_string;
    }

    if (hatrack_bucket_unreserved(hv.local_hv)) {
        hv.xxh_hv = XXH3_128bits(s->data, s->u8_bytes);

        *cache = hv.local_hv;
    }

    return *cache;
}

hatrack_hash_t
n00b_custom_string_hash(void *v)
{
    n00b_type_t *t = n00b_get_my_type(v);

    if (n00b_type_is_string(t)) {
        return new_string_hash(v);
    }

    return old_string_hash(v);
}

void
n00b_store_bits(uint64_t     *bitfield,
                mmm_header_t *alloc)
{
    crown_store_t *store = (crown_store_t *)alloc->data;
    void          *end   = (void *)&store->buckets[store->last_slot + 1];

    n00b_mark_raw_to_addr(bitfield, alloc, end);
}

void
n00b_dict_gc_bits_raw(uint64_t *bitfield, void *alloc)
{
    n00b_dict_t *dict = (n00b_dict_t *)alloc;

    n00b_mark_address(bitfield, alloc, &dict->crown_instance.store_current);
    n00b_mark_address(bitfield, alloc, &dict->crown_instance.aux_info_for_store);
    n00b_mark_address(bitfield, alloc, &dict->bucket_aux);
}

void
n00b_dict_gc_bits_bucket_full(uint64_t     *bitfield,
                              mmm_header_t *alloc)
{
    n00b_mark_address(bitfield, alloc, &alloc->next);
    n00b_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    n00b_mark_address(bitfield, alloc, &item->key);
    n00b_mark_address(bitfield, alloc, &item->value);
}

void
n00b_dict_gc_bits_bucket_key(uint64_t     *bitfield,
                             mmm_header_t *alloc)
{
    n00b_mark_address(bitfield, alloc, &alloc->next);
    n00b_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    n00b_mark_address(bitfield, alloc, &item->key);
}

void
n00b_dict_gc_bits_bucket_value(uint64_t     *bitfield,
                               mmm_header_t *alloc)
{
    n00b_mark_address(bitfield, alloc, &alloc->next);
    n00b_mark_address(bitfield, alloc, &alloc->cleanup_aux);
    hatrack_dict_item_t *item = (hatrack_dict_item_t *)alloc->data;
    n00b_mark_address(bitfield, alloc, &item->value);
}

void
n00b_dict_gc_bits_bucket_hdr_only(uint64_t     *bitfield,
                                  mmm_header_t *alloc)
{
    n00b_mark_address(bitfield, alloc, &alloc->next);
    n00b_mark_address(bitfield, alloc, &alloc->cleanup_aux);
}

void
n00b_setup_unmanaged_dict(n00b_dict_t *dict,
                          size_t       hash_type,
                          bool         trace_keys,
                          bool         trace_vals)
{
    hatrack_dict_init(dict, hash_type, N00B_GC_SCAN_ALL);

    if (trace_keys && trace_vals) {
        hatrack_dict_set_aux(dict, n00b_dict_gc_bits_bucket_full);
        return;
    }
    if (trace_keys) {
        hatrack_dict_set_aux(dict, n00b_dict_gc_bits_bucket_key);
        return;
    }
    if (trace_vals) {
        hatrack_dict_set_aux(dict, n00b_dict_gc_bits_bucket_value);
        return;
    }

    hatrack_dict_set_aux(dict, n00b_dict_gc_bits_bucket_hdr_only);
    return;
}

// Used for dictionaries that are temporary and cannot ever be used in
// an object context. This is mainly for short-term state like marshal
// memos and for type hashing.
n00b_dict_t *
n00b_new_unmanaged_dict(size_t hash, bool trace_keys, bool trace_vals)
{
    n00b_dict_t *dict = n00b_gc_raw_alloc(
        sizeof(n00b_dict_t),
        N00B_GC_SCAN_ALL);

    n00b_setup_unmanaged_dict(dict, hash, trace_keys, trace_vals);
    return dict;
}

static void
n00b_dict_init(n00b_dict_t *dict, va_list args)
{
    size_t       hash_fn;
    n00b_list_t *type_params;
    n00b_type_t *key_type;
    n00b_type_t *value_type;
    n00b_type_t *n00b_dict_type = n00b_get_my_type(dict);

    if (n00b_dict_type != NULL) {
        type_params = n00b_type_get_params(n00b_dict_type);
        key_type    = n00b_private_list_get(type_params, 0, NULL);
        value_type  = n00b_private_list_get(type_params, 1, NULL);

        if (n00b_type_is_string(key_type) || n00b_type_is_regex(key_type)
            || n00b_type_is_buffer(key_type)) {
            hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM;
        }
        else {
            if (n00b_type_is_int_type(key_type)
                || n00b_type_is_float_type(key_type)
                || n00b_type_is_ref(key_type)) {
                hash_fn = HATRACK_DICT_KEY_TYPE_INT;
            }
            else {
                hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR;
            }
        }
    }
    else {
        hash_fn = va_arg(args, size_t);
    }

    hatrack_dict_init(dict, hash_fn, N00B_GC_SCAN_ALL);

    if (n00b_dict_type) {
        void *aux_fun = NULL;

        if (n00b_type_requires_gc_scan(key_type)) {
            if (n00b_type_requires_gc_scan(value_type)) {
                aux_fun = n00b_dict_gc_bits_bucket_full;
            }
            else {
                aux_fun = n00b_dict_gc_bits_bucket_key;
            }
        }
        else {
            if (n00b_type_requires_gc_scan(value_type)) {
                aux_fun = n00b_dict_gc_bits_bucket_value;
            }
            else {
                aux_fun = n00b_dict_gc_bits_bucket_hdr_only;
            }
        }
        hatrack_dict_set_aux(dict, aux_fun);
    }

    switch (hash_fn) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM:
        // clang-format off
        hatrack_dict_set_custom_hash(dict,
                                (hatrack_hash_func_t)n00b_custom_string_hash);
        // clang-format on
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        hatrack_dict_set_hash_offset(dict, N00B_string_HASH_KEY_POINTER_OFFSET);
        /* fallthrough */
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        hatrack_dict_set_cache_offset(dict, N00B_HASH_CACHE_OBJ_OFFSET);
        break;
    default:
        break;
    }

    dict->slow_views = false;
}

static n00b_string_t *
dict_to_string(n00b_dict_t *dict)
{
    uint64_t             view_len;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(dict, &view_len);

    n00b_list_t   *items    = n00b_new(n00b_type_list(n00b_type_string()),
                                  length : view_len);
    n00b_list_t   *one_item = n00b_new(n00b_type_list(n00b_type_string()));
    n00b_string_t *colon    = n00b_cached_colon();

    for (uint64_t i = 0; i < view_len; i++) {
        n00b_string_t *k = n00b_to_literal(view[i].key);
        n00b_string_t *v = n00b_to_literal(view[i].value);

        if (!k) {
            k = n00b_to_string(view[i].key);
        }
        if (!v) {
            v = n00b_to_string(view[i].value);
        }
        n00b_list_set(one_item, 0, k);
        n00b_list_set(one_item, 1, v);
        n00b_list_append(items, n00b_string_join(one_item, colon));
    }

    return n00b_cformat("{«#»}",
                        n00b_string_join(items, n00b_cached_comma_padded()));
}

static bool
dict_can_coerce_to(n00b_type_t *my_type, n00b_type_t *dst_type)
{
    return n00b_types_are_compat(my_type, dst_type, NULL);
}

static n00b_dict_t *
dict_coerce_to(n00b_dict_t *dict, n00b_type_t *dst_type)
{
    uint64_t             len;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(dict, &len);
    n00b_dict_t         *res  = n00b_new(dst_type);

    for (uint64_t i = 0; i < len; i++) {
        void *key_copy = n00b_copy(view[i].key);
        void *val_copy = n00b_copy(view[i].value);

        hatrack_dict_put(res, key_copy, val_copy);
    }

    return res;
}

n00b_dict_t *
n00b_dict_copy(n00b_dict_t *dict)
{
    return dict_coerce_to(dict, n00b_get_my_type(dict));
}

static n00b_dict_t *
n00b_dict_shallow_copy(n00b_dict_t *dict)
{
    uint64_t             len;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(dict, &len);
    n00b_dict_t         *res  = n00b_new(n00b_get_my_type(dict));

    for (uint64_t i = 0; i < len; i++) {
        void *key_copy = view[i].key;
        void *val_copy = view[i].value;

        hatrack_dict_put(res, key_copy, val_copy);
    }

    return res;
}

int64_t
dict_len(n00b_dict_t *dict)
{
    uint64_t len;
    uint64_t view = (uint64_t)hatrack_dict_items_nosort(dict, &len);

    return (int64_t)len | (int64_t)(view ^ view);
}

n00b_dict_t *
dict_plus(n00b_dict_t *d1, n00b_dict_t *d2)
{
    uint64_t             l1;
    uint64_t             l2;
    hatrack_dict_item_t *v1 = hatrack_dict_items_sort(d1, &l1);
    hatrack_dict_item_t *v2 = hatrack_dict_items_sort(d2, &l2);

    n00b_dict_t *result = n00b_new(n00b_get_my_type(d1));

    for (uint64_t i = 0; i < l1; i++) {
        hatrack_dict_put(result, v1[i].key, v1[i].value);
    }

    for (uint64_t i = 0; i < l2; i++) {
        hatrack_dict_put(result, v2[i].key, v2[i].value);
    }

    return result;
}

void *
dict_get(n00b_dict_t *d, void *k)
{
    bool found = false;

    void *result = hatrack_dict_get(d, k, &found);

    if (found == false) {
        N00B_CRAISE("Dictionary key not found.");
    }

    return result;
}

n00b_list_t *
n00b_dict_keys(n00b_dict_t *dict)
{
    uint64_t     view_len;
    n00b_type_t *dict_type = n00b_get_my_type(dict);
    n00b_type_t *key_type  = n00b_private_list_get(dict_type->items, 0, NULL);
    void       **keys      = hatrack_dict_keys_sort(dict, &view_len);
    n00b_list_t *result    = n00b_list(key_type);

    for (unsigned int i = 0; i < view_len; i++) {
        n00b_private_list_append(result, keys[i]);
    }

    return result;
}

n00b_list_t *
n00b_dict_values(n00b_dict_t *dict)
{
    uint64_t     view_len;
    n00b_type_t *dict_type = n00b_get_my_type(dict);
    n00b_type_t *val_type  = n00b_private_list_get(dict_type->items, 1, NULL);
    void       **values    = hatrack_dict_values_sort(dict, &view_len);
    n00b_list_t *result    = n00b_list(val_type);

    for (unsigned int i = 0; i < view_len; i++) {
        n00b_private_list_append(result, values[i]);
    }

    return result;
}

static n00b_dict_t *
to_dict_lit(n00b_type_t *objtype, n00b_list_t *items, n00b_string_t *lm)
{
    uint64_t     n      = n00b_list_len(items);
    n00b_dict_t *result = n00b_new(objtype);

    for (unsigned int i = 0; i < n; i++) {
        n00b_tuple_t *tup = n00b_list_get(items, i, NULL);
        hatrack_dict_put(result, n00b_tuple_get(tup, 0), n00b_tuple_get(tup, 1));
    }

    return result;
}

const n00b_vtable_t n00b_dict_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)n00b_dict_init,
        [N00B_BI_FINALIZER]     = (n00b_vtable_entry)hatrack_dict_cleanup,
        [N00B_BI_TO_STRING]     = (n00b_vtable_entry)dict_to_string,
        [N00B_BI_COERCIBLE]     = (n00b_vtable_entry)dict_can_coerce_to,
        [N00B_BI_COERCE]        = (n00b_vtable_entry)dict_coerce_to,
        [N00B_BI_SHALLOW_COPY]  = (n00b_vtable_entry)n00b_dict_shallow_copy,
        [N00B_BI_COPY]          = (n00b_vtable_entry)n00b_dict_copy,
        [N00B_BI_ADD]           = (n00b_vtable_entry)dict_plus,
        [N00B_BI_LEN]           = (n00b_vtable_entry)dict_len,
        [N00B_BI_INDEX_GET]     = (n00b_vtable_entry)dict_get,
        [N00B_BI_INDEX_SET]     = (n00b_vtable_entry)hatrack_dict_put,
        [N00B_BI_VIEW]          = (n00b_vtable_entry)hatrack_dict_items_sort,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)to_dict_lit,
        [N00B_BI_GC_MAP]        = (n00b_vtable_entry)n00b_dict_gc_bits_raw,
        NULL,
    },
};
