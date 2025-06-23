#pragma once

#include <stdint.h>

typedef struct n00b_static_memory {
    int64_t **items;
    uint32_t  num_items;
    uint32_t  alloc_len;
} n00b_static_memory;
