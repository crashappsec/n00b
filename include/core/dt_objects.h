#pragma once
#include "n00b/base.h"

typedef void *n00b_obj_t;

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

// At least for now, we're going to only us built-in methods of fixed
// size and know parameters in the vtable.
typedef void (*n00b_vtable_entry)(n00b_obj_t *, va_list);
typedef void (*n00b_container_init)(n00b_obj_t *, void *, va_list);
typedef uint64_t (*n00b_obj_size_helper)(void *);

// Note that in the long term, many of these things wouldn't be baked
// into a static table, but because we are not implementing any of
// them in n00b itself right now, this is just better overall.
typedef enum {
    N00B_BI_CONSTRUCTOR = 0,
    N00B_BI_ALLOC_SZ,
    N00B_BI_TO_STRING,
    N00B_BI_TO_LITERAL,
    N00B_BI_FORMAT,
    N00B_BI_FINALIZER,
    N00B_BI_COERCIBLE,    // Pass 2 types, return coerrced type, or type error.
    N00B_BI_COERCE,       // Actually do the coercion.
    N00B_BI_FROM_LITERAL, // Used to parse a literal.
    N00B_BI_COPY,
    N00B_BI_SHALLOW_COPY,
    N00B_BI_RESTORE,
    // __ functions. With primitive numeric types the compiler knows
    // to generate the proper underlying code, so don't need them.
    // But any higher-level stuff that want to overload the op, they do.
    //
    // The compiler will use their presence to type check, and will use
    // coercible to see if conversion is possible.
    //
    // The type requirements are annotated to the right.  For
    // functions that can have a different return type, we need to add
    // something for that.
    //
    // If the function isn't provided, we assume it must be the same
    // as the operand.
    N00B_BI_ADD, // `t + `t -> `t
    N00B_BI_SUB, // `t - `t -> `t
    N00B_BI_MUL, // `t * `t -> `v -- requires return type
    N00B_BI_DIV, // `t / `t -> `v -- requires return type
    N00B_BI_MOD, // `t % `t -> `v
    N00B_BI_EQ,
    N00B_BI_LT,
    N00B_BI_GT,
    // Container funcs
    N00B_BI_LEN,       // `t -> int
    N00B_BI_INDEX_GET, // `t[`n] -> `v (such that `t = list[`v] or
                       //  `t = dict[`n, `v]
    N00B_BI_INDEX_SET, // `t[`n] = `v -- requires index type
    N00B_BI_SLICE_GET, // `t[int:int] -> `v
    N00B_BI_SLICE_SET,
    // Returns the item type, given how the type is parameterized.
    N00B_BI_ITEM_TYPE,
    N00B_BI_VIEW, // Return a view on a container.
    N00B_BI_CONTAINER_LIT,
    N00B_BI_GC_MAP,
    N00B_BI_RENDER,
    N00B_BI_HASH,
    N00B_BI_NUM_FUNCS,
} n00b_builtin_type_fn;

typedef enum : uint8_t {
    n00b_ix_item_sz_byte    = 0,
    n00b_ix_item_sz_16_bits = 1,
    n00b_ix_item_sz_32_bits = 2,
    n00b_ix_item_sz_64_bits = 3,
    n00b_ix_item_sz_1_bit   = 0xff,
} n00b_ix_item_sz_t;

typedef struct {
    n00b_vtable_entry methods[N00B_BI_NUM_FUNCS];
} n00b_vtable_t;

typedef struct {
    alignas(8)
    // clang-format off
    // The base ID for the data type. For now, this is redundant while we
    // don't have user-defined types, since we're caching data type info
    // in a fixed array. But once we add user-defined types, they'll
    // get their IDs from a hash function, and that won't work for
    // everything.
    const char         *name;
    const uint64_t      typeid;
    const n00b_vtable_t *vtable;
    const uint32_t      alloc_len; // How much space to allocate.
    const n00b_dt_kind_t dt_kind;
    const bool          by_value : 1;
    const bool          mutable : 1;
    // clang-format on
} n00b_dt_info_t;

