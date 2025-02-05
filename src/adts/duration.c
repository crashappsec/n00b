#include "n00b.h"

n00b_duration_t *
n00b_now(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_REALTIME, result);

    return result;
}

n00b_duration_t *
n00b_timestamp(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_MONOTONIC, result);

    return result;
}

n00b_duration_t *
n00b_process_cpu(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, result);

    return result;
}

n00b_duration_t *
n00b_thread_cpu(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, result);

    return result;
}

#if defined(__MACH__)
n00b_duration_t *
n00b_uptime(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_UPTIME_RAW, result);

    return result;
}
#elif defined(__linux__)
// In Posix, the MONOTONIC clock's reference frame is arbitrary.
// In Linux, it's uptime, and this is higher precisssion than using
// sysinfo(), which only has second resolution.
n00b_duration_t *
n00b_uptime(void)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    clock_gettime(CLOCK_MONOTONIC, result);

    return result;
}
#else
#error "Unsupported system."
#endif

static n00b_duration_t *monotonic_start;

void
n00b_init_program_timestamp(void)
{
    n00b_gc_register_root(&monotonic_start, 1);
    monotonic_start = n00b_timestamp();
}

n00b_duration_t *
n00b_program_clock(void)
{
    n00b_duration_t *now = n00b_timestamp();

    return n00b_duration_diff(now, monotonic_start);
}

static n00b_list_t *
duration_atomize(n00b_string_t *s)
{
    n00b_string_t *one;
    n00b_list_t   *result = n00b_list(n00b_type_string());
    int            l      = n00b_string_byte_len(s);
    char          *p      = s->data;
    char          *end    = p + l;
    int            start  = 0;
    int            cur    = 0;

    while (p < end) {
        while (p < end && isdigit(*p)) {
            cur++;
            p++;
        }
        if (start == cur) {
            return NULL;
        }
        one = n00b_string_slice(s, start, cur);
        n00b_list_append(result, one);

        while (p < end && (isspace(*p) || *p == ',')) {
            cur++;
            p++;
        }

        start = cur;

        while (p < end && !isdigit(*p) && *p != ' ' && *p != ',') {
            cur++;
            p++;
        }
        if (start == cur) {
            return NULL;
        }

        one = n00b_string_slice(s, start, cur);
        n00b_list_append(result, one);

        while (p < end && (isspace(*p) || *p == ',')) {
            cur++;
            p++;
        }
        start = cur;
    }

    return result;
}

#define SEC_PER_MIN   60
#define SEC_PER_HR    (SEC_PER_MIN * 60)
#define SEC_PER_DAY   (SEC_PER_HR * 24)
#define SEC_PER_WEEK  (SEC_PER_DAY * 7)
#define SEC_PER_YEAR  (SEC_PER_DAY * 365)
#define NS_PER_MS     1000000
#define NS_PER_US     1000
#define N00B_MAX_UINT (~0ULL)

static inline int64_t
tv_sec_multiple(n00b_string_t *s)
{
    // clang-format off
    switch (s->data[0]) {
    case 's':
	if (!strcmp(s->data, "s") ||
	    !strcmp(s->data, "sec") ||
	    !strcmp(s->data, "secs") ||
	    !strcmp(s->data, "seconds")) {
	    return 1;
	}
	return 0;
    case 'm':
	if (!strcmp(s->data, "m") ||
	    !strcmp(s->data, "min") ||
	    !strcmp(s->data, "mins") ||
	    !strcmp(s->data, "minutes")) {
	    return SEC_PER_MIN;
	}
	return 0;
    case 'h':
	if (!strcmp(s->data, "h") ||
	    !strcmp(s->data, "hr") ||
	    !strcmp(s->data, "hrs") ||
	    !strcmp(s->data, "hours")) {
	    return SEC_PER_HR;
	}
	return 0;
    case 'd':
	if (!strcmp(s->data, "d") ||
	    !strcmp(s->data, "day") ||
	    !strcmp(s->data, "days")) {
	    return SEC_PER_DAY;
	}
	return 0;
    case 'w':
	if (!strcmp(s->data, "w") ||
	    !strcmp(s->data, "wk") ||
	    !strcmp(s->data, "wks") ||
	    !strcmp(s->data, "week") ||
	    !strcmp(s->data, "weeks")) {
	    return SEC_PER_WEEK;
	}
	return 0;
    case 'y':
	if (!strcmp(s->data, "y") ||
	    !strcmp(s->data, "yr") ||
	    !strcmp(s->data, "yrs") ||
	    !strcmp(s->data, "year") ||
	    !strcmp(s->data, "years")) {
	    return SEC_PER_YEAR;
	}
	return 0;
    default:
	return 0;
    }
}

