#pragma once
#include "n00b.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#define N00B_PCRE2_ERR_LEN    120 // More than needed according to docs.

#include "pcre2.h"

// Laid out w/ the string repr first, so we can easily treat it like a
// string when we need to (primarily when we go to hash it).
typedef struct {
    n00b_string_t repr;
    pcre2_code   *regex;
} n00b_regex_t;

typedef struct {
    n00b_string_t *input_string;
    int64_t        start;
    int64_t        end;
    n00b_list_t   *captures;
} n00b_match_t;

extern pcre2_compile_context *n00b_pcre2_compile;
extern pcre2_match_context   *n00b_pcre2_match;
extern n00b_list_t           *n00b_regex_raw_match(n00b_regex_t *,
                                                   n00b_string_t *,
                                                   int,
                                                   bool exact,
                                                   bool no_eol,
                                                   bool all);
static inline n00b_regex_t *
n00b_regex(n00b_string_t *s)
{
    return n00b_new(n00b_type_regex(), s);
}

static inline n00b_regex_t *
n00b_regex_unanchored(n00b_string_t *s)
{
    return n00b_new(n00b_type_regex(), s, n00b_kw("anchored", n00b_ka(false)));
}

static inline n00b_regex_t *
n00b_regex_multiline(n00b_string_t *s, bool anchored)
{
    return n00b_new(n00b_type_regex(),
                    s,
                    n00b_kw("anchored",
                            n00b_ka(anchored),
                            "multiline",
                            n00b_ka(true)));
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
