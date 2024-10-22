#pragma once

#include "n00b.h"

#define n00b_codepoint_category(c)     utf8proc_category(c)
#define n00b_codepoint_valid(c)        utf8proc_codepoint_valid(c)
#define n00b_codepoint_lower(c)        utf8proc_tolower(c)
#define n00b_codepoint_upper(c)        utf8proc_toupper(c)
#define n00b_codepoint_width(c)        utf8proc_charwidth(c)
#define n00b_codepoint_is_printable(c) (n00b_codepoint_width(c) != 0)

static inline bool
n00b_codepoint_is_space(n00b_codepoint_t cp)
{
    // Fast path.
    if (cp == ' ' || cp == '\n' || cp == '\r') {
        return true;
    }

    switch (n00b_codepoint_category(cp)) {
    case UTF8PROC_CATEGORY_ZS:
        return true;
    case UTF8PROC_CATEGORY_ZL:
    case UTF8PROC_CATEGORY_ZP:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_codepoint_is_n00b_id_start(n00b_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
        return true;
    default:
        switch (cp) {
        case '_':
        case '?':
        case '$':
            return true;
        default:
            return false;
        }
    }
}

static inline bool
n00b_codepoint_is_id_start(n00b_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_codepoint_is_id_continue(n00b_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_MN:
    case UTF8PROC_CATEGORY_MC:
    case UTF8PROC_CATEGORY_PC:
        return true;
    default:
        return false;
    }
}

static inline bool
n00b_codepoint_is_n00b_id_continue(n00b_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
    case UTF8PROC_CATEGORY_MN:
    case UTF8PROC_CATEGORY_MC:
    case UTF8PROC_CATEGORY_PC:
        return true;
    default:
        switch (cp) {
        case '_':
        case '?':
        case '$':
            return true;
        default:
            return false;
        }
    }
}

static inline bool
n00b_codepoint_is_ascii_digit(n00b_codepoint_t cp)
{
    return cp >= '0' && cp <= '9';
}

static inline bool
n00b_codepoint_is_ascii_upper(n00b_codepoint_t cp)
{
    return cp >= 'A' && cp <= 'Z';
}

static inline bool
n00b_codepoint_is_ascii_lower(n00b_codepoint_t cp)
{
    return cp >= 'a' && cp <= 'z';
}

static inline bool
n00b_codepoint_is_unicode_lower(n00b_codepoint_t cp)
{
    return utf8proc_category(cp) == UTF8PROC_CATEGORY_LL;
}

static inline bool
n00b_codepoint_is_unicode_upper(n00b_codepoint_t cp)
{
    return utf8proc_category(cp) == UTF8PROC_CATEGORY_LU;
}

static inline bool
n00b_codepoint_is_unicode_digit(n00b_codepoint_t cp)
{
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_NL:
    case UTF8PROC_CATEGORY_ND:
        return true;
    default:
        return false;
    }
}
