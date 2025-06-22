#pragma once
#include "n00b.h"

typedef uint64_t n00b_ntype_t;
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

typedef struct {
    uint16_t        num_params;
    uint16_t        base_typeid;
    n00b_tnode_kind kind;
} n00b_thash_payload_t;

struct n00b_tnode_t {
    // Soon? Not yet. KISS.
    //    __int128_t           user_object_name_hash;
    void                *typevar_constraints;
    n00b_tnode_t        *forward;
    n00b_tnode_t        *params;
    n00b_thash_payload_t info;
    uint8_t              concrete : 1;
    // Not concrete, but cannot be linked.
    // This results in copying a type, if there are type vars to bind.
    uint8_t              locked   : 1;
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

extern void _n00b_init_type_ctx(n00b_type_ctx_t *, ...);
#define n00b_init_type_ctx(x, ...) \
    _n00b_init_type_ctx(x, N00B_VA(__VA_ARGS__))

extern n00b_tnunify_result_kind n00b_ntypes_can_unify(n00b_type_ctx_t *,
                                                      n00b_ntype_t,
                                                      n00b_ntype_t);
extern n00b_ntype_t _n00b_ntype_unify(n00b_ntype_t, n00b_ntype_t, ...);
#define n00b_ntype_unify(x, y, ...)                      \
    _n00b_ntype_unify(x, y, N00B_VA(__VA_ARGS__))

extern n00b_string_t *_n00b_ntype_get_name(n00b_ntype_t, ...);
#define n00b_ntype_get_name(x, ...) \
    _n00b_ntype_get_name(x, N00B_VA(__VA_ARGS__))

extern bool _n00b_tconcrete(n00b_ntype_t, ...);
#define n00b_tconcrete(x, ...) _n00b_tconcrete(x, N00B_VA(__VA_ARGS__))

extern n00b_ntype_t _n00b_tcopy(n00b_ntype_t, ...);
#define n00b_tcopy(x, ...) _n00b_tcopy(x, N00B_VA(__VA_ARGS__))

extern bool _n00b_has_base_container_type(n00b_ntype_t, n00b_builtin_t, ...);
#define n00b_has_base_container_type(x, y, ...) \
    _n00b_has_base_container_type(x, y,  N00B_VA(__VA_ARGS__))


extern n00b_ntype_t n00b_ntype_instantiate(int32_t, n00b_karg_info_t *);

extern n00b_ntype_t n00b_tlist(n00b_ntype_t);
extern n00b_ntype_t n00b_tset(n00b_ntype_t);
extern n00b_ntype_t n00b_tdict(n00b_ntype_t, n00b_ntype_t);
extern n00b_ntype_t n00b_ttuple(n00b_list_t *);
extern n00b_ntype_t n00b_ttree(n00b_ntype_t);
extern n00b_ntype_t n00b_ttvar(void);

static inline n00b_ntype_t
n00b_tprimitive(int32_t id)
{
    assert(id >= 0 && id < N00B_T_NUM_PRIMITIVE_BUILTINS);
    return (n00b_ntype_t)id;
}

static inline bool
n00b_runtime_type_check(n00b_ntype_t t1, n00b_ntype_t t2)
{
    return t1 == t2;
}

#include "core/builtin_tinfo_gen.h"

