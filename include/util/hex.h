#pragma once

#include "n00b.h"

// TODO, should use a streaming interface for long dumps.

extern n00b_string_t *_n00b_hex_dump(void *, uint32_t, ...);
extern void           n00b_debug_object(void *, void *);
extern n00b_string_t *n00b_bytes_to_hex(char *, int);

#define n00b_hex_dump(p, l, ...) _n00b_hex_dump(p, l, N00B_VA(__VA_ARGS__))

extern const uint8_t n00b_hex_map[16];

static inline void
n00b_debug_dump(char *title, char *where, int64_t n)
{
    n00b_printf("«em1»«#»«/»\n«#»\n",
                n00b_cstring(title),
                n00b_hex_dump(where, n));
}

static inline n00b_string_t *
n00b_heap_dump(char *start, int64_t bytes, uint64_t guard, bool fields)
{
    if (!guard) {
        guard = n00b_gc_guard;
    }

    return n00b_hex_dump(start, bytes);
}

static inline void
n00b_debug_dump_buf(char *title, n00b_buf_t *buf)
{
    n00b_debug_dump(title, buf->data, n00b_buffer_len(buf));
}
