#define N00B_USE_INTERNAL_API
#include "n00b.h"

// ONE dimensional layout algorithm.
//
// Meant for both:
// 1. Column sizing in tables.
//
// 2. Sizing text windows in one dimension; generally one should size
//    on one dimension, then calculate the needs in the other
//    dimension, since the first dimension's layout can affect the
//    values in the other dimension (specifically, text wrapping)
//
// When there isn't much space and the constraints on cells would
// cause an overflow, the algorithm ends up dropping sizes to 0 to force
// a fit. Generally, it does it first by priority, then right to left.

n00b_tree_node_t *
_n00b_new_layout_cell(n00b_tree_node_t *parent, ...)
{
    n00b_layout_t    *layout = n00b_gc_alloc_mapped(n00b_layout_t,
                                                 N00B_GC_SCAN_NONE);
    n00b_tree_node_t *result;

    if (parent) {
        result = n00b_tree_add_node(parent, layout);
    }
    else {
        result = n00b_new_tree_node(n00b_type_ref(), layout);
    }

    int64_t minimum        = 0;
    int64_t maximum        = 0;
    int64_t preference     = 0;
    double  min_pct        = 0.0;
    double  max_pct        = 0.0;
    int64_t preference_pct = 0.0;
    int64_t priority       = 0;
    int64_t flex           = 0;
    int64_t child_pad      = 0;

    n00b_karg_only_init(parent);
    n00b_kw_int64("min", minimum);
    n00b_kw_int64("max", maximum);
    n00b_kw_int64("preference", preference);
    n00b_kw_float("min_pct", min_pct);
    n00b_kw_float("max_pct", max_pct);
    n00b_kw_float("preference_pct", preference_pct);
    n00b_kw_int64("priority", priority);
    n00b_kw_int64("flex", flex);
    n00b_kw_int64("child_pad", child_pad);

    if (minimum) {
        layout->min.value.i = minimum;
    }
    else {
        if (min_pct < 1.0 && min_pct > 0.0) {
            layout->min.value.d = min_pct;
            layout->min.pct     = true;
        }
    }

    if (maximum) {
        layout->max.value.i = maximum;
    }
    else {
        if (max_pct <= 1.0 && max_pct > 0.0) {
            layout->max.value.d = max_pct;
            layout->max.pct     = true;
        }
    }

    if (preference) {
        layout->pref.value.i = preference;
    }
    else {
        if (preference_pct < 1.0 && preference_pct > 0.0) {
            layout->pref.value.d = preference_pct;
            layout->pref.pct     = true;
        }
    }

    layout->priority      = priority;
    layout->flex_multiple = flex;
    layout->child_pad     = child_pad;

    return result;
}

static void
add_kid_result_nodes(n00b_tree_node_t *tn_cell, n00b_tree_node_t *tn_res)
{
    if (!tn_cell->children) {
        return;
    }

    int l = n00b_tree_get_number_children(tn_cell);

    for (int i = 0; i < l; i++) {
        n00b_layout_result_t *n = n00b_gc_alloc_mapped(n00b_layout_result_t,
                                                       N00B_GC_SCAN_ALL);
        assert(!n->cell);
        assert(!n->computed_min);
        assert(!n->computed_max);
        n->cell                 = n00b_tree_get_child(tn_cell, i);
        n00b_layout_t *layout   = n00b_tree_get_contents(n->cell);
        n->cached_priority      = layout->priority;
        n->cached_flex_multiple = layout->flex_multiple;

        n00b_tree_node_t *kid = n00b_tree_add_node(tn_res, n);
        add_kid_result_nodes(n->cell, kid);
    }
}

static inline n00b_tree_node_t *
generate_result_tree(n00b_tree_node_t *node, int64_t available)
{
    n00b_layout_result_t *t      = n00b_gc_alloc_mapped(n00b_layout_result_t,
                                                   N00B_GC_SCAN_ALL);
    n00b_tree_node_t     *result = n00b_new_tree_node(n00b_type_ref(), t);
    n00b_layout_t        *layout = n00b_tree_get_contents(node);

    t->cell                 = node;
    t->size                 = available;
    t->computed_min         = compute_size(&layout->min, available);
    t->computed_max         = compute_size(&layout->max, available);
    t->computed_pref        = compute_size(&layout->pref, available);
    t->cached_priority      = layout->priority;
    t->cached_flex_multiple = layout->flex_multiple;

    add_kid_result_nodes(node, result);

    return result;
}

