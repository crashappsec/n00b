#include "n00b.h"
#pragma once

typedef struct n00b_fmt_spec_t {
    n00b_codepoint_t fill;
    int64_t         width;
    int64_t         precision;
    unsigned int    kind  : 2; // N00B_FMT_FMT_ONLY / NUMBERED / NAMED
    unsigned int    align : 2;
    unsigned int    sign  : 2;
    unsigned int    sep   : 2;
    unsigned int    empty : 1;
    n00b_codepoint_t type;
} n00b_fmt_spec_t;

typedef struct n00b_fmt_info_t {
    union {
        char   *name;
        int64_t position;
    } reference;
    struct n00b_fmt_info_t *next;
    n00b_fmt_spec_t         spec;
    int                    start;
    int                    end;
} n00b_fmt_info_t;

#define N00B_FMT_FMT_ONLY 0
#define N00B_FMT_NUMBERED 1
#define N00B_FMT_NAMED    2

// <, >, =
#define N00B_FMT_ALIGN_LEFT   0
#define N00B_FMT_ALIGN_RIGHT  1
#define N00B_FMT_ALIGN_CENTER 2

// +, -, ' '
#define N00B_FMT_SIGN_DEFAULT   0
#define N00B_FMT_SIGN_ALWAYS    1
#define N00B_FMT_SIGN_POS_SPACE 2

// "_", ","

#define N00B_FMT_SEP_DEFAULT 0
#define N00B_FMT_SEP_COMMA   1
#define N00B_FMT_SEP_USCORE  2

/*
  [[fill]align][sign][width][grouping_option]["." precision][type]
fill            ::=  *
align           ::=  "<" | ">" | "^"
sign            ::=  "+" | "-" | " "
width           ::=  digit+
grouping_option ::=  "_" | ","
precision       ::=  digit+
type            ::=  [^0-9<>^_, ]
*/
