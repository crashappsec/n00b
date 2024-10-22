#pragma once

#include "n00b.h"

typedef struct {
    // Actually, since objects already contain the full type, this really
    // only needs to hold the base type ID. Should def fix.
    n00b_type_t *held_type;
    void       *held_value;
} n00b_mixed_t;
