#pragma once

#include "n00b.h"

#define n00b_use_truecolor() (1)

extern n00b_color_t n00b_lookup_color(n00b_string_t *);
extern n00b_color_t n00b_to_vga(n00b_color_t truecolor);
