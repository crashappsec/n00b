#pragma once
#include "n00b.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#define N00B_PCRE2_ERR_LEN    120 // More than needed according to docs.

#include "pcre2.h"

typedef pcre2_code n00b_regex_t;
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
n00b_create_regex(n00b_string_t *s,
                  bool           anchored,
                  bool           insensitive,
                  bool           multiline)
{
    int           err;
    PCRE2_SIZE    eoffset;
    n00b_regex_t *result;
    int           flags = PCRE2_ALLOW_EMPTY_CLASS | PCRE2_ALT_BSUX
              | PCRE2_NEVER_BACKSLASH_C | PCRE2_UCP | PCRE2_NO_UTF_CHECK;

    flags |= (PCRE2_ANCHORED * (int)anchored);
    flags |= (PCRE2_CASELESS * (int)insensitive);
    flags |= (PCRE2_MULTILINE * (int)multiline);

    result = pcre2_compile((PCRE2_SPTR8)s->data,
                           s->u8_bytes,
                           flags,
                           &err,
                           &eoffset,
                           n00b_pcre2_compile);

    if (!result) {
        PCRE2_UCHAR8 buf[N00B_PCRE2_ERR_LEN];
        pcre2_get_error_message(err, buf, N00B_PCRE2_ERR_LEN);
        N00B_RAISE(n00b_cstring(buf));
    }

    return result;
}

static inline n00b_regex_t *
n00b_regex_any_non_empty(void)
{
    return n00b_create_regex(n00b_cstring(".+"), false, false, false);
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
