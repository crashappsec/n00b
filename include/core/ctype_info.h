#pragma once

#include "n00b/base.h"
#include "core/vtable.h"

typedef enum {
    N00B_DT_KIND_nil,
    N00B_DT_KIND_primitive,
    N00B_DT_KIND_internal, // Internal primitives.
    N00B_DT_KIND_type_var,
    N00B_DT_KIND_list,
    N00B_DT_KIND_dict,
    N00B_DT_KIND_tuple,
    N00B_DT_KIND_func,
    N00B_DT_KIND_box,
    N00B_DT_KIND_maybe,
    N00B_DT_KIND_object,
    N00B_DT_KIND_oneof,
} n00b_dt_kind_t;

typedef struct {
    alignas(8)
        // clang-format off
    // The base ID for the data type. For now, this is redundant while we
    // don't have user-defined types, since we're caching data type info
    // in a fixed array. But once we add user-defined types, they'll
    // get their IDs from a hash function, and that won't work for
    // everything.
    const char          *name;
    const uint64_t       typeid;
    const n00b_vtable_t *vtable;
    const uint32_t       alloc_len; // How much space to allocate.
    const n00b_dt_kind_t dt_kind;
    const uint8_t        int_bits; // Bits, not including sign.
    const uint8_t        box_id;     // # of builtin that boxes us.
    const uint8_t        unbox_id;   // # of builtin we box.
    const uint8_t        promote_to;
    const uint32_t       by_value : 1;
    const uint32_t       mutable  : 1;
    const uint32_t       sign     : 1;
    // clang-format on
} n00b_dt_info_t;
