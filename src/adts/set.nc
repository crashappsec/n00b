#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    void   *item;
    int64_t order;
} set_sort_info_t;

static int
set_order_cmp(const void *bucket1, const void *bucket2)
{
    set_sort_info_t *item1 = (void *)bucket1;
    set_sort_info_t *item2 = (void *)bucket2;

    int64_t cval = item1->order - item2->order;

    if (!cval) {
        return 0;
    }
    if (cval > 1) {
        return 1;
    }
    return -1;
}

// Returns the full array.
//
// Often stuff like this is sorted based on insertion order.
//
// But, when we are trying to use the sort order to determine
// equality, we want to sort by the hash value, which is consistent
// based on identity.
//
// The subset-type functions use the identity (hash-based) sorting.
// When we're merging stuff, we generally prefer to keep a consistent
// insertion order if it's not too challenging.

static set_sort_info_t *
get_array_for_locked_set(n00b_set_t *s, uint32_t count, bool insertion_order)
{
    n00b_dict_store_t *store;
    int32_t            ix = 0;
    set_sort_info_t   *result;
    int64_t            seq;

    store = s->store;

    if (!count) {
        n00b_dict_unlock_post_copy(s);
        return NULL;
    }

    result = n00b_gc_array_alloc(set_sort_info_t, count);

    for (uint32_t i = 0; i <= store->last_slot; i++) {
        n00b_dict_bucket_t *b = &store->buckets[i];

        if (!b->hv || ((atomic_read(&b->flags) & N00B_HT_FLAG_DELETED) != 0)) {
            continue;
        }

        if (insertion_order) {
            seq = b->insert_order;
        }
        else {
            seq = ((int64_t *)&b->hv)[0];
        }

        result[ix++] = (set_sort_info_t){
            .item  = b->key,
            .order = seq,
        };
    }

    qsort(result, count, sizeof(set_sort_info_t), set_order_cmp);
    return result;
}

static int64_t *
set_to_array(n00b_set_t *s, uint32_t *n)
{
    n00b_dict_lock(s, false, n);
    set_sort_info_t *info   = get_array_for_locked_set(s, *n, true);
    int64_t         *result = (int64_t *)info;

    n00b_dict_unlock_post_copy(s);

    // Consolodate the array.

    int32_t count = *(int32_t *)n;

    for (int i = 1; i < count; i++) {
        result[i] = (int64_t)info[i].item;
    }

    if (count) {
        result[count] = 0ULL;
    }

    return result;
}

n00b_list_t *
n00b_set_items(n00b_set_t *s)
{
    int32_t      n;
    n00b_ntype_t t     = n00b_type_get_param(n00b_get_my_type(s), 0);
    int64_t     *items = set_to_array(s, (uint32_t *)&n);

    return n00b_list(t, length : n, contents : items, copy : false);
}

bool
n00b_set_is_eq(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t n1;
    int32_t n2;

    n00b_dict_lock(s1, false, (uint32_t *)&n1);
    n00b_dict_lock(s2, false, (uint32_t *)&n2);

    if (n1 != n2) {
        n00b_dict_unlock_post_copy(s1);
        n00b_dict_unlock_post_copy(s2);
        return false;
    }

    set_sort_info_t *arr1 = get_array_for_locked_set(s1, n1, false);
    set_sort_info_t *arr2 = get_array_for_locked_set(s2, n2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    for (int i = 0; i < n1; i++) {
        if (arr1[i].order != arr2[i].order) {
            return false;
        }
    }

    return true;
}

// The macro for is_subset calls this after swapping the set ordering.
bool
_n00b_set_is_superset(n00b_set_t *s1, n00b_set_t *s2, ...)
{
    keywords
    {
        bool proper = false;
    }

    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)&num2);

    set_sort_info_t *arr1 = get_array_for_locked_set(s1, num1, false);
    set_sort_info_t *arr2 = get_array_for_locked_set(s2, num2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    int j = 0;

    if (proper && (num1 == num2)) {
        return false;
    }

    for (int i = 0; i < num2; i++) {
        while (true) {
            if (arr1[j].order == arr2[i].order) {
                break;
            }

            // If view1's order # is larger, then it's not in view2.
            if (arr1[j].order > arr2[i].order) {
                return false;
            }

            // If we're at the end of view1, but there are still
            // items in view2, then we're not a superset.
            if (j == num1) {
                return false;
            }
            j++;
        }
    }

    return true;
}

// If either set is empty, we consider it disjoint, even if BOTH sets
// are empty, since this is really meant to answer, "do they share any
// items..." No, they do not.
bool
n00b_sets_are_disjoint(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)&num2);

    set_sort_info_t *arr1 = get_array_for_locked_set(s1, num1, false);
    set_sort_info_t *arr2 = get_array_for_locked_set(s2, num2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    if (!num1 || !num2) {
        return true;
    }

    int i = 0;
    int j = 0;

    while (true) {
        if (i == num1 || j == num2) {
            return true;
        }
        if (arr1[i].order == arr2[j].order) {
            return false;
        }
        if (arr1[i].order > arr2[j].order) {
            j++;
        }
        else {
            i++;
        }
    }
}