static inline int64_t
tv_nano_multiple(n00b_string_t *s)
{
    // For sub-seconds, we convert to nanoseconds.
    switch (s->data[0]) {
    case 'n':
	if (!strcmp(s->data, "n") ||
	    !strcmp(s->data, "ns") ||
	    !strcmp(s->data, "nsec") ||
	    !strcmp(s->data, "nsecs") ||
	    !strcmp(s->data, "nanosec") ||
	    !strcmp(s->data, "nanosecs") ||
	    !strcmp(s->data, "nanosecond") ||
	    !strcmp(s->data, "nanoseconds")) {
	    return 1;
	}
	return 0;
    case 'm':
	if (!strcmp(s->data, "m") ||
	    !strcmp(s->data, "ms") ||
	    !strcmp(s->data, "msec") ||
	    !strcmp(s->data, "msecs") ||
	    !strcmp(s->data, "millisec") ||
	    !strcmp(s->data, "millisecs") ||
	    !strcmp(s->data, "millisecond") ||
	    !strcmp(s->data, "milliseconds")) {
	    return NS_PER_MS;
	}
	if (!strcmp(s->data, "microsec") ||
	    !strcmp(s->data, "microsecs") ||
	    !strcmp(s->data, "microsecond") ||
	    !strcmp(s->data, "microseconds")) {
	    return NS_PER_US;
	}
	return 0;
    case 'u':
	if (!strcmp(s->data, "u") ||
	    !strcmp(s->data, "us") ||
	    !strcmp(s->data, "usec") ||
	    !strcmp(s->data, "usecs") ||
	    !strcmp(s->data, "usecond") ||
	    strcmp(s->data, "useconds")) {
	    return NS_PER_US;
	}
	return 0;
    default:
	return 0;
    }
    // clang-format on
}

static bool
str_to_duration(n00b_string_t        *s,
                struct timespec      *ts,
                n00b_compile_error_t *err)
{
    n00b_list_t *atoms = duration_atomize(s);

    if (!atoms) {
        *err = n00b_err_invalid_duration_lit;
        return false;
    }

    int            i   = 0;
    int            n   = n00b_list_len(atoms);
    __uint128_t    sec = 0;
    __uint128_t    sub = 0;
    __uint128_t    tmp;
    int64_t        multiple;
    bool           neg;
    n00b_string_t *tmpstr;

    while (i < n) {
        tmp = n00b_raw_int_parse(n00b_list_get(atoms, i++, NULL), err, &neg);

        if (neg) {
            *err = n00b_err_invalid_duration_lit;
            return false;
        }

        if (*err != n00b_err_no_error) {
            *err = n00b_err_invalid_duration_lit;
            return false;
        }

        if (tmp > N00B_MAX_UINT) {
            *err = n00b_err_parse_lit_overflow;
            return false;
        }

        tmpstr   = n00b_list_get(atoms, i++, NULL);
        multiple = tv_sec_multiple(tmpstr);

        if (multiple) {
            sec += (multiple * tmp);
            if (sec > N00B_MAX_UINT) {
                *err = n00b_err_parse_lit_overflow;
                return false;
            }
            continue;
        }

        multiple = tv_nano_multiple(tmpstr);
        if (!multiple) {
            *err = n00b_err_invalid_duration_lit;
            return false;
        }

        sub += (multiple * tmp);
        if (sub > N00B_MAX_UINT) {
            *err = n00b_err_parse_lit_overflow;
            return false;
        }
    }

    ts->tv_sec  = sec;
    ts->tv_nsec = sub;

    return true;
}

