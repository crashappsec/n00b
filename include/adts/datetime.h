#pragma once
#include "n00b.h"

typedef struct n00b_date_time_t {
    struct tm    dt;
    int64_t      fracsec;
    unsigned int have_time     : 1;
    unsigned int have_sec      : 1;
    unsigned int have_frac_sec : 1;
    unsigned int have_month    : 1;
    unsigned int have_year     : 1;
    unsigned int have_day      : 1;
    unsigned int have_offset   : 1;
} n00b_date_time_t;

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
