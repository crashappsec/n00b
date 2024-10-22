#pragma once
#include "n00b.h"

typedef enum {
    n00b_nt_error,
    n00b_nt_module,
    n00b_nt_body,
    n00b_nt_assign,
    n00b_nt_attr_set_lock,
    n00b_nt_cast,
    n00b_nt_section,
    n00b_nt_if,
    n00b_nt_elif,
    n00b_nt_else,
    n00b_nt_typeof,
    n00b_nt_switch,
    n00b_nt_for,
    n00b_nt_while,
    n00b_nt_break,
    n00b_nt_continue,
    n00b_nt_return,
    n00b_nt_simple_lit,
    n00b_nt_lit_list,
    n00b_nt_lit_dict,
    n00b_nt_lit_set,
    n00b_nt_lit_empty_dict_or_set,
    n00b_nt_lit_tuple,
    n00b_nt_lit_unquoted,
    n00b_nt_lit_callback,
    n00b_nt_lit_tspec,
    n00b_nt_lit_tspec_tvar,
    n00b_nt_lit_tspec_named_type,
    n00b_nt_lit_tspec_parameterized_type,
    n00b_nt_lit_tspec_func,
    n00b_nt_lit_tspec_varargs,
    n00b_nt_lit_tspec_return_type,
    n00b_nt_or,
    n00b_nt_and,
    n00b_nt_cmp,
    n00b_nt_binary_op,
    n00b_nt_binary_assign_op,
    n00b_nt_unary_op,
    n00b_nt_enum,
    n00b_nt_global_enum,
    n00b_nt_enum_item,
    n00b_nt_identifier,
    n00b_nt_func_def,
    n00b_nt_func_mods,
    n00b_nt_func_mod,
    n00b_nt_formals,
    n00b_nt_varargs_param,
    n00b_nt_member,
    n00b_nt_index,
    n00b_nt_call,
    n00b_nt_paren_expr,
    n00b_nt_variable_decls,
    n00b_nt_sym_decl,
    n00b_nt_decl_qualifiers,
    n00b_nt_use,
    n00b_nt_param_block,
    n00b_nt_param_prop,
    n00b_nt_extern_block,
    n00b_nt_extern_sig,
    n00b_nt_extern_param,
    n00b_nt_extern_local,
    n00b_nt_extern_dll,
    n00b_nt_extern_pure,
    n00b_nt_extern_holds,
    n00b_nt_extern_allocs,
    n00b_nt_extern_return,
    n00b_nt_label,
    n00b_nt_case,
    n00b_nt_range,
    n00b_nt_assert,
    n00b_nt_config_spec,
    n00b_nt_section_spec,
    n00b_nt_section_prop,
    n00b_nt_field_spec,
    n00b_nt_field_prop,
    n00b_nt_expression,
    n00b_nt_extern_box,
    n00b_nt_elided,
#ifdef N00B_DEV
    n00b_nt_print,
#endif
} n00b_node_kind_t;

typedef enum : int64_t {
    n00b_op_plus,
    n00b_op_minus,
    n00b_op_mul,
    n00b_op_mod,
    n00b_op_div,
    n00b_op_fdiv,
    n00b_op_shl,
    n00b_op_shr,
    n00b_op_bitand,
    n00b_op_bitor,
    n00b_op_bitxor,
    n00b_op_lt,
    n00b_op_lte,
    n00b_op_gt,
    n00b_op_gte,
    n00b_op_eq,
    n00b_op_neq,
} n00b_operator_t;

typedef struct {
    n00b_token_t *comment_tok;
    int          sibling_id;
} n00b_comment_node_t;

typedef struct {
    // Every node gets a token to mark its location, even if the same
    // token appears in separate nodes (it will never have semantic
    // meaning in more than one).
    n00b_token_t        *token;
    n00b_token_t        *short_doc;
    n00b_token_t        *long_doc;
    n00b_list_t         *comments;
    n00b_obj_t          *value;
    void               *extra_info;
    struct n00b_scope_t *static_scope;
    n00b_type_t         *type;
    // Parse children are stored beside us because we're using the n00b_tree.
    n00b_node_kind_t     kind;
    int                 total_kids;
    int                 sibling_id;
    // The extra_info field is node specific, and in some cases where it
    // will always be used, is pre-alloc'd for us (generanlly the things
    // that branch prealloc).
    //
    // - For literals, holds the litmod. Unused after check pass.
    // - Holds symbol objects for enums, identifiers, members.
    // - For extern signatures, holds the ctype_id for parameters, until the
    //   decl pass where they get put into an ffi_decl object.
    // - For binops, indexing and anything overloadable, stores a
    //   call_resolution_t; this one is NOT pre-alloc'd for us.
    // - For breaks, continues, returns, it will hold the n00b_loop_info_t
    //   (the pnode_t not the tree node) that constitutes the jump target.
    bool                have_value;
} n00b_pnode_t;

typedef struct n00b_checkpoint_t {
    struct n00b_checkpoint_t *prev;
    char                    *fn;
    jmp_buf                  env;
} n00b_checkpoint_t;
