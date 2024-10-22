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
