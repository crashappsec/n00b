#pragma once

#include "n00b.h"

#define n00b_dict(x, y)                n00b_new(n00b_type_dict(x, y))
#define n00b_dict_with_filter(x, y, d) n00b_new(n00b_type_dict(x, y), 1ULL, d)
extern n00b_dict_t *n00b_dict_copy(n00b_dict_t *);
extern n00b_list_t *n00b_dict_keys(n00b_dict_t *);
extern n00b_list_t *n00b_dict_values(n00b_dict_t *);

#ifdef N00B_USE_INTERNAL_API
extern n00b_dict_t *n00b_new_unmanaged_dict(size_t, bool, bool);
#endif

static inline bool
n00b_dict_contains(n00b_dict_t *d, void *k)
{
    bool result;

    hatrack_dict_get(d, k, &result);

    return result;
}
