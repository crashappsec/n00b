#pragma once

#include "core/builtin_ctypes.h"

typedef struct n00b_tnode_t n00b_tnode_t;

// THASH byte values that provide 'type' info for the hash.
typedef enum : uint8_t {
    n00b_thasht_typevar, // Implies ! concrete.
    n00b_thasht_primitive,
    n00b_thasht_fn,
    n00b_thasht_varargs_fn,
    n00b_thasht_list,
    n00b_thasht_dict,
    n00b_thasht_set,
    n00b_thasht_tuple,
    n00b_thasht_value,
    n00b_thasht_object,
} n00b_tnode_kind;

typedef enum {
    n00b_type_match_exact,
    n00b_type_match_promoted,
    n00b_type_match_right_more_specific,
    n00b_type_match_left_more_specific,
    n00b_type_match_both_have_more_generic_bits,
    n00b_type_cant_match,
} n00b_type_exact_result_t;

typedef struct {
    uint16_t        num_params;
    uint16_t        base_typeid;
    n00b_tnode_kind kind;
} n00b_thash_payload_t;

struct n00b_tnode_t {
    // Soon? Not yet. KISS.
    //    __int128_t           user_object_name_hash;
    void                *typevar_constraints;
    char                *name;
    n00b_tnode_t        *forward;
    n00b_tnode_t        *params;
    n00b_thash_payload_t info;
    uint8_t              concrete : 1;
    // Not concrete, but cannot be linked.
    // This results in copying a type, if there are type vars to bind.
    uint8_t              locked   : 1;
    uint8_t              missing_params;
};

typedef enum {
    n00b_types_incompatable,
    n00b_types_unify,
    n00b_types_require_binding,
} n00b_tnunify_result_kind;

typedef struct n00b_type_ctx_t {
    n00b_dict_t    name_map;    // n00b_ntype_t : char *
    n00b_dict_t    reverse_map; // n00b_ntype_t : n00b_tnode_t *
    n00b_zarray_t *contents;
    bool           keep_names;
} n00b_type_ctx_t;

#define N00B_THASH_OFFSET_BIT (1ULL << 63)

extern void                     _n00b_init_type_ctx(n00b_type_ctx_t *, ...);
extern n00b_type_ctx_t         *n00b_get_runtime_universe(void); /* once */
extern n00b_tnunify_result_kind _n00b_types_can_unify(n00b_ntype_t,
                                                      n00b_ntype_t,
                                                      ...);
extern n00b_ntype_t             n00b_get_my_type(void *);
extern n00b_ntype_t             _n00b_unify(n00b_ntype_t, n00b_ntype_t, ...);
extern n00b_string_t           *_n00b_base_type_name(n00b_ntype_t, ...);
extern bool                     _n00b_type_is_concrete(n00b_ntype_t, ...);
extern n00b_ntype_t             _n00b_type_copy(n00b_ntype_t, ...);
extern bool                     _n00b_has_base_container_type(n00b_ntype_t,
                                                              n00b_builtin_t,
                                                              ...);
extern n00b_ntype_t             n00b_type_instantiate(int32_t,
                                                      n00b_karg_info_t *);
extern n00b_ntype_t             n00b_type_list(n00b_ntype_t);
extern n00b_ntype_t             n00b_type_set(n00b_ntype_t);
extern n00b_ntype_t             n00b_type_dict(n00b_ntype_t, n00b_ntype_t);
extern n00b_ntype_t             n00b_type_tuple(n00b_list_t *);
extern n00b_ntype_t             n00b_type_fn(n00b_ntype_t,
                                             n00b_list_t *,
                                             bool);
extern n00b_ntype_t             n00b_type_tree(n00b_ntype_t);
extern n00b_ntype_t             n00b_new_typevar(void);
extern n00b_ntype_t             n00b_new_named_typevar(char *);
extern n00b_ntype_t             _n00b_get_type(n00b_tnode_t *, ...);
extern n00b_ntype_t             _n00b_get_promotion_type(n00b_ntype_t,
                                                         n00b_ntype_t,
                                                         ...);
extern n00b_tnode_t            *_n00b_get_tnode(n00b_ntype_t t, ...);
extern n00b_string_t           *_n00b_ntype_get_name(n00b_ntype_t, ...);

// Seems redundant to can_unify??
extern n00b_type_exact_result_t n00b_type_cmp_exact(n00b_ntype_t t1,
                                                    n00b_ntype_t t2);

#define n00b_types_can_unify(x, y, ...) \
    _n00b_types_can_unify(x, y, N00B_VA(__VA_ARGS__))
#define n00b_init_type_ctx(x, ...) \
    _n00b_init_type_ctx(x, N00B_VA(__VA_ARGS__))
#define n00b_unify(x, y, ...) \
    _n00b_unify(x, y, N00B_VA(__VA_ARGS__))
#define n00b_base_type_name(x, ...) \
    _n00b_base_type_name(x, N00B_VA(__VA_ARGS__))
#define n00b_type_is_concrete(x, ...) \
    _n00b_type_is_concrete(x, N00B_VA(__VA_ARGS__))
#define n00b_type_copy(x, ...) \
    _n00b_type_copy(x, N00B_VA(__VA_ARGS__))
#define n00b_has_base_container_type(x, y, ...) \
    _n00b_has_base_container_type(x, y, N00B_VA(__VA_ARGS__))
#define n00b_get_type(x, ...) \
    _n00b_get_type(x, N00B_VA(__VA_ARGS__))
#define n00b_get_promotion_type(x, y, ...) \
    _n00b_get_promotion_type(x, y, N00B_VA(__VA_ARGS__))
#define n00b_get_tnode(x, ...) \
    _n00b_get_tnode(x, N00B_VA(__VA_ARGS__))
#define n00b_ntype_get_name(x, ...) \
    _n00b_ntype_get_name(x, N00B_VA(__VA_ARGS__))

#define n00b_type_is_u8(x)    n00b_type_is_byte(x)
#define n00b_type_u8()        n00b_type_byte()
#define n00b_type_is_u64(x)   n00b_type_is_uint(x)
#define n00b_type_u64()       n00b_type_uint()
#define n00b_type_i64()       n00b_type_int()
#define n00b_type_is_f64(x)   n00b_type_float(x)
#define n00b_type_f64()       n00b_type_float()
#define n00b_type_is_kargs(x) n00b_type_keyword(x)
#define n00b_type_kargs()     n00b_type_keyword()

// Temporary I think
#define n00b_type_any_dict(x, y) n00b_type_dict(x, y)
