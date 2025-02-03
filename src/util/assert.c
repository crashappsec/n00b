#include "n00b.h"

#ifdef N00B_DEBUG

#define IX_FAIL 0
#define IX_FUNC 1
#define IX_LOC  2

#if !defined(N00B_BACKTRACE_SUPPORTED)
#define NUM_ASSERTION_STRS 3
#else
#define IX_BT              3
#define NUM_ASSERTION_STRS 4
#endif

static n00b_utf8_t *n00b_assert_strs[NUM_ASSERTION_STRS];

int
n00b_show_assert_failure(char *expr, char *func, char *file, int line)
{
    fprintf(stderr,
            "%s %s\n%s %s %s %s:%d",
            n00b_assert_strs[IX_FAIL]->data,
            expr,
            n00b_assert_strs[IX_FUNC]->data,
            func,
            n00b_assert_strs[IX_LOC]->data,
            file,
            line);

#if defined(N00B_BACKTRACE_SUPPORTED)
    fprintf(stderr, "%s", n00b_assert_strs[IX_BT]->data);
    n00b_static_c_backtrace();
#endif
    fprintf(stderr, "\n");
    abort();
    return 0;
}

void
n00b_assertion_init(void)
{
    n00b_gc_register_root(&n00b_assert_strs[0], NUM_ASSERTION_STRS);

    n00b_assert_strs[IX_FAIL] = n00b_rich_lit("[h4]ASSERTION FAILED:[/] ");
    n00b_assert_strs[IX_FUNC] = n00b_rich_lit("[h1]Function:[/] ");
    n00b_assert_strs[IX_LOC]  = n00b_rich_lit("[h1]Location:[/] ");
#ifdef N00B_BACKTRACE_SUPPORTED
    n00b_assert_strs[IX_BT] = n00b_rich_lit("\n[h4]Backtrace:[/]\n");
#endif
}
#endif