static void
duration_init(struct timespec *ts, va_list args)
{
    n00b_string_t       *to_parse = NULL;
    int64_t              sec      = -1;
    int64_t              nanosec  = -1;
    struct timeval      *tv       = NULL;
    n00b_compile_error_t err;

    n00b_karg_va_init(args);
    n00b_kw_ptr("to_parse", args);
    n00b_kw_ptr("timeval", tv);
    n00b_kw_uint64("sec", sec);
    n00b_kw_uint64("nanosec", nanosec);

    if (tv) {
        TIMEVAL_TO_TIMESPEC(tv, ts);
        return;
    }

    if (to_parse) {
        if (!str_to_duration(to_parse, ts, &err)) {
            N00B_RAISE(n00b_err_code_to_str(err));
        }
        return;
    }

    if (sec < 0) {
        sec = 0;
    }
    if (nanosec < 0) {
        nanosec = 0;
    }

    ts->tv_sec  = sec;
    ts->tv_nsec = nanosec;
    return;
}

static n00b_string_t *
repr_sec(int64_t n)
{
    n00b_list_t   *l = n00b_list(n00b_type_string());
    n00b_string_t *s;
    int64_t        tmp;

    if (n >= SEC_PER_YEAR) {
        tmp = n / SEC_PER_YEAR;
        if (tmp > 1) {
            s = n00b_cformat("«#» years", tmp);
        }
        else {
            s = n00b_cstring("1 year");
        }

        n00b_list_append(l, s);
        n -= (tmp * SEC_PER_YEAR);
    }

    if (n >= SEC_PER_WEEK) {
        tmp = n / SEC_PER_WEEK;
        if (tmp > 1) {
            s = n00b_cformat("«#» weeks", tmp);
        }
        else {
            s = n00b_cstring("1 week");
        }

        n00b_list_append(l, s);
        n -= (tmp * SEC_PER_WEEK);
    }

    if (n >= SEC_PER_DAY) {
        tmp = n / SEC_PER_DAY;
        if (tmp > 1) {
            s = n00b_cformat("«#» days", tmp);
        }
        else {
            s = n00b_cstring("1 day");
        }

        n00b_list_append(l, s);
        n -= (tmp * SEC_PER_DAY);
    }

    if (n >= SEC_PER_HR) {
        tmp = n / SEC_PER_HR;
        if (tmp > 1) {
            s = n00b_cformat("«#» hours", tmp);
        }
        else {
            s = n00b_cstring("1 hour");
        }

        n00b_list_append(l, s);
        n -= (tmp * SEC_PER_HR);
    }

    if (n >= SEC_PER_MIN) {
        tmp = n / SEC_PER_MIN;
        if (tmp > 1) {
            s = n00b_cformat("«#» minutes", tmp);
        }
        else {
            s = n00b_cstring("1 minute");
        }

        n00b_list_append(l, s);
        n -= (tmp * SEC_PER_MIN);
    }

    if (n) {
        if (n == 1) {
            s = n00b_cstring("1 second");
        }
        else {
            s = n00b_cformat("«#» seconds", n);
        }
        n00b_list_append(l, s);
    }

    return n00b_string_join(l, n00b_cached_comma_padded());
}

