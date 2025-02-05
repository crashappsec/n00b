#pragma once
#include "n00b.h"

typedef struct {
    n00b_string_t      *target_symbol_name;
    n00b_type_t      *target_type;
    n00b_tree_node_t *decl_loc;
    n00b_funcinfo_t   binding;
} n00b_callback_info_t;

#define N00B_CB_FLAG_FFI    1
#define N00B_CB_FLAG_STATIC 2
