#pragma once
#include "n00b.h"

static inline n00b_date_time_t *
n00b_current_date_time(void)
{
    time_t now = time(NULL);
    return n00b_new(n00b_type_datetime(),
                    n00b_header_kargs("timestamp", (int64_t)now));
}

static inline n00b_date_time_t *
n00b_current_date(void)
{
    time_t now = time(NULL);
    return n00b_new(n00b_type_date(),
                    n00b_header_kargs("timestamp", (int64_t)now));
}

static inline n00b_date_time_t *
n00b_current_time(void)
{
    time_t now = time(NULL);
    return n00b_new(n00b_type_time(),
                    n00b_header_kargs("timestamp", (int64_t)now));
}

static inline n00b_date_time_t *
n00b_datetime_from_duration(n00b_duration_t *dur)
{
    return n00b_new(n00b_type_datetime(),
                    n00b_header_kargs("timestamp",
                                      (int64_t)dur->tv_sec,
                                      "fracsec",
                                      (int64_t)dur->tv_nsec));
}
