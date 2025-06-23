#include "n00b.h"

static inline void *
new_segment(n00b_zarray_t *arr)
{
    return mmap(NULL,
                arr->segment_len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                0,
                0);
}

n00b_zarray_t *
n00b_zarray_new(uint32_t items_per_segment, uint32_t item_size)
{
    int s = getpagesize();

    n00b_zarray_t *result = mmap(NULL,
                                 s,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON,
                                 0,
                                 0);

    item_size         = n00b_round_up_to_given_power_of_2(16, item_size);
    int alloc_len     = item_size * items_per_segment;
    alloc_len         = n00b_round_up_to_given_power_of_2(s, alloc_len);
    items_per_segment = alloc_len / item_size;

    result->items_per_segment = items_per_segment;
    result->segment_len       = alloc_len;
    result->item_size         = item_size;
    result->max_segments      = (s - sizeof(n00b_zarray_t)) / sizeof(void *);

    result->segment_pointers[0] = new_segment(result);

    return result;
}

void *
n00b_zarray_cell_address(n00b_zarray_t *arr, uint32_t ix)
{
    uint32_t next = atomic_read(&arr->next_item);

    if (ix >= next) {
        return NULL;
    }

    uint32_t seg_ix  = ix / arr->items_per_segment;
    uint32_t item_no = ix % arr->items_per_segment;

    uint64_t **segment = atomic_read(&arr->segment_pointers[seg_ix]);

    return (void *)&((char *)segment)[item_no * arr->item_size];
}

uint32_t
_n00b_zarray_new_cell(n00b_zarray_t *arr, void **loc, ...)
{
    keywords
    {
        uint32_t num_slots = 1;
    }

    uint32_t start_segment;
    uint32_t end_segment;
    uint32_t start_ix;

    if (num_slots > arr->items_per_segment) {
        abort();
    }

    // Burn through slots to keep items consecutive if multiple
    // slots were asked for at the same time. We do this in the type
    // system, and count on the locality, directly indexing.
    do {
        start_ix      = atomic_fetch_add(&arr->next_item, num_slots);
        start_segment = start_ix / arr->items_per_segment;
        end_segment   = (start_ix + num_slots - 1) / arr->items_per_segment;
    } while (end_segment != start_segment);

    if (start_segment >= arr->max_segments) {
        fprintf(stderr, "Array is too large.\n");
        abort();
    }

    char *ptr = (void *)atomic_read(&arr->segment_pointers[start_segment]);

    if (!ptr) {
        char      *try      = new_segment(arr);
        uint64_t **expected = NULL;

        if (!n00b_cas(&arr->segment_pointers[start_segment],
                      &expected,
                      (void *)try)) {
            munmap(try, arr->segment_len);
        }
    }

    *loc = n00b_zarray_cell_address(arr, start_ix);

    return start_ix;
}

uint32_t
n00b_zarray_new_cells(n00b_zarray_t *arr, void **loc, uint32_t num)
{
    return n00b_zarray_new_cell(arr, loc, num_slots : num);
}

uint32_t
n00b_zarray_len(n00b_zarray_t *arr)
{
    return atomic_load(&arr->next_item);
}

void
n00b_zarray_delete(n00b_zarray_t *arr)
{
    int num_segments = atomic_read(&arr->next_item) / arr->items_per_segment;

    for (int i = 0; i < num_segments; i++) {
        uint64_t **p = arr->segment_pointers[i];
        if (i) {
            munmap(p, arr->segment_len);
        }
    }

    munmap(arr, getpagesize());
}

int32_t
n00b_zarray_get_index(n00b_zarray_t *arr, void *addr)
{
    char    *test     = (char *)addr;
    uint32_t len      = atomic_read(&arr->next_item);
    uint32_t segs     = len / arr->items_per_segment;
    uint32_t leftover = len % arr->items_per_segment;
    char    *sstart;
    char    *send;

    for (uint32_t i = 0; i < segs; i++) {
        sstart = (char *)arr->segment_pointers[i];
        send   = sstart + arr->segment_len;

        if (!sstart) {
            continue;
        }
        if (test < sstart || test > send) {
            continue;
        }

        uint32_t diff = test - sstart;
        assert(!(diff % arr->item_size));
        return i * arr->items_per_segment + (diff / arr->item_size);
    }

    if (!leftover) {
        return -1;
    }

    sstart = (char *)arr->segment_pointers[segs];
    if (!sstart) {
        return -1;
    }

    send = sstart + (leftover * arr->item_size);

    if (test < sstart || test > send) {
        return -1;
    }
    uint32_t diff = test - sstart;
    assert(!(diff % arr->item_size));

    return segs * arr->items_per_segment + (diff / arr->item_size);
}
