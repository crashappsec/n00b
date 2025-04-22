#pragma once
#include "n00b.h"

static inline bool
n00b_nanosleep_raw(n00b_duration_t *rqtp, n00b_duration_t *rmtp)
{
    int result;
    n00b_gts_suspend();
    result = nanosleep(rqtp, rmtp);
    n00b_gts_resume();

    return result != 0;
}

static inline void
n00b_nanosleep(uint64_t s, uint64_t ns)
{
    struct timespec ts        = {.tv_sec = s, .tv_nsec = ns};
    struct timespec remainder = ts;

    n00b_gts_suspend();
    while (nanosleep((void *)&ts, (void *)&remainder)) {
        ts = remainder;
    }
    n00b_gts_resume();
}