static inline int64_t
set_initial_values(n00b_tree_node_t *t, int64_t width, int64_t inner_pad)
{
    int64_t available = width;
    int     l         = n00b_tree_get_number_children(t);

    available -= inner_pad * (l - 1);

    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *kid_tree = n00b_tree_get_child(t, i);
        n00b_layout_result_t *kid_lo   = n00b_tree_get_contents(kid_tree);
        n00b_layout_t        *layout   = n00b_tree_get_contents(kid_lo->cell);

        kid_lo->computed_min  = compute_size(&layout->min, width);
        kid_lo->computed_max  = compute_size(&layout->max, width);
        kid_lo->computed_pref = compute_size(&layout->pref, width);

        kid_lo->size = n00b_max(kid_lo->computed_min, kid_lo->computed_pref);
        available -= kid_lo->size;
    }

    return available;
}

static int
ascending_width(n00b_layout_result_t **ptr1, n00b_layout_result_t **ptr2)
{
    n00b_layout_result_t *info1 = *ptr1;
    n00b_layout_result_t *info2 = *ptr2;

    // First sort by the current value, second sort by the max.

    if (info1->size != info2->size) {
        if (info1->size < info2->size) {
            return -1;
        }
        return 1;
    }

    if (info1->computed_max == info2->computed_max) {
        return 0;
    }

    if (info1->computed_max < info2->computed_max) {
        return -1;
    }

    return 1;
}

static int
descending_width(n00b_layout_result_t **ptr1, n00b_layout_result_t **ptr2)
{
    n00b_layout_result_t *info1 = *ptr1;
    n00b_layout_result_t *info2 = *ptr2;

    if (info1->size == info2->size) {
        if (info1->computed_min < info2->computed_min) {
            return 1;
        }
        if (info1->computed_min > info2->computed_min) {
            return -1;
        }

        return 0;
    }

    if (info1->size < info2->size) {
        return 1;
    }

    return -1;
}

static int
ascending_priority(n00b_layout_result_t **ptr1,
                   n00b_layout_result_t **ptr2)
{
    n00b_layout_result_t *info1 = *ptr1;
    n00b_layout_result_t *info2 = *ptr2;

    if (info1->cached_priority != info2->cached_priority) {
        if (info1->cached_priority < info2->cached_priority) {
            return -1;
        }
        return 1;
    }

    // If priorities are the same, the higher order should be sorted
    // toward the beginning of the list, as we will steal from the back,
    // all other things being equal.
    if (info1->order > info2->order) {
        return -1;
    }

    return 1;
}

static int64_t
incremental_expand(n00b_list_t          *l,
                   n00b_layout_result_t *next,
                   int64_t               available)
{
    int                   n     = n00b_list_len(l);
    n00b_layout_result_t *first = n00b_list_get(l, 0, NULL);

    int target = n00b_min(first->computed_max, next->size);
    int needed = target - first->size;
    int remainder;

    if (available < needed * n) {
        needed    = available / n;
        remainder = available % n;

        for (int i = 0; i < n; i++) {
            n00b_layout_result_t *item   = n00b_list_get(l, i, NULL);
            int                   to_add = needed;

            if (i < remainder) {
                to_add++;
            }

            item->size += to_add;
        }

        return 0;
    }

    for (int i = 0; i < n; i++) {
        n00b_layout_result_t *item = n00b_list_get(l, i, NULL);
        item->size += needed;
        available -= needed;
    }

    while (first->computed_max == first->size) {
        n00b_list_dequeue(l);
        if (!n00b_list_len(l)) {
            break;
        }
        first = n00b_list_get(l, 0, NULL);
    }

    // If we stopped the expand due to hitting a node's max,
    // restart the expand in our new state.
    //
    // Recursion should be fine here, it'll never be too deep.
    if (available && n00b_list_len(l) && first->size < next->computed_max) {
        return incremental_expand(l, next, available);
    }

    return available;
}

