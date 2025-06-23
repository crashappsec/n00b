#pragma once
#include "n00b.h"

typedef void *(*n00b_stream_cb_t)(void *, void *);

typedef struct {
    n00b_stream_cb_t cb;
    void            *params;
    void            *user_defined;
} n00b_callback_cookie_t;

extern n00b_stream_t *_n00b_new_callback_stream(n00b_stream_cb_t,
                                                void *,
                                                ...);

#define n00b_new_callback_stream(callback, param, ...)              \
    n00b_name_cb_hack(                                              \
        _n00b_new_callback_stream((void *)(callback),               \
                                  param,                            \
                                  __VA_ARGS__ __VA_OPT__(, ) 0ULL), \
        #callback)
