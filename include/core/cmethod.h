#pragma once
#include "n00b.h"

typedef n00b_string_t *(*n00b_repr_fn)(void *);
typedef void *(*n00b_copy_fn)(void *);
typedef void *(*n00b_binop_fn)(void *, void *);
typedef int64_t (*n00b_len_fn)(void *);
typedef void *(*n00b_index_get_fn)(void *, void *);
typedef void (*n00b_index_set_fn)(void *, void *, void *);
typedef void *(*n00b_slice_get_fn)(void *, int64_t, int64_t);
typedef void (*n00b_slice_set_fn)(void *, int64_t, int64_t, void *);
typedef bool (*n00b_can_coerce_fn)(n00b_ntype_t, n00b_ntype_t);
typedef void *(*n00b_coerce_fn)(void *, n00b_ntype_t);
typedef bool (*n00b_cmp_fn)(void *, void *);
typedef void *(*n00b_literal_fn)(n00b_string_t *,
                                 n00b_lit_syntax_t,
                                 n00b_string_t *,
                                 n00b_compile_error_t *);
typedef void *(*n00b_container_lit_fn)(n00b_ntype_t,
                                       n00b_list_t *,
                                       n00b_string_t *);
typedef n00b_string_t *(*n00b_format_fn)(void *, n00b_string_t *s);
typedef n00b_ntype_t (*n00b_ix_item_ty_fn)(n00b_ntype_t);
typedef void *(*n00b_view_fn)(void *, uint64_t *);
typedef void (*n00b_restore_fn)(void *);
typedef n00b_list_t *(*n00b_render_fn)(void *, int64_t, int64_t);
