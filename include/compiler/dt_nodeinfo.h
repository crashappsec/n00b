#pragma once
#include "n00b.h"

// This data structure is the first bytes of the extra_info field for anything
// that might be a jump target, including loops, conditionals, case statements,
// etc.
//
// Some of these things can be labeled: loops, typeof() and switch().
// And, we can `break` out of switch / typeof cases. But we can't use
// 'continue' without a loop, which is why we need to track whether
// this data structure is associated with a loop... `continue`
// statements always look for an enclosing loop.

typedef struct {
    n00b_string_t *label;
    n00b_list_t *awaiting_patches;
    int         entry_ip;
    int         exit_ip;
    bool        non_loop;
} n00b_control_info_t;

typedef struct {
    n00b_control_info_t branch_info;
    // switch() and typeof() can be labeled, but do not have automatic
    // variables, so they don't ever get renamed. That's why `label`
    // lives inside of branch_info, but the rest of this is in the
    // loop info.
    n00b_string_t        *label_ix;
    n00b_string_t        *label_last;
    n00b_tree_node_t   *prelude;
    n00b_tree_node_t   *test;
    n00b_tree_node_t   *body;
    n00b_symbol_t      *shadowed_ix;
    n00b_symbol_t      *loop_ix;
    n00b_symbol_t      *named_loop_ix;
    n00b_symbol_t      *shadowed_last;
    n00b_symbol_t      *loop_last;
    n00b_symbol_t      *named_loop_last;
    n00b_symbol_t      *lvar_1;
    n00b_symbol_t      *lvar_2;
    n00b_symbol_t      *shadowed_lvar_1;
    n00b_symbol_t      *shadowed_lvar_2;
    bool               ranged;
    unsigned int       gen_ix       : 1;
    unsigned int       gen_named_ix : 1;
} n00b_loop_info_t;

typedef struct n00b_jump_info_t {
    n00b_control_info_t *linked_control_structure;
    n00b_zinstruction_t *to_patch;
    bool                top;
} n00b_jump_info_t;

#ifdef N00B_USE_INTERNAL_API
typedef struct {
    n00b_scope_t     *module_scope; // For callback resolution.
    n00b_string_t      *name;
    n00b_type_t      *sig;
    n00b_tree_node_t *loc;
    n00b_symbol_t    *resolution;
    unsigned int     polymorphic : 1;
    unsigned int     deferred    : 1;
    unsigned int     callback    : 1;
} n00b_call_resolution_info_t;
#endif
