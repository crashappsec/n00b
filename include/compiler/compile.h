#pragma once
#include "n00b.h"

extern n00b_compile_ctx *n00b_new_compile_context(n00b_string_t *);
extern n00b_compile_ctx *n00b_compile_from_entry_point(n00b_string_t *);
extern n00b_module_t    *n00b_init_module_from_loc(n00b_compile_ctx *,
                                                 n00b_string_t *);
extern n00b_type_t      *n00b_string_to_type(n00b_string_t *);
extern bool             n00b_generate_code(n00b_compile_ctx *,
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
