#pragma once

#include "n00b.h"

typedef struct {
    int64_t        **data;
    int32_t          append_ix;
    // The actual length if treated properly. We should be
    // careful about it.
    int32_t          length; // The allocated length.
    pthread_rwlock_t lock;
    // Used when we hold the write lock to prevent nested acquires.
    bool             dont_acquire;
} n00b_list_t;

typedef struct hatstack_t n00b_stack_t;
