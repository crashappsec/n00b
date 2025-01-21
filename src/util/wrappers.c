#include "n00b.h"

// Currently not supporting the keyword argument, so need to add the
// null terminator.
n00b_utf32_t *
n00b_wrapper_join(n00b_list_t *l, const n00b_str_t *joiner)
{
    return n00b_str_join(l, joiner);
}

n00b_str_t *
n00b_wrapper_repr(n00b_obj_t obj)
{
    return n00b_repr(obj);
}

n00b_str_t *
n00b_wrapper_to_str(n00b_obj_t obj)
{
    n00b_type_t *t = n00b_type_resolve(n00b_get_my_type(obj));
    return n00b_to_str(obj, t);
}

n00b_str_t *
n00b_wrapper_hostname(void)
{
    struct utsname info;

    uname(&info);

    return n00b_new_utf8(info.nodename);
}

n00b_str_t *
n00b_wrapper_os(void)
{
    struct utsname info;

    uname(&info);

    return n00b_new_utf8(info.sysname);
}

n00b_str_t *
n00b_wrapper_arch(void)
{
    struct utsname info;

    uname(&info);

    return n00b_new_utf8(info.machine);
}

void
n00b_snap_column(n00b_grid_t *table, int64_t n)
{
    n00b_set_column_style(table, n, n00b_new_utf8("snap"));
}

uint64_t
n00b_clz(uint64_t n)
{
    return __builtin_clzll(n);
}
