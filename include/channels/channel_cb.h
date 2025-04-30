#pragma once
#include "n00b.h"

typedef void *(*n00b_channel_cb_t)(void *, void *);

typedef struct {
    n00b_channel_cb_t cb;
    void             *params;
} n00b_callback_channel_t;

extern n00b_channel_t *_n00b_new_callback_channel(n00b_channel_cb_t *,
                                                  void *,
                                                  ...);

#define n00b_new_callback_channel(callback, param, ...) \
    _n00b_new_callback_channel((void *)(callback),      \
                               param,                   \
                               __VA_ARGS__ __VA_OPT__(, ) 0ULL)
