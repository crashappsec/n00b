#pragma once
#include "n00b.h"

static inline n00b_regex_t *
n00b_regex(n00b_string_t *s)
{
    return n00b_new(n00b_type_regex(), s);
}

static inline n00b_regex_t *
n00b_regex_unanchored(n00b_string_t *s)
{
    return n00b_new(n00b_type_regex(), s, n00b_header_kargs("anchored", 0ULL));
}

static inline n00b_regex_t *
n00b_regex_multiline(n00b_string_t *s, bool anchored)
{
    return n00b_new(n00b_type_regex(),
                    s,
                    n00b_header_kargs("anchored",
                                      (int64_t)anchored,
                                      "multiline",
                                      1ULL));
}

static inline n00b_regex_t *
n00b_regex_any_non_empty(void)
{
    return n00b_regex(n00b_cstring(".+"));
}

static inline n00b_match_t *
n00b_match(n00b_regex_t *re, n00b_string_t *s)
{
    n00b_list_t *l = n00b_regex_raw_match(re, s, 0, false, false, false);

    if (!l || n00b_list_len(l) == 0) {
        return NULL;
    }

    return n00b_list_pop(l);
}

static inline n00b_list_t *
n00b_match_all(n00b_regex_t *re, n00b_string_t *s)
{
    return n00b_regex_raw_match(re, s, 0, false, false, true);
}

static inline n00b_match_t *
n00b_match_from(n00b_regex_t *re, n00b_string_t *s, int64_t ix)
{
    n00b_list_t *l = n00b_regex_raw_match(re, s, ix, false, false, false);

    if (n00b_list_len(l) == 0) {
        return NULL;
    }

    return n00b_list_pop(l);
}

static inline n00b_list_t *
n00b_match_all_from(n00b_regex_t *re, n00b_string_t *s, int64_t ix)
{
    return n00b_regex_raw_match(re, s, ix, false, false, true);
}
