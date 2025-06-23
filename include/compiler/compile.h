#pragma once
#include "n00b.h"

// This is a kind of INTERFACE to static objects.  At compile time, we
// essentially assign out indexes into this array; everything added to
// it gets 64 bits reserved.
//
// We give strings their own space so that, during incremental
// compilation, we can de-dupe any string objects.
//
// At the end of compilation, we fill this out and marshal it.

typedef struct {
    n00b_static_memory   *memory_layout;
    n00b_dict_t          *str_consts;
    n00b_dict_t          *obj_consts;
    n00b_dict_t          *value_consts;
    n00b_scope_t         *final_attrs;
    n00b_scope_t         *final_globals;
    n00b_spec_t          *final_spec;
    struct n00b_module_t *entry_point;
    struct n00b_module_t *sys_package;
    n00b_dict_t          *module_cache;
    n00b_list_t          *module_ordering;
    n00b_set_t           *backlog;   // Modules we need to process.
    n00b_set_t           *processed; // Modules we've finished with.

    // New static memory implementation.  The object / value bits in
    // the n00b_static_memory struct will be NULL until we actually go
    // to save out the object file. Until then, we keep a dictionary
    // of memos that map the memory address of objects to save to
    // their index in the appropriate list.

    bool fatality;
} n00b_compile_ctx;

extern n00b_compile_ctx *n00b_new_compile_context(n00b_string_t *);
extern n00b_compile_ctx *n00b_compile_from_entry_point(n00b_string_t *);
extern n00b_module_t    *n00b_init_module_from_loc(n00b_compile_ctx *,
                                                   n00b_string_t *);
extern n00b_ntype_t      n00b_string_to_type(n00b_string_t *);
extern bool              n00b_generate_code(n00b_compile_ctx *,
                                            n00b_vm_t *);
extern n00b_vm_t        *n00b_vm_new(n00b_compile_ctx *cctx);

static inline bool
n00b_got_fatal_compiler_error(n00b_compile_ctx *ctx)
{
    return ctx->fatality;
}

extern bool n00b_incremental_module(n00b_vm_t *,
                                    n00b_string_t *,
                                    bool,
                                    n00b_compile_ctx **);

#ifdef N00B_USE_INTERNAL_API
extern void n00b_module_decl_pass(n00b_compile_ctx *,
                                  n00b_module_t *);
extern void n00b_check_pass(n00b_compile_ctx *);
#endif
