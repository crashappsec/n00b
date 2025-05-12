#pragma once
#include "n00b.h"

typedef struct {
    n00b_buf_t *buffer;
    int         rposition;
    int         wposition;
} n00b_buffer_channel_t;

extern n00b_channel_t *n00b_channel_from_buffer(n00b_buf_t *,
                                                int64_t,
                                                n00b_list_t *,
                                                bool);
extern n00b_channel_t *_n00b_in_buf_channel(n00b_buf_t *, ...);
extern n00b_channel_t *_n00b_out_buf_channel(n00b_buf_t *, bool, ...);
extern n00b_channel_t *_n00b_io_buf_channel(n00b_buf_t *, bool, ...);

#define n00b_in_buf_channel(buffer, ...) \
    _n00b_in_buf_channel(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL)
#define n00b_out_buf_channel(buffer, ...) \
    _n00b_out_buf_channel(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL, 0ULL)
#define n00b_io_buf_channel(buffer, ...) \
    _n00b_io_buf_channel(buffer, __VA_ARGS__ __VA_OPT__(, ) 0ULL, 0ULL)

static inline n00b_buf_t *
n00b_channel_extract_buffer(n00b_channel_t *s)
{
    n00b_buffer_channel_t *c = (void *)s->cookie;

    if (!n00b_type_is_buffer(n00b_get_my_type(c->buffer))) {
        return NULL;
    }

    return c->buffer;
}
