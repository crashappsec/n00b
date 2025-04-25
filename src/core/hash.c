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

// The n00b object hash.

hatrack_hash_t
n00b_hash(void *object)
{
    if (!n00b_in_heap(object)) {
        return hash_pointer(object);
    }

    n00b_type_t *t = n00b_get_my_type(object);

    if (!t) {
        return hash_pointer(object);
    }

    n00b_dt_info_t       *info      = n00b_type_get_data_type_info(t);
    size_t                hash_type = info->hash_fn;
    hatrack_hash_func_t   func      = NULL;
    hatrack_offset_info_t offsets   = {0, 0};

    switch (hash_type) {
    case HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM:
        func = n00b_custom_string_hash;
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_CSTR:
        offsets.hash_offset = N00B_string_HASH_KEY_POINTER_OFFSET;
        break;
    case HATRACK_DICT_KEY_TYPE_OBJ_PTR:
    case HATRACK_DICT_KEY_TYPE_OBJ_INT:
    case HATRACK_DICT_KEY_TYPE_OBJ_REAL:
        offsets.cache_offset = N00B_HASH_CACHE_OBJ_OFFSET;
        break;
    }

    return hatrack_hash_by_type(hash_type, func, &offsets, object);
}

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

    if (t->base_index == N00B_T_STRING) {
        return new_string_hash(v);
    }

    return old_string_hash(v);
}
