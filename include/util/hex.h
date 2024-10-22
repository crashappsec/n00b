#pragma once

#include "n00b.h"

// TODO, should use a streaming interface for long dumps.

extern char       *n00b_hexl(void *, int32_t, uint64_t, int32_t, char *);
extern n00b_utf8_t *_n00b_hex_dump(void *, uint32_t, ...);

#define n00b_hex_dump(p, l, ...) _n00b_hex_dump(p, l, N00B_VA(__VA_ARGS__))

extern const uint8_t n00b_hex_map[16];
