#pragma once
#include "n00b.h"

typedef struct {
    n00b_string_t *name;
    n00b_ntype_t   type;
    unsigned int   ffi_holds  : 1;
    unsigned int   ffi_allocs : 1;
} n00b_fn_param_info_t;

typedef struct n00b_sig_info_t {
    n00b_ntype_t          full_type;
    n00b_fn_param_info_t *param_info;
    n00b_scope_t         *fn_scope;
    n00b_scope_t         *formals;
    n00b_fn_param_info_t  return_info; // minus initialization.
    int                   num_params;
    unsigned int          pure        : 1;
    unsigned int          void_return : 1;
} n00b_sig_info_t;

typedef struct {
    n00b_string_t          *short_doc;
    n00b_string_t          *long_doc;
    n00b_sig_info_t        *signature_info;
    struct n00b_cfg_node_t *cfg;
    int64_t                 noscan;
    int32_t                 frame_size;
    // sc = 'short circuit'
    // If we are a 'once' function, this is the offset into static data,
    // where we will place:
    //
    // - A boolean.
    // - A pthread_mutex_t
    // - A void *
    //
    // The idea is, if the boolean is true, we only ever read and
    // return the cached (memoized) result, stored in the void *. If
    // it's false, we grab the lock, check the boolean a second time,
    // run thecm function, set the memo and the boolean, and then
    // unlock.
    int32_t                 sc_lock_offset;
    int32_t                 sc_bool_offset;
    int32_t                 sc_memo_offset;
    int32_t                 local_id;
    int32_t                 module_id;
    int32_t                 offset;

    unsigned int private : 1;
    unsigned int once    : 1;
} n00b_fn_decl_t;

typedef struct n00b_funcinfo_t {
    union {
        n00b_ffi_decl_t *ffi_interface;
        n00b_fn_decl_t  *local_interface;
    } implementation;

    unsigned int ffi : 1;
    unsigned int va  : 1;
} n00b_funcinfo_t;
