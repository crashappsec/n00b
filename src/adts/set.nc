#define N00B_USE_INTERNAL_API

#include "n00b.h"

static void
n00b_set_init(n00b_set_t *set, va_list args)
{
    keywords
    {
        hatrack_hash_func_t custom_hash = NULL;
    }

    size_t          hash_fn;
    n00b_type_t    *stype = n00b_get_my_type(set);
    n00b_dt_info_t *info;

    stype = n00b_list_get(n00b_type_get_params(stype), 0, NULL);
    info  = n00b_type_get_data_type_info(stype);

    switch (info->typeid) {
    case N00B_T_REF:
        hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR;
        break;
    case N00B_T_STRING:
    case N00B_T_REGEX:
        custom_hash = (hatrack_hash_func_t)n00b_custom_string_hash;
        break;
    default:
        hash_fn = info->hash_fn;
        break;
    }

    if (custom_hash != NULL) {
        hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM;
        hatrack_set_init(set, hash_fn);
        hatrack_set_set_custom_hash(set, custom_hash);
    }
    else {
        hatrack_set_init(set, hash_fn);
        hatrack_set_set_hash_offset(set, N00B_string_HASH_KEY_POINTER_OFFSET);
        hatrack_set_set_cache_offset(set, N00B_HASH_CACHE_OBJ_OFFSET);
    }
}

n00b_set_t *
n00b_set_shallow_copy(n00b_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    n00b_set_t *result = n00b_new(n00b_get_my_type(s));
    uint64_t    count  = 0;
    void      **items  = (void **)hatrack_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        n00b_assert(items[i] != NULL);
        hatrack_set_add(result, items[i]);
    }

    return result;
}

n00b_list_t *
n00b_set_to_list(n00b_set_t *s)
{
    if (s == NULL) {
        return NULL;
    }

    n00b_type_t *item_type = n00b_type_get_param(n00b_get_my_type(s), 0);
    n00b_list_t *result    = n00b_new(n00b_type_list(item_type));
    uint64_t     count     = 0;
    void       **items     = (void **)hatrack_set_items_sort(s, &count);

    for (uint64_t i = 0; i < count; i++) {
        n00b_list_append(result, items[i]);
    }

    return result;
}

static n00b_set_t *
to_set_lit(n00b_type_t *objtype, n00b_list_t *items, n00b_string_t *litmod)
{
    n00b_set_t *result = n00b_new(objtype);
    int         n      = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        void *item = n00b_list_get(items, i, NULL);

        n00b_assert(item != NULL);
        hatrack_set_add(result, item);
    }

    return result;
}

void
n00b_set_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    // TODO: do this up like dicts.
}

const n00b_vtable_t n00b_set_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)n00b_set_init,
        [N00B_BI_FINALIZER]     = (n00b_vtable_entry)hatrack_set_cleanup,
        [N00B_BI_SHALLOW_COPY]  = (n00b_vtable_entry)n00b_set_shallow_copy,
        [N00B_BI_VIEW]          = (n00b_vtable_entry)hatrack_set_items_sort,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)to_set_lit,
        [N00B_BI_GC_MAP]        = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};
