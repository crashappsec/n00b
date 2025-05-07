#pragma once
#include "n00b.h"

typedef void *(*n00b_channel_cb_t)(void *, void *);

typedef struct {
    n00b_channel_t   *cref;
    n00b_channel_cb_t cb;
    void             *thunk;
} n00b_callback_channel_t;

extern n00b_channel_t *_n00b_new_callback_channel(n00b_channel_cb_t *, ...);

#define n00b_new_callback_channel(callback, ...) \
    _n00b_new_callback_channel(                  \
        (void *)(callback),                      \
        N00B_PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