static inline void
cleanup_shrink_set(n00b_list_t *shrink_set)
{
    while (n00b_list_len(shrink_set)) {
        n00b_layout_result_t *item = n00b_list_get(shrink_set, 0, NULL);

        if (item->size <= item->computed_min) {
            n00b_list_dequeue(shrink_set);
        }
        else {
            return;
        }
    }
}

static inline int64_t
incremental_shrink(n00b_list_t *shrink_set,
                   int64_t      cur_width,
                   int64_t      boundary,
                   int64_t      available)
{
    int to_cut        = available * -1;
    int n             = n00b_list_len(shrink_set);
    int allocated_cut = to_cut / n;
    int remainder     = to_cut % n;
    int span          = cur_width - boundary;

    for (int i = 0; i < n; i++) {
        n00b_layout_result_t *item = n00b_list_get(shrink_set, i, NULL);
        assert(item->size == cur_width);
    }
    if (allocated_cut > span) {
        for (int i = 0; i < n; i++) {
            n00b_layout_result_t *item = n00b_list_get(shrink_set, i, NULL);
            item->size -= span;
        }

        cleanup_shrink_set(shrink_set);
        return available + span * n;
    }

    for (int i = 0; i < n; i++) {
        n00b_layout_result_t *item = n00b_list_get(shrink_set, i, NULL);
        item->size -= allocated_cut;
        if (i < remainder) {
            item->size -= 1;
        }
    }

    return 0;
}

static inline int64_t
apply_max_values(n00b_tree_node_t *t, int64_t width, int64_t available)
{
    int          l      = n00b_tree_get_number_children(t);
    int64_t      needed = 0;
    n00b_list_t *list   = n00b_list(n00b_type_ref());

    // Pick out all the cells with a max value set.
    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *kid_tree = n00b_tree_get_child(t, i);
        n00b_layout_result_t *kid_lo   = n00b_tree_get_contents(kid_tree);

        int64_t max = kid_lo->computed_max;

        if (max && max > kid_lo->size) {
            needed += max - kid_lo->size;
        }
    }

    l = n00b_list_len(list);

    if (!l) {
        return available;
    }

    if (needed < available) {
        for (int i = 0; i < l; i++) {
            n00b_layout_result_t *info = n00b_list_get(list, i, NULL);
            info->size                 = info->computed_max;
        }

        return available - needed;
    }

    // If we don't have room for everyone to hit their max, we sort by
    // the current size and step up allocations to the smallest first,
    // until they become tied for smallest...

    n00b_list_sort(list, (n00b_sort_fn)ascending_width);

    n00b_list_t *expanding = n00b_list(n00b_type_ref());
    n00b_list_append(expanding, n00b_list_dequeue(list));

    while (n00b_list_len(list) && available) {
        n00b_layout_result_t *cur = n00b_list_dequeue(list);
        available                 = incremental_expand(expanding,
                                       cur,
                                       available);
    }

    return 0;
}

static inline void
apply_flex_growth(n00b_tree_node_t *t, int64_t available)
{
    // At this point, anything with a 'max' has hit it; remaining
    // space is allocated based on 'flex units'. If something with an
    // explicit max also specifies flex, we ignore it.
    //
    // Also, we choose to divide up the space proportionally based on
    // what's available, instead of getting flex items to the same
    // size first. That might be a behavior to change or make
    // optional. We'll have to see.

    int64_t      number_of_flex_units = 0;
    int64_t      flex_value;
    int64_t      remainder;
    n00b_list_t *list = n00b_list(n00b_type_ref());
    int          l    = n00b_tree_get_number_children(t);

    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *node = n00b_tree_get_child(t, i);
        n00b_layout_result_t *kid  = n00b_tree_get_contents(node);

        if (!kid->computed_max && kid->cached_flex_multiple) {
            n00b_list_append(list, kid);
            number_of_flex_units += kid->cached_flex_multiple;
        }
    }

    if (!number_of_flex_units) {
        return;
    }

    l          = n00b_list_len(list);
    flex_value = available / number_of_flex_units;
    remainder  = available % number_of_flex_units;

    int remainder_given = 0;

    for (int i = 0; i < l; i++) {
        n00b_layout_result_t *kid    = n00b_list_get(list, i, NULL);
        int64_t               to_add = flex_value * kid->cached_flex_multiple;
        if (remainder_given < remainder) {
            int64_t n = n00b_min(kid->cached_flex_multiple,
                                 remainder - remainder_given);
            to_add += n;
            remainder_given += n;
        }
        kid->size += to_add;
    }
}

