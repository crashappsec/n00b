#pragma once
#include "n00b.h"

// For the foreign function interface, it's easier to redeclare
// libffi's structures than to deal w/ ensuring we have the right .h
// on each architecture. The only thing we need that might be
// different on different platforms is the ABI; hopefully
// FFI_DEFAUL T_ABI is the same everywhere, but if it isn't, that's
// easier to deal with than the header file situation.
//

typedef enum {
    N00B_FFI_FIRST_ABI   = 0,
    N00B_FFI_NON_GNU_ABI = 1,
    N00B_FFI_GNU_ABI     = 2,
    N00B_FFI_LAST_ABI,
#ifdef __GNUC__
    N00B_FFI_DEFAULT_ABI = N00B_FFI_GNU_ABI
#else
    N00B_FFI_DEFAULT_ABI = N00B_NON_GNU_ABI
#endif
} n00b_ffi_abi;

// This is libffi's `ffi_type`.
typedef struct n00b_ffi_type {
    size_t                size;
    unsigned short        alignment;
    unsigned short        ffitype;
    struct n00b_ffi_type **elements;
} n00b_ffi_type;

// This is libffi's `ffi_cif` type.
typedef struct {
    n00b_ffi_abi    abi;
    unsigned       nargs;
    n00b_ffi_type **arg_types;
    n00b_ffi_type  *rtype;
    unsigned       bytes;
    unsigned       flags;
    // Currently, no platform in libffi takes more than two 'unsigned int's
    // worth of space, so only one of these should be necessary, but
    // adding an extra one just in case; enough platforms take two fields
    // that I can see there eventually being a platform w/ 2 64-bit slots.
    // We alloc ourselves based on this size, so no worries there.
    uint64_t       extra_cif1;
    uint64_t       extra_cif2;
} n00b_ffi_cif;

typedef struct {
    void          *fptr;
    n00b_string_t    *local_name;
    n00b_string_t    *extern_name;
    uint64_t       str_convert;
    uint64_t       hold_info;
    uint64_t       alloc_info;
    n00b_ffi_type **args;
    n00b_ffi_type  *ret;
    n00b_ffi_cif    cif;
} n00b_zffi_cif;

typedef enum {
    N00B_FFI_OK = 0,
    N00B_FFI_BAD_TYPEDEF,
    N00B_FFI_BAD_ABI,
    N00B_FFI_BAD_ARGTYPE
} n00b_ffi_status;

typedef struct n00b_ffi_decl_t {
    n00b_string_t            *short_doc;
    n00b_string_t            *long_doc;
    n00b_string_t            *local_name;
    struct n00b_sig_info_t *local_params;
    n00b_string_t            *external_name;
    n00b_list_t            *dll_list;
    uint8_t               *external_params;
    uint8_t                external_return_type;
    bool                   skip_boxes;
    n00b_zffi_cif           cif;
    int64_t                num_ext_params;
    int64_t                global_ffi_call_ix;
} n00b_ffi_decl_t;

extern n00b_ffi_type ffi_type_void;
extern n00b_ffi_type ffi_type_uint8;
extern n00b_ffi_type ffi_type_sint8;
extern n00b_ffi_type ffi_type_uint16;
extern n00b_ffi_type ffi_type_sint16;
extern n00b_ffi_type ffi_type_uint32;
extern n00b_ffi_type ffi_type_sint32;
extern n00b_ffi_type ffi_type_uint64;
extern n00b_ffi_type ffi_type_sint64;
extern n00b_ffi_type ffi_type_float;
extern n00b_ffi_type ffi_type_double;
extern n00b_ffi_type ffi_type_pointer;

#define ffi_type_uchar  ffi_type_uint8
#define ffi_type_schar  ffi_type_sint8
#define ffi_type_ushort ffi_type_uint16
#define ffi_type_sshort ffi_type_sint16
#define ffi_type_ushort ffi_type_uint16
#define ffi_type_sshort ffi_type_sint16
#define ffi_type_uint   ffi_type_uint32
#define ffi_type_sint   ffi_type_sint32
#define ffi_type_ulong  ffi_type_uint64
#define ffi_type_slong  ffi_type_sint64
