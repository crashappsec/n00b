#pragma once

#include "n00b.h"

typedef struct break_info_st {
    int32_t num_slots;
    int32_t num_breaks;
    int32_t breaks[];
} n00b_break_info_t;

extern const int n00b_minimum_break_slots;

extern n00b_break_info_t *n00b_get_grapheme_breaks(const n00b_string_t *,
                                                   int32_t,
                                                   int32_t);
extern n00b_break_info_t *n00b_get_line_breaks(const n00b_string_t *);
extern n00b_break_info_t *n00b_get_all_line_break_ops(const n00b_string_t *);
extern n00b_break_info_t *n00b_wrap_text(const n00b_string_t *,
                                         int32_t,
                                         int32_t);
extern n00b_break_info_t *n00b_word_breaks(n00b_string_t *s);
// New interface for the new strings.
extern n00b_break_info_t *n00b_breaks_grapheme(n00b_string_t *,
                                               int32_t,
                                               int32_t);
extern n00b_break_info_t *n00b_breaks_line(n00b_string_t *);
extern n00b_break_info_t *n00b_breaks_all_options(n00b_string_t *);
extern n00b_break_info_t *n00b_break_wrap(n00b_string_t *, int32_t, int32_t);
