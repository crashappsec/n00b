#include "n00b.h"

#pragma once

typedef uint64_t n00b_type_hash_t;

typedef struct n00b_type_info_t n00b_type_info_t;

typedef struct {
    // Bitfield of what container types we might be while inferring.
    uint64_t           *container_options;
    // Holds known value info when infering containers.
    struct n00b_type_t *value_type;
    // Object properties. maps prop name to type node.
    n00b_dict_t        *props;
} tv_options_t;

typedef struct n00b_type_t {
    tv_options_t     options; // Type-specific info.
    n00b_string_t   *name;
    n00b_list_t     *items;
    n00b_builtin_t   base_index;
    uint64_t         flags;
    n00b_type_hash_t fw;
    n00b_type_hash_t typeid;
} n00b_type_t;

typedef struct n00b_obj_info_t {
    n00b_dict_t   *props;
    n00b_string_t *name;
} n00b_obj_info_t;

typedef struct n00b_type_info_t {
    n00b_obj_info_t *obj_info;
    n00b_list_t     *items; // A list of type IDs.
    uint64_t         flags;
    n00b_builtin_t   base_index;

    union {
        uint64_t                 hash;
        struct n00b_type_node_t *fw;
    } ref;

    n00b_lit_syntax_t syntax;
    bool              indexed; // 1st item is always the index type.
    bool              ambiguous;
    bool              concrete;
} n00b_type_info_t;

typedef uint64_t n00b_tid_t; // Will rename to type_t later.

#define N00B_FN_TY_VARARGS 1
// 'Locked' means this type node cannot forward, even though it might
//  have type variables. That causes the system to copy. That is used
//  for function signatures for instance, to keep calls from
//  restricting generic types.
//
// Basically think of locked types as fully resolved in one universe,
// but being copied over into others.
#define N00B_FN_TY_LOCK    2

// When we're type inferencing, we will keep things as type variables
// until the last possible minute, whenever we don't have an absolutely
// concrete type.

// For containers where the exact container type is unknown, this
// indicates whether or not we're confident in how many type
// parameters there will be. This is present mainly to aid in tuple
// merging, and I'm not focused on it right now, so will need to do
// more testing later.
#define N00B_FN_UNKNOWN_TV_LEN 4

// This indicates that we know about a function type, but have not yet
// received any information about their arguments.
//
// This isn't consulted during unification; it's primarily intended for
// call sites to check, so they can fill in param info on callbacks where
// the arguments are elided before we're prepared to do a global lookup.
#define N00B_FN_MISSING_PARAMS 8

typedef enum {
    n00b_type_match_exact,
    n00b_type_match_left_more_specific,
    n00b_type_match_right_more_specific,
    n00b_type_match_both_have_more_generic_bits,
    n00b_type_cant_match,
} n00b_type_exact_result_t;

typedef uint64_t (*n00b_next_typevar_fn)(void);

typedef struct n00b_type_universe_t {
    n00b_dict_t     *dict;
    _Atomic uint64_t next_typeid;
} n00b_type_universe_t;

#ifdef N00B_USE_INTERNAL_API
n00b_string_t *n00b_internal_type_repr(n00b_type_t *, n00b_dict_t *, int64_t *);
#endif
