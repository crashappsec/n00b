#pragma once
#include "n00b.h"

extern n00b_duration_t *n00b_now(void);
extern n00b_duration_t *n00b_timestamp(void);
extern n00b_duration_t *n00b_process_cpu(void);
extern n00b_duration_t *n00b_thread_cpu(void);
extern n00b_duration_t *n00b_uptime(void);
extern n00b_duration_t *n00b_program_clock(void);
extern void            n00b_init_program_timestamp(void);
extern n00b_duration_t *n00b_duration_diff(n00b_duration_t *,
                                         n00b_duration_t *);
