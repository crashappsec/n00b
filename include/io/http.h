#pragma once
#include "n00b.h"

typedef enum {
    n00b_http_get    = 0,
    n00b_http_header = 1,
    n00b_http_post   = 2,

} n00b_http_method_t;

typedef struct {
    CURL          *curl;
    n00b_buf_t    *buf;
    n00b_stream_t *to_send;
    n00b_stream_t *output_stream;
    char          *errbuf;
    // Internal lock makes sure that two threads don't call at once.
    n00b_mutex_t   lock;
    CURLcode       code; // Holds the last result code.
} n00b_basic_http_t;

typedef struct {
    n00b_buf_t    *contents;
    n00b_string_t *error;
    CURLcode       code;
} n00b_basic_http_response_t;

// Somewhat short-term TODO:
// Header access.
// Mime.
// POST.
// Cert pin.

extern n00b_basic_http_response_t *_n00b_http_get(n00b_string_t *, ...);
extern n00b_basic_http_response_t *_n00b_http_upload(n00b_string_t *,
                                                     n00b_buf_t *,
                                                     ...);

#define n00b_http_get(u, ...) \
    _n00b_http_get(u, N00B_VA(__VA_ARGS__))

#define n00b_http_upload(u, b, ...) \
    _n00b_http_upload(u, b, N00B_VA(__VA_ARGS__))

#define n00b_vp(x) ((void *)(int64_t)(x))
