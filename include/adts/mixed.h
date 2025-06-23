#pragma once

#include "n00b.h"

typedef struct {
    // Actually, since objects already contain the full type, this really
    // only needs to hold the base type ID. Should def fix.
    n00b_ntype_t held_type;
    void        *held_value;
} n00b_mixed_t;

extern void n00b_mixed_set_value(n00b_mixed_t *, n00b_ntype_t, void **);
extern void n00b_unbox_mixed(n00b_mixed_t *, n00b_ntype_t, void **);
