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
