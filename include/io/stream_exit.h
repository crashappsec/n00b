#pragma once
#include "n00b.h"

typedef struct {
    int            pid;
    int            stats;
    struct rusage  usage;
    bool           exited;
    bool           err;
    n00b_thread_t *waiter;
} n00b_exit_info_t;

extern n00b_stream_t *n00b_new_exit_stream(int64_t);
