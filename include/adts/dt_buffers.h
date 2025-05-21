#pragma once

#include "n00b.h"

typedef struct {
    char         *data;
    int32_t       flags;
    int32_t       byte_len;
    int32_t       alloc_len;
    n00b_rwlock_t lock;
} n00b_buf_t;
