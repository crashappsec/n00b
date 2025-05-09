#pragma once
#include "n00b.h"

extern n00b_duration_t *n00b_now(void);
extern n00b_duration_t *n00b_timestamp(void);
extern n00b_duration_t *n00b_process_cpu(void);
extern n00b_duration_t *n00b_thread_cpu(void);
extern n00b_duration_t *n00b_uptime(void);
extern n00b_duration_t *n00b_program_clock(void);
extern void             n00b_init_program_timestamp(void);
extern n00b_duration_t *n00b_duration_diff(n00b_duration_t *,
                                           n00b_duration_t *);
extern n00b_duration_t *n00b_duration_add(n00b_duration_t *,
                                          n00b_duration_t *);
extern bool             n00b_duration_eq(n00b_duration_t *,
                                         n00b_duration_t *);
extern bool             n00b_duration_gt(n00b_duration_t *,
                                         n00b_duration_t *);
extern bool             n00b_duration_lt(n00b_duration_t *,
                                         n00b_duration_t *);
extern n00b_duration_t *n00b_duration_multiply(n00b_duration_t *, double);
extern n00b_duration_t *n00b_new_ms_timeout(int);
extern n00b_duration_t *n00b_duration_from_ms(int);
extern int64_t          n00b_duration_to_ms(n00b_duration_t *);

static inline n00b_duration_t *
n00b_duration_divide(n00b_duration_t *dur, double f)
{
    return n00b_duration_multiply(dur, 1 / f);
}

static inline struct timeval *
n00b_duration_to_timeval(n00b_duration_t *d)
{
    if (!d) {
        return NULL;
    }
    struct timeval *r = n00b_gc_alloc_mapped(struct timeval,
                                             N00B_GC_SCAN_ALL);

    TIMESPEC_TO_TIMEVAL(r, d);

    return r;
}

static inline n00b_duration_t *
n00b_timeval_to_duration(struct timeval *s)
{
    return n00b_new(n00b_type_duration(), n00b_kw("timeval", s));
}
