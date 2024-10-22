#pragma once
#include "n00b.h"

typedef enum : int8_t {
    N00B_SK_MODULE,
    N00B_SK_FUNC,
    N00B_SK_EXTERN_FUNC,
    N00B_SK_ENUM_TYPE,
    N00B_SK_ENUM_VAL,
    N00B_SK_ATTR,
    N00B_SK_VARIABLE,
    N00B_SK_FORMAL,
    N00B_SK_NUM_SYM_KINDS
} n00b_symbol_kind;

enum {
    N00B_F_HAS_INITIALIZER  = 0x0001,
    N00B_F_DECLARED_CONST   = 0x0002,
    N00B_F_DECLARED_LET     = 0x0004,
    N00B_F_IS_DECLARED      = 0x0008,
    N00B_F_TYPE_IS_DECLARED = 0x0010,
    // Internally, when something has the 'const' flag above set, it
    // only implies the object is user immutable. This flag
    // differentiates between true CONSTs that we could put in
    // read-only storage, and what is for iteration variables on
    // loops, etc.
    N00B_F_USER_IMMUTIBLE   = 0x0020,
    N00B_F_FN_PASS_DONE     = 0x0040,
    N00B_F_USE_ERROR        = 0x0080,
    N00B_F_STATIC_STORAGE   = 0x0100,
    N00B_F_STACK_STORAGE    = 0x0200,
    N00B_F_REGISTER_STORAGE = 0x0400,
    N00B_F_FUNCTION_SCOPE   = 0x0800,
};

typedef enum n00b_scope_kind : int8_t {
    N00B_SCOPE_GLOBAL     = 1,
    N00B_SCOPE_MODULE     = 2,
    N00B_SCOPE_LOCAL      = 4,
    N00B_SCOPE_FUNC       = 8,
    N00B_SCOPE_FORMALS    = 16,
    N00B_SCOPE_ATTRIBUTES = 32,
    N00B_SCOPE_IMPORTS    = 64,
} n00b_scope_kind;

typedef struct {
    // Information we throw away after compilation.
    n00b_tree_node_t *type_decl_node;
    n00b_tree_node_t *value_node;
    void            *cfg_kill_node;
    n00b_tree_node_t *declaration_node;
    n00b_list_t      *sym_defs;
    n00b_list_t      *sym_uses;
} n00b_ct_sym_info_t;

typedef struct n00b_symbol_t {
    // The `value` field gets the proper value for vars and enums, but
    // for other types, it gets a pointer to one of the specific data
    // structures in this file.
    n00b_ct_sym_info_t   *ct; // Compile-time symbol info.
    struct n00b_symbol_t *linked_symbol;
    n00b_utf8_t          *name;
    n00b_utf8_t          *loc;
    n00b_type_t          *type;
    n00b_obj_t            value;
    n00b_symbol_kind      kind;

    // For constant value types, this is an absolute byte offset
    // from the start of the `const_data` buffer.
    //
    // For constant reference types, this field represents a byte
    // offset into the const_instantiations buffer. The object
    // marshaling sticks this value after the object when it
    // successfully marshals it, giving it the offset to put the
    // unmarshaled reference.
    //
    // For all other values, this represents a byte offset from
    // whatever reference point we have; for globals and module
    // variables, it's the start of the runtime mutable static space,
    // and for functions, it'll be from the frame pointer, if it's in
    // the function scope.
    uint32_t static_offset;

    // For things that are statically allocated, this indicates which
    // module's arena we're using, using an index that is equal to
    // the module's index into the `module_ordering` field in the
    // compilation context.
    uint32_t local_module_id;
    uint32_t flags;
} n00b_symbol_t;

typedef struct {
    n00b_utf8_t   *short_doc;
    n00b_utf8_t   *long_doc;
    n00b_symbol_t *linked_symbol;
    n00b_obj_t     callback;
    n00b_obj_t     validator;
    n00b_obj_t     default_value;
    unsigned int  param_index;
    unsigned int  have_default : 1;
} n00b_module_param_info_t;

typedef struct n00b_scope_t {
    struct n00b_scope_t *parent;
    n00b_dict_t         *symbols;
    enum n00b_scope_kind kind;
} n00b_scope_t;
