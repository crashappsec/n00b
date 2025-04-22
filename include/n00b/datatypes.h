#pragma once

// Stuff used widely enough that it's worth defining early.
typedef struct hatrack_set_st n00b_set_t;
typedef uint64_t              n00b_size_t;
typedef struct timespec       n00b_duration_t;
typedef struct n00b_stream_t  n00b_stream_t;
typedef struct n00b_table_t   n00b_table_t;

#include "adts/dt_box.h"
#include "core/dt_kargs.h"
#include "core/dt_objects.h"
#include "core/dt_literals.h"
#include "text/dt_colors.h"
#include "text/dt_codepoints.h"
#include "adts/dt_flags.h"

typedef struct n00b_type_t n00b_type_t;

#include "core/heap.h"
#include "core/thread.h"
#include "core/kargs.h"
#include "core/dt_exceptions.h"
#include "core/locks.h"
#include "core/tsi.h" // Thread-specific info.

#include "adts/dt_lists.h"
#include "adts/dt_trees.h"
#include "util/dt_tree_patterns.h"
#include "core/dt_types.h"
#include "adts/dt_buffers.h"
#include "crypto/dt_crypto.h"
#include "adts/dt_mixed.h"
#include "adts/dt_tuples.h"
#include "compiler/dt_lex.h"
#include "compiler/dt_errors.h"
#include "compiler/dt_parse.h"
#include "compiler/dt_scopes.h"
#include "core/dt_ffi.h"
#include "core/dt_ufi.h"
#include "core/dt_vm.h"
#include "adts/dt_callbacks.h"
#include "compiler/dt_nodeinfo.h"
#include "compiler/dt_specs.h"
#include "compiler/dt_cfgs.h"
#include "compiler/dt_compile.h"

typedef n00b_string_t *(*n00b_repr_fn)(n00b_obj_t);
typedef n00b_obj_t (*n00b_copy_fn)(n00b_obj_t);
typedef n00b_obj_t (*n00b_binop_fn)(n00b_obj_t, n00b_obj_t);
typedef int64_t (*n00b_len_fn)(n00b_obj_t);
typedef n00b_obj_t (*n00b_index_get_fn)(n00b_obj_t, n00b_obj_t);
typedef void (*n00b_index_set_fn)(n00b_obj_t, n00b_obj_t, n00b_obj_t);
typedef n00b_obj_t (*n00b_slice_get_fn)(n00b_obj_t, int64_t, int64_t);
typedef void (*n00b_slice_set_fn)(n00b_obj_t, int64_t, int64_t, n00b_obj_t);
typedef bool (*n00b_can_coerce_fn)(n00b_type_t *, n00b_type_t *);
typedef void *(*n00b_coerce_fn)(void *, n00b_type_t *);
typedef bool (*n00b_cmp_fn)(n00b_obj_t, n00b_obj_t);
typedef n00b_obj_t (*n00b_literal_fn)(n00b_string_t *,
                                      n00b_lit_syntax_t,
                                      n00b_string_t *,
                                      n00b_compile_error_t *);
typedef n00b_obj_t (*n00b_container_lit_fn)(n00b_type_t *,
                                            n00b_list_t *,
                                            n00b_string_t *);
typedef n00b_string_t *(*n00b_format_fn)(n00b_obj_t, n00b_string_t *s);
typedef n00b_type_t *(*n00b_ix_item_ty_fn)(n00b_type_t *);
typedef void *(*n00b_view_fn)(n00b_obj_t, uint64_t *);
typedef void (*n00b_restore_fn)(n00b_obj_t);
typedef n00b_list_t *(*n00b_render_fn)(n00b_obj_t, int64_t, int64_t);
