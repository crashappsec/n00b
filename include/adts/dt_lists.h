#pragma once

#include "n00b.h"

typedef struct {
    int64_t      **data;
    uint64_t       noscan;
    n00b_rw_lock_t lock;
    int32_t        append_ix          : 30;
    // The actual length if treated properly. We should be
    // careful about it.
    int32_t        length             : 30; // The allocated length.
    uint32_t       enforce_uniqueness : 1;
} n00b_list_t;

typedef struct hatstack_t n00b_stack_t;
