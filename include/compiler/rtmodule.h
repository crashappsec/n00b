#pragma once
#include "n00b.h"

#pragma once
#include "n00b.h"

typedef enum {
    n00b_compile_status_struct_allocated,
    n00b_compile_status_tokenized,
    n00b_compile_status_code_parsed,
    n00b_compile_status_code_loaded,     // parsed w/ declarations processed.
    n00b_compile_status_scopes_merged,   // Merged info global scope,
    n00b_compile_status_tree_typed,      // full symbols and parsing.
    n00b_compile_status_applied_folding, // Skippable and not done yet.
    n00b_compile_status_generated_code
} n00b_module_compile_status;

typedef struct n00b_ct_module_info_t {
#ifdef N00B_DEV
    // Cache all the print nodes to type check before running.
    // This is a temporary measure until we add varargs functions.
    n00b_list_t *print_nodes;
#endif
    // Information we always throw away after compilation. Things like
    // source code where we might want to keep it don't live here.
    n00b_list_t               *tokens;          // xlist: n00b_token_t
    n00b_tree_node_t          *parse_tree;
    n00b_list_t               *errors;          // xlist: n00b_compile_error
    n00b_scope_t              *global_scope;    // Symbols w/ global scope
    n00b_scope_t              *attribute_scope; // Declared or used attrs
    n00b_scope_t              *imports;         // via 'use' statements.
    n00b_cfg_node_t           *cfg;             // CFG for the module top-level.
    n00b_list_t               *callback_lits;
    n00b_spec_t               *local_specs;
    n00b_module_compile_status status;
    unsigned int              fatal_errors : 1;
} n00b_ct_module_info_t;

extern bool          n00b_validate_module_info(n00b_module_t *);
extern n00b_module_t *n00b_init_module_from_loc(n00b_compile_ctx *,
                                              n00b_str_t *);
extern n00b_module_t *n00b_new_module_compile_ctx();
extern n00b_grid_t   *n00b_get_module_summary_info(n00b_compile_ctx *);
extern bool          n00b_add_module_to_worklist(n00b_compile_ctx *,
                                                n00b_module_t *);
extern n00b_utf8_t   *n00b_package_from_path_prefix(n00b_utf8_t *,
                                                  n00b_utf8_t **);
extern n00b_utf8_t   *n00b_format_module_location(n00b_module_t *ctx,
                                                n00b_token_t *);

static inline void
n00b_module_set_status(n00b_module_t *ctx, n00b_module_compile_status status)
{
    if (ctx->ct->status < status) {
        ctx->ct->status = status;
    }
}

#define n00b_set_package_search_path(x, ...) \
    _n00b_set_package_search_path(x, N00B_VA(__VA_ARGS__))

extern n00b_module_t *
n00b_find_module(n00b_compile_ctx *ctx,
                n00b_str_t       *path,
                n00b_str_t       *module,
                n00b_str_t       *package,
                n00b_str_t       *relative_package,
                n00b_str_t       *relative_path,
                n00b_list_t      *fext);

static inline n00b_utf8_t *
n00b_module_fully_qualified(n00b_module_t *f)
{
    if (f->package) {
        return n00b_cstr_format("{}.{}", f->package, f->name);
    }

    return f->name;
}

static inline bool
n00b_path_is_url(n00b_str_t *path)
{
    if (n00b_str_starts_with(path, n00b_new_utf8("https:"))) {
        return true;
    }

    if (n00b_str_starts_with(path, n00b_new_utf8("http:"))) {
        return true;
    }

    return false;
}

#define N00B_INDEX_FN  "$index"
#define N00B_SLICE_FN  "$slice"
#define N00B_PLUS_FN   "$plus"
#define N00B_MINUS_FN  "$minus"
#define N00B_MUL_FN    "$mul"
#define N00B_MOD_FN    "$mod"
#define N00B_DIV_FN    "$div"
#define N00B_FDIV_FN   "$fdiv"
#define N00B_SHL_FN    "$shl"
#define N00B_SHR_FN    "$shr"
#define N00B_BAND_FN   "$bit_and"
#define N00B_BOR_FN    "$bit_or"
#define N00B_BXOR_FN   "$bit_xor"
#define N00B_CMP_FN    "$cmp"
#define N00B_SET_INDEX "$set_index"
#define N00B_SET_SLICE "$set_slice"

void n00b_vm_remove_compile_time_data(n00b_vm_t *);
