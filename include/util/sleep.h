#pragma once
#include "n00b.h"

static inline bool
n00b_nanosleep_raw(n00b_duration_t *rqtp, n00b_duration_t *rmtp)
{
    int result;
    N00B_DBG_CALL(n00b_thread_suspend);
    result = nanosleep(rqtp, rmtp);
    N00B_DBG_CALL(n00b_thread_resume);

    return result != 0;
}

static inline void
n00b_nanosleep(uint64_t s, uint64_t ns)
{
    struct timespec ts        = {.tv_sec = s, .tv_nsec = ns};
    struct timespec remainder = ts;

    N00B_DBG_CALL(n00b_thread_suspend);
    while (nanosleep((void *)&ts, (void *)&remainder)) {
        ts = remainder;
    }
    N00B_DBG_CALL(n00b_thread_resume);
}

static inline void
n00b_sleep_ms(int ms)
{
    struct timespec *ts        = (struct timespec *)n00b_duration_from_ms(ms);
    struct timespec  remainder = {.tv_sec = 0, .tv_nsec = 0};

    N00B_DBG_CALL(n00b_thread_suspend);
    while (nanosleep((void *)ts, (void *)&remainder)) {
        *ts = remainder;
    }
    N00B_DBG_CALL(n00b_thread_resume);
}
