#pragma once
#include "n00b.h"

typedef struct {
    n00b_fn_decl_t      *decl;
    n00b_zinstruction_t *i;
} n00b_call_backpatch_info_t;

extern void          n00b_layout_module_symbols(n00b_compile_ctx *,
                                                n00b_module_t *);
extern uint32_t      _n00b_layout_const_obj(n00b_compile_ctx *, n00b_obj_t, ...);
extern n00b_table_t *n00b_disasm(n00b_vm_t *, n00b_module_t *m);
extern void          n00b_add_module(n00b_zobject_file_t *, n00b_module_t *);
extern n00b_vm_t    *n00b_new_vm(n00b_compile_ctx *cctx);
extern void          n00b_internal_codegen(n00b_compile_ctx *, n00b_vm_t *);
extern n00b_string_t  *n00b_fmt_instr_name(n00b_zinstruction_t *);

#define n00b_layout_const_obj(c, f, ...) \
    _n00b_layout_const_obj(c, f, N00B_VA(__VA_ARGS__))

// The new API.
extern uint64_t n00b_add_static_object(void *, n00b_compile_ctx *);
extern uint64_t n00b_add_static_string(n00b_string_t *, n00b_compile_ctx *);
extern uint64_t n00b_add_value_const(uint64_t, n00b_compile_ctx *);
