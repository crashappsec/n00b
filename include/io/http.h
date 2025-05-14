#pragma once
#include "n00b.h"

typedef enum {
    n00b_http_get    = 0,
    n00b_http_header = 1,
    n00b_http_post   = 2,

} n00b_http_method_t;

typedef struct {
    CURL           *curl;
    n00b_buf_t     *buf;
    n00b_stream_t *to_send;
    n00b_stream_t *output_stream;
    char           *errbuf;
    // Internal lock makes sure that two threads don't call at once.
    n00b_lock_t     lock;
    CURLcode        code; // Holds the last result code.
} n00b_basic_http_t;

typedef struct {
    n00b_buf_t    *contents;
    n00b_string_t *error;
    CURLcode       code;
} n00b_basic_http_response_t;

static inline CURLcode
n00b_basic_http_raw_setopt(n00b_basic_http_t *self,
                           CURLoption         option,
                           void              *param)
{
    return curl_easy_setopt(self->curl, option, param);
}

static inline void
n00b_basic_http_reset(n00b_basic_http_t *self)
{
    curl_easy_reset(self->curl);
}

static inline CURLcode
n00b_basic_http_run_request(n00b_basic_http_t *self)
{
    n00b_lock_acquire(&self->lock);
    self->code = curl_easy_perform(self->curl);
    n00b_lock_release(&self->lock);

    return self->code;
}

// Somewhat short-term TODO:
// Header access.
// Mime.
// POST.
// Cert pin.

extern n00b_basic_http_response_t *_n00b_http_get(n00b_string_t *, ...);
extern n00b_basic_http_response_t *_n00b_http_upload(n00b_string_t *,
                                                     n00b_buf_t *,
                                                     ...);

static inline bool
n00b_validate_url(n00b_string_t *candidate)
{
    CURLU *handle = curl_url();

    return curl_url_set(handle, CURLUPART_URL, candidate->data, 0) == CURLUE_OK;
}

static inline bool
n00b_http_op_succeded(n00b_basic_http_response_t *op)
{
    return op->code == CURLE_OK;
}

static inline n00b_buf_t *
n00b_http_op_get_output_buffer(n00b_basic_http_response_t *op)
{
    if (!n00b_http_op_succeded(op)) {
        return NULL;
    }

    return op->contents;
}

static inline n00b_string_t *
n00b_http_op_get_output_utf8(n00b_basic_http_response_t *op)
{
    if (!n00b_http_op_succeded(op)) {
        return NULL;
    }

    return n00b_buf_to_string(op->contents);
}

#define n00b_http_get(u, ...) \
    _n00b_http_get(u, N00B_VA(__VA_ARGS__))

#define n00b_http_upload(u, b, ...) \
    _n00b_http_upload(u, b, N00B_VA(__VA_ARGS__))

#define n00b_vp(x) ((void *)(int64_t)(x))
