#pragma once

#include "n00b.h"

extern n00b_buf_t    *n00b_buffer_add(n00b_buf_t *, n00b_buf_t *);
extern n00b_buf_t    *n00b_buffer_join(n00b_list_t *, n00b_buf_t *);
extern int64_t        n00b_buffer_len(n00b_buf_t *);
extern void           n00b_buffer_resize(n00b_buf_t *, uint64_t);
extern n00b_string_t *n00b_buf_to_utf8_string(n00b_buf_t *);
extern n00b_string_t *n00b_buffer_to_hex_str(n00b_buf_t *buf);
extern int64_t        _n00b_buffer_find(n00b_buf_t *, n00b_buf_t *, ...);
extern char          *n00b_buffer_to_c(n00b_buf_t *, int64_t *);
extern n00b_buf_t    *n00b_buffer_from_codepoint(n00b_codepoint_t);

#define n00b_buffer_find(buf_main, sub, ...) \
    _n00b_buffer_find(buf_main, sub, N00B_VA(__VA_ARGS__))

static inline n00b_buf_t *
n00b_buffer_empty(void)
{
    return n00b_new(n00b_type_buffer(), n00b_header_kargs("length", 0ULL));
}

#define _n00b_buffer_acquire_w(b) \
    n00b_rw_write_lock(&b->lock)

#define _n00b_buffer_acquire_r(b) \
    n00b_rw_read_lock(&b->lock)

static inline void
n00b_buffer_release(n00b_buf_t *b)
{
    n00b_lock_release(&b->lock);
}

static inline n00b_string_t *
n00b_buf_to_string(n00b_buf_t *b)
{
    return n00b_utf8(b->data, b->byte_len);
}

static inline n00b_buf_t *
n00b_string_to_buffer(n00b_string_t *s)
{
    return n00b_new(n00b_type_buffer(),
                    n00b_header_kargs("length",
                                      (int64_t)s->u8_bytes,
                                      "ptr",
                                      (int64_t)s->data));
}

static inline n00b_buf_t *
n00b_buffer_from_bytes(char *bytes, int64_t len)
{
    return n00b_new(n00b_type_buffer(),
                    n00b_header_kargs("length",
                                      (int64_t)bytes,
                                      "ptr",
                                      (int64_t)len));
}

#define n00b_buffer_acquire_w(b)       \
    {                                  \
        _n00b_buffer_acquire_w(b);     \
        defer(n00b_buffer_release(b)); \
    }

#define n00b_buffer_acquire_r(b)       \
    {                                  \
        _n00b_buffer_acquire_r(b);     \
        defer(n00b_buffer_release(b)); \
    }
