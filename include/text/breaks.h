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

static inline n00b_break_info_t *
n00b_alloc_break_structure(n00b_string_t *s, int shift)
{
    n00b_break_info_t *result;
    int32_t            alloc_slots = n00b_max(n00b_string_codepoint_len(s)
                                       >> shift,
                                   n00b_minimum_break_slots);

    result = n00b_gc_flex_alloc(n00b_break_info_t, int32_t, alloc_slots, NULL);

    result->num_slots  = alloc_slots;
    result->num_breaks = 0;

    return result;
}

static inline n00b_break_info_t *
n00b_grow_break_structure(n00b_break_info_t *breaks)
{
    int32_t new_slots = breaks->num_slots * 2;

    n00b_break_info_t *res;

    res = n00b_gc_flex_alloc(n00b_break_info_t, int32_t, new_slots, NULL);

    res->num_slots  = new_slots;
    res->num_breaks = breaks->num_breaks;

    memcpy(res->breaks, breaks->breaks, breaks->num_slots * sizeof(int32_t));

    return res;
}

static inline void
n00b_add_break(n00b_break_info_t **listp, int32_t br)
{
    n00b_break_info_t *breaks = *listp;

    if (breaks->num_slots == breaks->num_breaks) {
        breaks = n00b_grow_break_structure(breaks);
        *listp = breaks;
    }

    breaks->breaks[breaks->num_breaks++] = br;
}

static inline void
n00b_undo_last_break(n00b_break_info_t *bi)
{
    bi->breaks[--bi->num_breaks] = 0;
}

// New interface for the new strings.
extern n00b_break_info_t *n00b_breaks_grapheme(n00b_string_t *,
                                               int32_t,
                                               int32_t);
extern n00b_break_info_t *n00b_breaks_line(n00b_string_t *);
extern n00b_break_info_t *n00b_breaks_all_options(n00b_string_t *);
extern n00b_break_info_t *n00b_break_wrap(n00b_string_t *, int32_t, int32_t);

static inline n00b_break_info_t *
n00b_break_alloc(n00b_string_t *s, int shift)
{
    n00b_break_info_t *result;
    int32_t            slots;

    slots = n00b_max(n00b_string_codepoint_len(s) >> shift,
                     n00b_minimum_break_slots);

    result = n00b_gc_flex_alloc(n00b_break_info_t, int32_t, slots, NULL);

    result->num_slots  = slots;
    result->num_breaks = 0;

    return result;
}

static inline n00b_break_info_t *
n00b_break_grow(n00b_break_info_t *breaks)
{
    int32_t new_slots = breaks->num_slots * 2;

    n00b_break_info_t *res;

    res = n00b_gc_flex_alloc(n00b_break_info_t, int32_t, new_slots, NULL);

    res->num_slots  = new_slots;
    res->num_breaks = breaks->num_breaks;

    memcpy(res->breaks, breaks->breaks, breaks->num_slots * sizeof(int32_t));

    return res;
}

static inline void
n00b_break_add(n00b_break_info_t **listp, int32_t br)
{
    n00b_break_info_t *breaks = *listp;

    if (breaks->num_slots == breaks->num_breaks) {
        breaks = n00b_break_grow(breaks);
        *listp = breaks;
    }

    breaks->breaks[breaks->num_breaks++] = br;
}