static n00b_string_t *
repr_ns(int64_t n)
{
    int64_t ms = n / NS_PER_MS;
    int64_t ns = n - (ms * NS_PER_MS);
    int64_t us = ns / NS_PER_US;

    ns = ns - (us * NS_PER_US);

    n00b_list_t *parts = n00b_list(n00b_type_string());

    if (ms) {
        n00b_list_append(parts, n00b_cformat("«#:n» msec", ms));
    }
    if (us) {
        n00b_list_append(parts, n00b_cformat("«#:n» usec", us));
    }
    if (ns) {
        n00b_list_append(parts, n00b_cformat("«#:n» nsec", ms));
    }

    return n00b_string_join(parts, n00b_cached_comma_padded());
}

static n00b_string_t *
duration_repr(n00b_duration_t *ts)
{
    // TODO: Do better.

    if (!ts->tv_sec && !ts->tv_nsec) {
        return n00b_cstring("0 seconds");
    }

    if (ts->tv_sec && ts->tv_nsec) {
        return n00b_cformat("«#0» «#1»",
                            repr_sec(ts->tv_sec),
                            repr_ns(ts->tv_nsec));
    }

    if (ts->tv_sec) {
        return repr_sec(ts->tv_sec);
    }
    return repr_ns(ts->tv_nsec);
}

bool
n00b_duration_eq(n00b_duration_t *t1, n00b_duration_t *t2)
{
    return (t1->tv_sec == t2->tv_sec && t1->tv_nsec == t2->tv_nsec);
}

bool
n00b_duration_gt(n00b_duration_t *t1, n00b_duration_t *t2)
{
    if (t1->tv_sec > t2->tv_sec) {
        return true;
    }
    if (t1->tv_sec < t2->tv_sec) {
        return false;
    }

    return t1->tv_nsec > t2->tv_nsec;
}

bool
n00b_duration_lt(n00b_duration_t *t1, n00b_duration_t *t2)
{
    if (t1->tv_sec < t2->tv_sec) {
        return true;
    }
    if (t1->tv_sec > t2->tv_sec) {
        return false;
    }

    return t1->tv_nsec < t2->tv_nsec;
}

n00b_duration_t *
n00b_duration_diff(n00b_duration_t *t1, n00b_duration_t *t2)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());
    n00b_duration_t *b, *l;

    if (n00b_duration_gt(t1, t2)) {
        b = t1;
        l = t2;
    }
    else {
        b = t2;
        l = t1;
    }
    result->tv_nsec = b->tv_nsec - l->tv_nsec;
    result->tv_sec  = b->tv_sec - l->tv_sec;

    if (result->tv_nsec < 0) {
        result->tv_nsec += 1000000000;
        result->tv_sec -= 1;
    }

    return result;
}

n00b_duration_t *
n00b_duration_add(n00b_duration_t *t1, n00b_duration_t *t2)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    result->tv_nsec = t1->tv_nsec + t2->tv_nsec;
    result->tv_sec  = t1->tv_sec + t2->tv_sec;

    if (result->tv_nsec >= 1000000000) {
        result->tv_sec += 1;
        result->tv_nsec -= 1000000000;
    }

    return result;
}

static n00b_duration_t *
duration_lit(n00b_string_t        *s,
             n00b_lit_syntax_t     st,
             n00b_string_t        *mod,
             n00b_compile_error_t *err)
{
    n00b_duration_t *result = n00b_new(n00b_type_duration());

    if (str_to_duration(s, result, err)) {
        return result;
    }

    return NULL;
}

const n00b_vtable_t n00b_duration_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)duration_init,
        [N00B_BI_REPR]         = (n00b_vtable_entry)duration_repr,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)duration_lit,
        [N00B_BI_EQ]           = (n00b_vtable_entry)n00b_duration_eq,
        [N00B_BI_LT]           = (n00b_vtable_entry)n00b_duration_lt,
        [N00B_BI_GT]           = (n00b_vtable_entry)n00b_duration_gt,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_duration_add,
        [N00B_BI_SUB]          = (n00b_vtable_entry)n00b_duration_diff,
        [N00B_BI_FINALIZER]    = NULL,
    },
};
