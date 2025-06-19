#pragma once
#include "n00b.h"

#define N00B_USEC_PER_SEC 1000000
#define N00B_NSEC_PER_SEC 1000000000
#define N00B_MS_PER_SEC   1000
#define N00B_NS_PER_MS    1000000

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
    return n00b_new(n00b_type_duration(),
                    n00b_header_kargs("timeval", (int64_t)s));
}

static inline void
n00b_write_now(n00b_duration_t *output)
{
    clock_gettime(CLOCK_REALTIME, (struct timespec *)output);
    output->tv_nsec *= N00B_NS_PER_US;
}

static inline int64_t
n00b_ns_from_duration(n00b_duration_t *d)
{
    return d->tv_sec * N00B_NS_PER_SEC + d->tv_nsec;
}

static inline int64_t
n00b_ns_minus(n00b_duration_t *d1, n00b_duration_t *d2)
{
    int64_t ns1 = n00b_ns_from_duration(d1);
    int64_t ns2 = n00b_ns_from_duration(d2);

    return ns1 - ns2;
}

static inline int64_t
n00b_ns_timestamp(void)
{
    n00b_duration_t d;
    clock_gettime(CLOCK_MONOTONIC, (void *)&d);
    d.tv_nsec *= N00B_NS_PER_US;

    return n00b_ns_from_duration(&d);
}

static inline int64_t
n00b_us_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * N00B_USEC_PER_SEC * tv.tv_usec;
}
