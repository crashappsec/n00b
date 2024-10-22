#pragma once
#include "n00b.h"

typedef struct n00b_date_time_t {
    struct tm    dt;
    int64_t     fracsec;
    unsigned int have_time     : 1;
    unsigned int have_sec      : 1;
    unsigned int have_frac_sec : 1;
    unsigned int have_month    : 1;
    unsigned int have_year     : 1;
    unsigned int have_day      : 1;
    unsigned int have_offset   : 1;
} n00b_date_time_t;