void *
_n00b_set_any_item(n00b_set_t *s, ...)
{
    keywords
    {
        bool  random  = false;
        bool *present = NULL;
    }

    int32_t ix = 0;
    int32_t n;

    n00b_dict_lock(s, false, (uint32_t *)&n);
    set_sort_info_t *arr = get_array_for_locked_set(s, n, true);
    n00b_dict_unlock_post_copy(s);

    if (present) {
        *present = n != 0;
    }

    if (!n) {
        return NULL;
    }

    if (random) {
        ix = n00b_rand32() % n;
    }

    return arr[ix].item;
}

// S1 - S2
// This does not keep insertion order.
n00b_set_t *
n00b_set_difference(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)&num2);

    set_sort_info_t *view1 = get_array_for_locked_set(s1, num1, false);
    set_sort_info_t *view2 = get_array_for_locked_set(s2, num2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    int         i      = 0;
    int         j      = 0;

    while (i != num1) {
        if (j < num2) {
            if (view1[i].order == view2[j].order) {
                i++;
                j++;
                continue;
            }

            if (view1[i].order > view2[j].order) {
                j++;
                continue;
            }
        }

        n00b_set_put(result, view1[i].item);
        i++;
    }

    return result;
}

// Union-- combine all elements in both sets, but dupe elements only
//         ever appear once.
//
// Here, we preserve the relative insertion ordering for the union.
// The numbers are set per-dict, so it's not consistent to a universal
// clock, just that the items contributed from each of the two sets
// remain in the same relative order in the new set.
//
// It remains this way, because I expect at some point to add an
// option to use a particular memory location to get the 'ordering'
// int64 value. (It was universal and required in hatrack; it
// currently isn't an option here, but I will at some point make it
// user definable).

n00b_set_t *
n00b_set_union(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)(uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)(uint32_t *)&num2);

    set_sort_info_t *view1 = get_array_for_locked_set(s1, num1, true);
    set_sort_info_t *view2 = get_array_for_locked_set(s2, num2, true);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    int         i      = 0;
    int         j      = 0;

    while ((i < num1) && (j < num2)) {
        if (view1[i].order <= view2[j].order) {
            n00b_set_add(result, view1[i].item);
            i++;
        }
        else {
            n00b_set_add(result, view2[j].item);
            j++;
        }
    }

    while (i < num1) {
        n00b_set_add(result, view1[i].item);
        i++;
    }

    while (j < num2) {
        n00b_set_add(result, view2[j].item);
        j++;
    }

    return result;
}

// Intersection -- Create a new set with all items that exist in BOTH
//                 sets. Does not keep insertion ordering right now.
n00b_set_t *
n00b_set_intersection(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)(uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)(uint32_t *)&num2);

    set_sort_info_t *view1 = get_array_for_locked_set(s1, num1, false);
    set_sort_info_t *view2 = get_array_for_locked_set(s2, num2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    int         i      = 0;
    int         j      = 0;

    while ((i < num1) && (j < num2)) {
        if (view1[i].order == view2[j].order) {
            n00b_set_add(result, view1[i].item);
            i++;
            j++;
            continue;
        }
        if (view1[i].order > view2[j].order) {
            j++;
        }
        else {
            i++;
        }
    }

    return result;
}

// Disjunction -- Create a new set with all items that appear only in
//                one of the two given sets. Also no insertion ordering.
n00b_set_t *
n00b_set_disjunction(n00b_set_t *s1, n00b_set_t *s2)
{
    int32_t num1;
    int32_t num2;

    n00b_dict_lock(s1, false, (uint32_t *)(uint32_t *)&num1);
    n00b_dict_lock(s2, false, (uint32_t *)(uint32_t *)&num2);

    set_sort_info_t *view1 = get_array_for_locked_set(s1, num1, false);
    set_sort_info_t *view2 = get_array_for_locked_set(s2, num2, false);

    n00b_dict_unlock_post_copy(s1);
    n00b_dict_unlock_post_copy(s2);

    n00b_set_t *result = n00b_new(n00b_get_my_type(s1));
    int         i      = 0;
    int         j      = 0;

    while ((i < num1) && (j < num2)) {
        if (view1[i].order == view2[i].order) {
            i++;
            j++;
            continue;
        }
        if (view1[i].order < view2[j].order) {
            n00b_set_add(result, view1[i++].item);
        }
        else {
            n00b_set_add(result, view2[j++].item);
        }
    }

    // If we didn't get to the end of a set, the rest of its items
    // definitely are not in the other set.
    //
    // At most, one of the two loops here will run.
    for (; i < num1; i++) {
        n00b_set_add(result, view1[i].item);
    }
    for (; j < num2; j++) {
        n00b_set_add(result, view2[j].item);
    }

    return result;
}

static void
n00b_set_init_valist(n00b_set_t *s, va_list args)
{
    n00b_dict_init_valist((void *)s, args);
}

const n00b_vtable_t n00b_set_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)n00b_set_init_valist,
        //        [N00B_BI_TO_STRING]     = (n00b_vtable_entry)n00b_set_to_string,
        [N00B_BI_SHALLOW_COPY] = (n00b_vtable_entry)n00b_dict_shallow_copy,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)set_to_array,
        //      [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)to_set_lit,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};
