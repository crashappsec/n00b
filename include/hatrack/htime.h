#pragma once

#include <time.h>

// #ifdef __MACH__
#if 0
#include <mach/clock.h>
#include <mach/mach.h>

extern _Bool        clock_service_inited;
extern clock_serv_t clock_service;

typedef mach_timespec_t hatrack_duration_t;
#define hatrack_get_timestamp(x)                 \
    if (!clock_service_inited) {                 \
        host_get_clock_service(mach_host_self(), \
                               CALENDAR_CLOCK,   \
                               &clock_service);  \
    }                                            \
    clock_gettime(clock_service, (x))
#else
typedef struct timespec hatrack_duration_t;
#define hatrack_get_timestamp(x) clock_gettime(CLOCK_MONOTONIC, (x))
#endif
