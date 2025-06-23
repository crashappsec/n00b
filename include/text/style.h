#pragma once
#include "n00b.h"

#define N00B_STYLE_EXTEND -1
extern n00b_string_t       *n00b_string_style(n00b_string_t *,
                                              n00b_text_element_t *);
extern n00b_string_t       *n00b_string_style_extended(n00b_string_t *,
                                                       n00b_text_element_t *);
extern char                *n00b_rich_to_ansi(n00b_string_t *, n00b_theme_t *);
extern n00b_string_t       *_n00b_string_style_ranges(n00b_string_t *,
                                                      n00b_text_element_t *,
                                                      int64_t,
                                                      int64_t,
                                                      ...);
extern n00b_text_element_t *n00b_text_style_overlay(n00b_text_element_t *,
                                                    n00b_text_element_t *);

#define n00b_string_style_ranges(str, style, start, end, ...) \
    _n00b_string_style_ranges(str, style, start, end, N00B_VA(__VA_ARGS__))

#define n00b_string_style_range(str, style, start, end) \
    _n00b_string_style_ranges(str, style, start, end, NULL)
