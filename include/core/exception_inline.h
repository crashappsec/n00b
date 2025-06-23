#pragma once
#include "n00b.h"

static inline n00b_string_t *
n00b_exception_get_file(n00b_exception_t *exception)
{
    return n00b_new(n00b_type_string(),
                    n00b_header_kargs("cstring", (int64_t)exception->file));
}

static inline uint64_t
n00b_exception_get_line(n00b_exception_t *exception)
{
    return exception->line;
}

static inline n00b_string_t *
n00b_exception_get_message(n00b_exception_t *exception)
{
    return exception->msg;
}

void n00b_exception_register_uncaught_handler(void (*)(n00b_exception_t *));

#ifdef _GNU_SOURCE
#define N00B_TURN_ON_GS
#undef _GNU_SOURCE
#endif

static inline n00b_string_t *
get_errno_message(int code)
{
    char buf[BUFSIZ];
    if (strerror_r(code, buf, BUFSIZ) != 0) {
        return n00b_cstring("Unknown error");
    }

    return n00b_cstring(buf);
}

#ifdef N00B_TURN_ON_GS
#define _GNU_SOURCE
#endif

#define n00b_raise_errcode(code) N00B_RAISE(get_errno_message((code)))

#define n00b_raise_errno()                                           \
    {                                                                \
        N00B_RAISE(get_errno_message(errno),                         \
                   n00b_header_kargs("error_code", (int64_t)errno)); \
    }

#define n00b_unreachable()                                    \
    {                                                         \
        n00b_string_t *s = n00b_cformat(                      \
            "Reached code that the developer "                \
            "(wrongly) believed was unreachable, at «#»:«#»", \
            n00b_cstring(__FILE__),                           \
            (int64_t)__LINE__);                               \
        N00B_RAISE(s);                                        \
    }

extern void n00b_default_uncaught_handler(n00b_exception_t *exception);
extern void n00b_print_exception(n00b_exception_t *, n00b_string_t *);
