#pragma once
#include "n00b.h"

typedef struct {
    int pipe[2];
} n00b_notifier_t;

n00b_notifier_t *n00b_new_notifier(void);

static inline void
n00b_notify(n00b_notifier_t *n, uint64_t value)
{
top:
    switch (write(n->pipe[1], &value, sizeof(uint64_t))) {
    case -1:
        if (errno == EINTR || errno == EAGAIN) {
            goto top;
        }
        n00b_unreachable();
    case sizeof(uint64_t):
        return;
    default:
        n00b_unreachable();
    }
}

extern int64_t n00b_wait(n00b_notifier_t *, int);
