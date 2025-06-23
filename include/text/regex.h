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
