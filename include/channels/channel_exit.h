#pragma once
#include "n00b.h"

typedef struct {
    int             pid;
    int             stats;
    struct rusage   usage;
    bool            exited;
    bool            err;
    n00b_channel_t *cref;
    n00b_thread_t  *waiter;
} n00b_exit_info_t;

extern n00b_channel_t *n00b_new_exit_channel(int64_t);
