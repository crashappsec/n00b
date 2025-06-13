#include "n00b.h"

// Currently not supporting the keyword argument, so need to add the
// null terminator.
n00b_string_t *
n00b_wrapper_join(n00b_list_t *l, n00b_string_t *joiner)
{
    return n00b_string_join(l, joiner);
}

n00b_string_t *
n00b_wrapper_repr(n00b_obj_t obj)
{
    n00b_string_t *result = n00b_to_literal(obj);

    if (!result) {
        result = n00b_cached_empty_string();
    }

    return result;
}

n00b_string_t *
n00b_wrapper_to_str(n00b_obj_t obj)
{
    return n00b_to_string(obj);
}

n00b_string_t *
n00b_wrapper_hostname(void)
{
    struct utsname info;

    uname(&info);

    return n00b_cstring(info.nodename);
}

n00b_string_t *
n00b_wrapper_os(void)
{
    struct utsname info;

    uname(&info);

    return n00b_cstring(info.sysname);
}

n00b_string_t *
n00b_wrapper_arch(void)
{
    struct utsname info;

    uname(&info);

    return n00b_cstring(info.machine);
}

uint64_t
n00b_clz(uint64_t n)
{
    return __builtin_clzll(n);
}
