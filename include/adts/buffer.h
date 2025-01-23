#pragma once

#include "n00b.h"

extern n00b_buf_t  *n00b_buffer_add(n00b_buf_t *, n00b_buf_t *);
extern n00b_buf_t  *n00b_buffer_join(n00b_list_t *, n00b_buf_t *);
extern int64_t      n00b_buffer_len(n00b_buf_t *);
extern void         n00b_buffer_resize(n00b_buf_t *, uint64_t);
extern n00b_utf8_t *n00b_buf_to_utf8_string(n00b_buf_t *);
extern n00b_utf8_t *n00b_buffer_to_hex_str(n00b_buf_t *buf);
extern int64_t      _n00b_buffer_find(n00b_buf_t *, n00b_buf_t *, ...);

#define n00b_buffer_find(buf_main, sub, ...) \
    _n00b_buffer_find(buf_main, sub, N00B_VA(__VA_ARGS__))

static inline n00b_buf_t *
n00b_buffer_empty()
{
    return n00b_new(n00b_type_buffer(), n00b_kw("length", n00b_ka(0)));
}

static inline void
n00b_buffer_acquire_w(n00b_buf_t *b)
{
    n00b_rw_lock_acquire_for_write(&b->lock);
}

static inline void
n00b_buffer_acquire_r(n00b_buf_t *b)
{
    n00b_rw_lock_acquire_for_read(&b->lock, true);
}

static inline void
n00b_buffer_release(n00b_buf_t *b)
{
    n00b_rw_lock_release(&b->lock);
}
