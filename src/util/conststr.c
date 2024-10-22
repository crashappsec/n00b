#include "n00b.h"

enum {
    LBRAK_IX     = 0,
    COMMA_IX     = 1,
    RBRAK_IX     = 2,
    LPAREN_IX    = 3,
    RPAREN_IX    = 4,
    ARROW_IX     = 5,
    BTICK_IX     = 6,
    STAR_IX      = 7,
    SPACE_IX     = 8,
    LBRACE_IX    = 9,
    RBRACE_IX    = 10,
    COLON_IX     = 11,
    COLON_NSP    = 12,
    SLASH_IX     = 13,
    PERIOD_IX    = 14,
    EMPTY_FMT_IX = 15,
    NEWLINE_IX   = 16,
    PUNC_MAX     = 17,
};

static n00b_str_t *type_punct[PUNC_MAX] = {
    0,
};

static inline void
init_punctuation()
{
    if (type_punct[0] == NULL) {
        type_punct[LBRAK_IX]     = n00b_utf8_repeat('[', 1);
        type_punct[COMMA_IX]     = n00b_new(n00b_type_utf8(),
                                       n00b_kw("cstring", n00b_ka(", ")));
        type_punct[RBRAK_IX]     = n00b_utf8_repeat(']', 1);
        type_punct[LPAREN_IX]    = n00b_utf8_repeat('(', 1);
        type_punct[RPAREN_IX]    = n00b_utf8_repeat(')', 1);
        type_punct[ARROW_IX]     = n00b_new(n00b_type_utf8(),
                                       n00b_kw("cstring", n00b_ka(" -> ")));
        type_punct[BTICK_IX]     = n00b_utf8_repeat('`', 1);
        type_punct[STAR_IX]      = n00b_utf8_repeat('*', 1);
        type_punct[SPACE_IX]     = n00b_utf8_repeat(' ', 1);
        type_punct[LBRACE_IX]    = n00b_utf8_repeat('{', 1);
        type_punct[RBRACE_IX]    = n00b_utf8_repeat('}', 1);
        type_punct[COLON_IX]     = n00b_new(n00b_type_utf8(),
                                       n00b_kw("cstring", n00b_ka(" : ")));
        type_punct[COLON_NSP]    = n00b_utf8_repeat(':', 1);
        type_punct[SLASH_IX]     = n00b_utf8_repeat('/', 1);
        type_punct[EMPTY_FMT_IX] = n00b_new(n00b_type_utf8(),
                                           n00b_kw("cstring", n00b_ka("{}")));
        type_punct[NEWLINE_IX]   = n00b_utf8_repeat('\n', 1);
        n00b_gc_register_root(&type_punct[0], PUNC_MAX);
    }
}

n00b_utf8_t *
n00b_get_lbrak_const()
{
    init_punctuation();
    return type_punct[LBRAK_IX];
}

n00b_utf8_t *
n00b_get_comma_const()
{
    init_punctuation();
    return type_punct[COMMA_IX];
}

n00b_utf8_t *
n00b_get_rbrak_const()
{
    init_punctuation();
    return type_punct[RBRAK_IX];
}

n00b_utf8_t *
n00b_get_lparen_const()
{
    init_punctuation();
    return type_punct[LPAREN_IX];
}

n00b_utf8_t *
n00b_get_rparen_const()
{
    init_punctuation();
    return type_punct[RPAREN_IX];
}

n00b_utf8_t *
n00b_get_arrow_const()
{
    init_punctuation();
    return type_punct[ARROW_IX];
}

n00b_utf8_t *
n00b_get_backtick_const()
{
    init_punctuation();
    return type_punct[BTICK_IX];
}

n00b_utf8_t *
n00b_get_asterisk_const()
{
    init_punctuation();
    return type_punct[STAR_IX];
}

n00b_utf8_t *
n00b_get_space_const()
{
    init_punctuation();
    return type_punct[SPACE_IX];
}

n00b_utf8_t *
n00b_get_lbrace_const()
{
    init_punctuation();
    return type_punct[LBRACE_IX];
}

n00b_utf8_t *
n00b_get_rbrace_const()
{
    init_punctuation();
    return type_punct[RBRACE_IX];
}

n00b_utf8_t *
n00b_get_colon_const()
{
    init_punctuation();
    return type_punct[COLON_IX];
}

n00b_utf8_t *
n00b_get_colon_no_space_const()
{
    init_punctuation();
    return type_punct[COLON_NSP];
}

n00b_utf8_t *
n00b_get_slash_const()
{
    init_punctuation();
    return type_punct[SLASH_IX];
}

n00b_utf8_t *
n00b_get_period_const()
{
    init_punctuation();
    return type_punct[PERIOD_IX];
}

n00b_utf8_t *
n00b_get_empty_fmt_const()
{
    init_punctuation();
    return type_punct[EMPTY_FMT_IX];
}

n00b_utf8_t *
n00b_get_newline_const()
{
    init_punctuation();
    return type_punct[NEWLINE_IX];
}

n00b_utf8_t *
n00b_in_parens(n00b_str_t *s)
{
    return n00b_to_utf8(n00b_str_concat(n00b_get_lparen_const(),
                                      n00b_str_concat(n00b_to_utf8(s),
                                                     n00b_get_rparen_const())));
}
