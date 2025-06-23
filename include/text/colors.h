#pragma once

#include "n00b.h"
// Temporary.
typedef int32_t n00b_color_t;

#define N00B_NO_COLOR -1

typedef struct {
    const char *name;
    int32_t     rgb;
} n00b_color_info_t;

#define n00b_use_truecolor() (1)

extern n00b_color_t n00b_lookup_color(n00b_string_t *);
extern n00b_color_t n00b_to_vga(n00b_color_t truecolor);
