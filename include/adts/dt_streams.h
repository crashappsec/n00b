#pragma once

#include "n00b.h"

typedef struct n00b_cookie_t n00b_cookie_t;

typedef void (*n00b_stream_setup_fn)(n00b_cookie_t *);
typedef size_t (*n00b_stream_read_fn)(n00b_cookie_t *, char *, int64_t);
typedef size_t (*n00b_stream_write_fn)(n00b_cookie_t *, char *, int64_t);
typedef void (*n00b_stream_close_fn)(n00b_cookie_t *);
typedef bool (*n00b_stream_seek_fn)(n00b_cookie_t *, int64_t);

typedef struct n00b_cookie_t {
    n00b_obj_t           object;
    char               *extra; // Whatever the implementation wants.
    int64_t             position;
    int64_t             eof;
    int64_t             flags;
    n00b_stream_setup_fn ptr_setup;
    n00b_stream_read_fn  ptr_read;
    n00b_stream_write_fn ptr_write;
    n00b_stream_close_fn ptr_close;
    n00b_stream_seek_fn  ptr_seek;
} n00b_cookie_t;

typedef struct {
    union {
        FILE         *f;
        n00b_cookie_t *cookie;
    } contents;
    int64_t flags;
} n00b_stream_t;

#define N00B_F_STREAM_READ         0x0001
#define N00B_F_STREAM_WRITE        0x0002
#define N00B_F_STREAM_APPEND       0x0004
#define N00B_F_STREAM_CLOSED       0x0008
#define N00B_F_STREAM_BUFFER_IN    0x0010
#define N00B_F_STREAM_STR_IN       0x0020
#define N00B_F_STREAM_UTF8_OUT     0x0040
#define N00B_F_STREAM_UTF32_OUT    0x0080
#define N00B_F_STREAM_USING_COOKIE 0x0100
