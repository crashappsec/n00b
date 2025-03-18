#pragma once

#ifdef N00B_DEBUG
extern int
n00b_show_assert_failure(char *expression,
                         char *func,
                         char *file,
                         int   line);

extern void n00b_assertion_init(void);

#define n00b_assert(expression)                          \
    (((expression)                                       \
      || (n00b_show_assert_failure(#expression,          \
                                   (char *)__FUNCTION__, \
                                   (char *)__FILE__,     \
                                   __LINE__))))

#else
#define n00b_assert(x)
#define n00b_assertion_init()
#endif

#if defined(__linux__)
#pragma GCC diagnostic ignored "-Wunused-value"
#endif
