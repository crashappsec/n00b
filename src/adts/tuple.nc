#include "n00b.h"

static void
tuple_init(n00b_tuple_t *tup, va_list args)
{
    keywords
    {
        n00b_list_t *contents = NULL;
    }

    uint64_t n = n00b_list_len(n00b_type_get_params(n00b_get_my_type(tup)));

    // Caution: the GC might scan some non-pointers here.  Could add a
    // hook for dealing w/ it, but would be more effort than it is
    // worth.
    tup->items     = n00b_gc_array_alloc(uint64_t, n);
    tup->num_items = n;

    if (contents != NULL) {
        for (unsigned int i = 0; i < n; i++) {
            tup->items[i] = n00b_list_get(contents, i, NULL);
        }
    }
}

// This should always be statically checked.
void
n00b_tuple_set(n00b_tuple_t *tup, int64_t ix, void *item)
{
    tup->items[ix] = item;
}

void *
n00b_tuple_get(n00b_tuple_t *tup, int64_t ix)
{
    return tup->items[ix];
}
int64_t
n00b_tuple_len(n00b_tuple_t *tup)
{
    return tup->num_items;
}

static n00b_string_t *
tuple_repr(n00b_tuple_t *tup)
{
    int          len   = tup->num_items;
    n00b_list_t *items = n00b_new(n00b_type_list(n00b_type_string()));

    for (int i = 0; i < len; i++) {
        n00b_string_t *s = n00b_to_literal(tup->items[i]);
        if (!s || !s->codepoints) {
            s = n00b_to_string(tup->items[i]);
        }
        n00b_list_append(items, s);
    }

    n00b_string_t *sep    = n00b_cached_comma_padded();
    n00b_string_t *result = n00b_string_join(items, sep);

    result = n00b_string_concat(n00b_cached_lparen(),
                                n00b_string_concat(result, n00b_cached_rparen()));

    return result;
}

static bool
tuple_can_coerce(n00b_ntype_t src, n00b_ntype_t dst)
{
    return n00b_types_are_compat(src, dst, NULL);
}

static n00b_tuple_t *
tuple_coerce(n00b_tuple_t *tup, n00b_ntype_t dst)
{
    n00b_list_t  *srcparams = n00b_type_get_params(n00b_get_my_type(tup));
    n00b_list_t  *dstparams = n00b_type_get_params(dst);
    int           len       = tup->num_items;
    n00b_tuple_t *res       = n00b_new(dst);

    for (int i = 0; i < len; i++) {
        n00b_ntype_t src_type = (uint64_t)n00b_list_get(srcparams, i, NULL);
        n00b_ntype_t dst_type = (uint64_t)n00b_list_get(dstparams, i, NULL);

        res->items[i] = n00b_coerce(tup->items[i], src_type, dst_type);
    }

    return res;
}

static n00b_tuple_t *
tuple_copy(n00b_tuple_t *tup)
{
    return tuple_coerce(tup, n00b_get_my_type(tup));
}

static void *
tuple_from_lit(n00b_ntype_t objtype, n00b_list_t *items, n00b_string_t *litmod)
{
    int l = n00b_list_len(items);

    if (l == 1) {
        return n00b_list_get(items, 0, NULL);
    }
    return n00b_new(objtype, contents : items);
}

// TODO:
// We need to scan the entire tuple, because we are currently
// improperly boxing some ints when we shouldn't.

/*
void
tuple_set_gc_bits(uint64_t       *bitfield,
                  n00b_base_obj_t *alloc)
{
    n00b_tuple_t *tup  = (n00b_tuple_t *)alloc->data;
    n00b_ntype_t t    = alloc->concrete_type;
    int          len  = n00b_type_get_num_params(t);
    int          base = n00b_ptr_diff(alloc, &tup->items[0]);

    for (int i = 0; i < len; i++) {
        if (n00b_type_requires_gc_scan(n00b_type_get_param(t, i))) {
            n00b_set_bit(bitfield, base + i);
        }
    }
}
*/

void *
n00b_clean_internal_list(n00b_list_t *l)
{
    // The goal here is to take an internal list and convert it into one
    // of the following:
    //
    // 1. A single object if there was one item.
    // 2. A list of the item type if all the items are the same.
    // 3. An appropriate tuple type otherwise.

    int len = n00b_list_len(l);

    switch (len) {
    case 0:
        return NULL;
    case 1:
        return n00b_autobox(n00b_list_get(l, 0, NULL));
    default:
        break;
    }

    n00b_list_t *items = n00b_list(n00b_type_internal());

    for (int i = 0; i < len; i++) {
        void *item = n00b_autobox(n00b_list_get(l, i, NULL));
        if (n00b_type_is_list(n00b_get_my_type(item))) {
            item = n00b_clean_internal_list(item);
        }
        n00b_list_append(items, item);
    }

    n00b_list_t *tup_types      = n00b_list(n00b_type_typespec());
    n00b_ntype_t li_type        = n00b_new_typevar();
    bool         requires_tuple = false;

    for (int i = 0; i < len; i++) {
        n00b_ntype_t t = n00b_get_my_type(n00b_list_get(items, i, NULL));
        if (!requires_tuple) {
            if (n00b_type_is_error(n00b_unify(li_type, t))) {
                requires_tuple = true;
            }
        }
        n00b_list_append(tup_types, (void *)t);
    }

    if (!requires_tuple) {
        n00b_ntype_t  res_type = n00b_type_resolve(n00b_get_my_type(items));
        n00b_tnode_t *tn       = n00b_get_tnode(res_type);
        n00b_list_t  *l        = n00b_type_get_params(res_type);

        for (int i = 0; i < n00b_list_len(l); i++) {
            n00b_ntype_t  t   = (n00b_ntype_t)n00b_list_get(l, i, NULL);
            n00b_tnode_t *sub = n00b_get_tnode(t);

            if (tn != sub && sub != &tn->params[i]) {
                tn->params[i].forward = sub;
            }
        }

        return items;
    }

    return n00b_new(n00b_type_tuple(tup_types), contents : items);
}

const n00b_vtable_t n00b_tuple_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)tuple_init,
        [N00B_BI_TO_STRING]     = (n00b_vtable_entry)tuple_repr,
        [N00B_BI_COERCIBLE]     = (n00b_vtable_entry)tuple_can_coerce,
        [N00B_BI_COERCE]        = (n00b_vtable_entry)tuple_coerce,
        [N00B_BI_COPY]          = (n00b_vtable_entry)tuple_copy,
        [N00B_BI_LEN]           = (n00b_vtable_entry)n00b_tuple_len,
        [N00B_BI_INDEX_GET]     = (n00b_vtable_entry)n00b_tuple_get,
        [N00B_BI_INDEX_SET]     = (n00b_vtable_entry)n00b_tuple_set,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)tuple_from_lit,
        [N00B_BI_GC_MAP]        = N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]     = NULL,
    },
};
