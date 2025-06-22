#pragma once

#include "n00b.h"

typedef struct {
    char *kw;
    void *value;
} n00b_one_karg_t;

typedef struct {
    int32_t          num_provided;
    int32_t          used;
    n00b_one_karg_t *args;
} n00b_karg_info_t;
