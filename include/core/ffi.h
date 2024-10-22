#pragma once
#include "n00b.h"

extern void           n00b_add_static_function(n00b_utf8_t *, void *);
extern void          *n00b_ffi_find_symbol(n00b_utf8_t *, n00b_list_t *);
extern int64_t        n00b_lookup_ctype_id(char *);
extern n00b_ffi_type  *n00b_ffi_arg_type_map(uint8_t);
extern void          *n00b_ref_via_ffi_type(n00b_box_t *, n00b_ffi_type *);
extern n00b_ffi_status ffi_prep_cif(n00b_ffi_cif *,
                                   n00b_ffi_abi,
                                   unsigned int,
                                   n00b_ffi_type *,
                                   n00b_ffi_type **);
extern n00b_ffi_status ffi_prep_cif_var(n00b_ffi_cif *,
                                       n00b_ffi_abi,
                                       unsigned int,
                                       unsigned int,
                                       n00b_ffi_type *,
                                       n00b_ffi_type **);
extern void           ffi_call(n00b_ffi_cif *, void *, void *, void **);

extern n00b_callback_info_t *n00b_callback_info_init();

#define N00B_CSTR_CTYPE_CONST 24
