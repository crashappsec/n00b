#pragma once
#include <stdint.h>

typedef struct {
    _Atomic uint32_t     next_item;
    uint32_t             items_per_segment;
    uint32_t             segment_len;
    uint32_t             item_size;
    uint32_t             max_segments;
    _Atomic(uint64_t **) segment_pointers[];
} n00b_zarray_t;

extern n00b_zarray_t *n00b_zarray_new(uint32_t, uint32_t);
extern void          *n00b_zarray_cell_address(n00b_zarray_t *,
                                               uint32_t);
extern uint32_t       _n00b_zarray_new_cell(n00b_zarray_t *,
                                            void **,
                                            ...);
extern uint32_t       n00b_zarray_new_cells(n00b_zarray_t *,
                                            void **,
                                            uint32_t);
extern uint32_t       n00b_zarray_len(n00b_zarray_t *);
extern void           n00b_zarray_delete(n00b_zarray_t *);
extern n00b_zarray_t *n00b_zarray_unsafe_copy(n00b_zarray_t *);
extern int32_t        n00b_zarray_get_index(n00b_zarray_t *, void *);

#define n00b_zarray_new_cell(arg1, arg2, ...) \
    _n00b_zarray_new_cell(arg1, arg2, N00B_VA(__VA_ARGS__))