static inline int64_t
shrink_to_fit(n00b_tree_node_t *t, int64_t available)
{
    int          l    = n00b_tree_get_number_children(t);
    n00b_list_t *list = n00b_list(n00b_type_ref());

    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *node = n00b_tree_get_child(t, i);
        n00b_layout_result_t *kid  = n00b_tree_get_contents(node);

        if (kid->size > kid->computed_min) {
            n00b_list_append(list, kid);
        }
    }

    n00b_list_sort(list, (n00b_sort_fn)descending_width);

    n00b_list_t          *shrinking = n00b_list(n00b_type_ref());
    n00b_layout_result_t *cur       = n00b_list_dequeue(list);
    n00b_layout_result_t *next;

    n00b_list_append(shrinking, cur);

    while (cur && n00b_list_len(shrinking) && available < 0) {
        while (n00b_list_len(list)) {
            next = n00b_list_get(list, 0, NULL);

            if (next->size == cur->size) {
                n00b_list_append(shrinking, n00b_list_dequeue(list));
            }
            else {
                break;
            }
        }

        n00b_list_sort(shrinking, (n00b_sort_fn)descending_width);

        int64_t target_width;

        cur = n00b_list_get(shrinking, 0, NULL);
        if (!cur) {
            break;
        }

        if (n00b_list_len(list)) {
            next = n00b_list_get(list, 0, NULL);
            if (next->size > cur->computed_min) {
                target_width = next->size;
            }
            else {
                target_width = cur->computed_min;
            }
        }
        else {
            target_width = cur->computed_min;
        }

        available = incremental_shrink(shrinking,
                                       cur->size,
                                       target_width,
                                       available);
    }

    return available;
}

static inline void
crop_via_priority(n00b_tree_node_t *t, int64_t available)
{
    int          l      = n00b_tree_get_number_children(t);
    n00b_list_t *list   = n00b_list(n00b_type_ref());
    int64_t      to_cut = -available;

    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *node = n00b_tree_get_child(t, i);
        n00b_layout_result_t *kid  = n00b_tree_get_contents(node);

        kid->order = i;

        n00b_list_append(list, kid);
    }

    n00b_list_sort(list, (n00b_sort_fn)ascending_priority);

    for (int i = 0; i < l; i++) {
        n00b_layout_result_t *kid = n00b_list_get(list, i, NULL);
        if (kid->size >= to_cut) {
            kid->size -= to_cut;
            return;
        }
        to_cut -= kid->size;
        kid->size = 0;
    }
}

static void
layout_one_level(n00b_tree_node_t *t)
{
    n00b_layout_result_t *node      = n00b_tree_get_contents(t);
    n00b_layout_t        *kid       = n00b_tree_get_contents(node->cell);
    int64_t               pad       = kid->child_pad;
    int64_t               width     = node->size;
    int64_t               available = set_initial_values(t, width, pad);

    if (available > 0) {
        apply_max_values(t, width, available);
    }

    if (available > 0) {
        apply_flex_growth(t, available);
    }

    if (available < 0) {
        available = shrink_to_fit(t, available);
        if (available < 0) {
            crop_via_priority(t, available);
        }
    }

    int l = n00b_tree_get_number_children(t);

    for (int i = 0; i < l; i++) {
        layout_one_level(n00b_tree_get_child(t, i));
    }
}

static n00b_string_t *
layout_result_repr(n00b_layout_result_t *node)
{
    return n00b_cformat("«em»«#:i»", node->size);
}

n00b_tree_node_t *
n00b_layout_calculate(n00b_tree_node_t *layout, int64_t available)
{
    n00b_tree_node_t *result = generate_result_tree(layout, available);
    layout_one_level(result);
    result->repr_fn = (n00b_node_repr_fn)layout_result_repr;
    return result;
}
