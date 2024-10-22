#pragma once
#include "n00b.h"

typedef n00b_filter_fn(n00b_stream_t *, n00b_stream_t *, void *);

n00b_stream_t *
n00b_stream_wrap(n00b_stream_t *, n00b_stream_t *, n00b_filter_fn, void *);
