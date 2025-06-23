#pragma once
#include "n00b.h"

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
