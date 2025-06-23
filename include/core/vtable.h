#pragma once

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

typedef void (*n00b_vtable_entry)(void *, va_list);

typedef struct {
    n00b_vtable_entry methods[N00B_BI_NUM_FUNCS];
} n00b_vtable_t;
