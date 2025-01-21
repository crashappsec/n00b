#pragma once

#include "n00b.h"

// TODO, should use a streaming interface for long dumps.

extern char        *n00b_hexl(void *, int32_t, uint64_t, int32_t, char *);
extern n00b_utf8_t *_n00b_hex_dump(void *, uint32_t, ...);
extern void         n00b_debug_object(void *, void *);

#define n00b_hex_dump(p, l, ...) _n00b_hex_dump(p, l, N00B_VA(__VA_ARGS__))

extern const uint8_t n00b_hex_map[16];

static inline void
n00b_debug_dump(char *title, char *where, int64_t n)
{
    n00b_printf("[h1]{}[/]\n{}\n",
                n00b_new_utf8(title),
                n00b_hex_dump(where, n));
}

static inline void
n00b_debug_dump_buf(char *title, n00b_buf_t *buf)
{
    n00b_debug_dump(title, buf->data, n00b_buffer_len(buf));
}
