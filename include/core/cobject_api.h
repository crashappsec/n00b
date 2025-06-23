#pragma once

#include "n00b/base.h"

extern bool         n00b_can_coerce(n00b_ntype_t, n00b_ntype_t);
extern void        *n00b_coerce(void *, n00b_ntype_t, n00b_ntype_t);
extern void        *n00b_coerce_object(const void *, n00b_ntype_t);
extern void        *n00b_copy(void *);
extern void        *n00b_copy_object_of_type(void *, n00b_ntype_t);
extern void        *n00b_add(void *, void *);
extern void        *n00b_sub(void *, void *);
extern void        *n00b_mul(void *, void *);
extern void        *n00b_div(void *, void *);
extern void        *n00b_mod(void *, void *);
extern bool         n00b_eq(n00b_ntype_t, void *, void *);
extern bool         n00b_equals(void *, void *);
extern bool         n00b_lt(n00b_ntype_t, void *, void *);
extern bool         n00b_gt(n00b_ntype_t, void *, void *);
extern int64_t      n00b_len(void *);
extern void        *n00b_index_get(void *, void *);
extern void         n00b_index_set(void *, void *, void *);
extern void        *n00b_slice_get(void *, int64_t, int64_t);
extern void         n00b_slice_set(void *, int64_t, int64_t, void *);
extern n00b_ntype_t n00b_get_item_type(void *);
extern void        *n00b_get_view(void *, int64_t *);
extern void        *n00b_container_literal(n00b_ntype_t,
                                           n00b_list_t *,
                                           n00b_string_t *);
extern void         n00b_finalize_allocation(void *);
extern void        *n00b_shallow(void *);
extern void        *n00b_autobox(void *);
extern n00b_list_t *n00b_render(void *, int64_t, int64_t);
extern void         n00b_scan_header_only(uint64_t *, int);

#if defined(N00B_ADD_ALLOC_LOC_INFO)
extern void *_n00b_new(n00b_heap_t *, char *, int, n00b_ntype_t, ...);

#define n00b_new(tid, ...) \
    _n00b_new(n00b_default_heap, __FILE__, __LINE__, tid, N00B_VA(__VA_ARGS__))
#else
extern void *_n00b_new(n00b_heap_t *, n00b_ntype_t, ...);

#define n00b_new(tid, ...) \
    _n00b_new(n00b_default_heap, tid, N00B_VA(__VA_ARGS__))
#endif
