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

static inline n00b_string_t *
n00b_string_style_by_tag(n00b_string_t *s, n00b_string_t *tag)
{
    n00b_box_props_t *props = n00b_lookup_style_by_tag(tag);

    if (!props) {
        return s;
    }

    return n00b_string_style(s, &props->text_style);
}

static inline n00b_buf_t *
n00b_apply_ansi(n00b_string_t *s)
{
    // Will use the default theme.
    char *cstr = n00b_rich_to_ansi(s, NULL);
    int   n    = strlen(cstr);

    return n00b_new(n00b_type_buffer(),
                    n00b_kw("length", n00b_ka(n), "ptr", cstr));
}

static inline n00b_buf_t *
n00b_apply_ansi_with_theme(n00b_string_t *s, n00b_theme_t *theme)
{
    // Will use the default theme.
    char *cstr = n00b_rich_to_ansi(s, theme);
    int   n    = strlen(cstr);

    return n00b_new(n00b_type_buffer(),
                    n00b_kw("length", n00b_ka(n), "ptr", cstr));
}

#ifdef N00B_USE_INTERNAL_API

static inline bool
n00b_style_extends_end(n00b_style_record_t *si)
{
    return si->end <= N00B_STYLE_EXTEND;
}

static inline bool
n00b_style_extends_front(n00b_style_record_t *si)
{
    return si->start <= N00B_STYLE_EXTEND;
}

static inline int
n00b_style_end(n00b_string_t *s, int i)
{
    int val = s->styling->styles[i].end;

    if (val <= N00B_STYLE_EXTEND) {
        return s->codepoints;
    }

    return n00b_min(val, s->codepoints);
}

static inline int
n00b_style_count(n00b_string_t *s)
{
    if (!s->styling) {
        return 0;
    }

    return s->styling->num_styles;
}

static inline int
n00b_style_start(n00b_string_t *s, int i)
{
    if (!s->styling || i >= s->styling->num_styles) {
        return s->codepoints;
    }
    int val = s->styling->styles[i].start;

    if (val <= N00B_STYLE_EXTEND) {
        if (!i) {
            return 0;
        }
        return n00b_style_end(s, i - 1);
    }

    return val;
}

// Breaks immutability so should only be done internally.
// Note that this RE-styles the string, it does not add or merge style info.
static inline void
n00b_internal_style_range(n00b_string_t    *s,
                          n00b_box_props_t *props,
                          int               start,
                          int               end)
{
    if (!s->styling) {
        s->styling = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                        n00b_style_record_t,
                                        1,
                                        N00B_GC_SCAN_ALL);

        s->styling->num_styles      = 1;
        s->styling->styles[0].info  = &props->text_style;
        s->styling->styles[0].start = start;
        s->styling->styles[0].end   = end;
    }
    else {
        s->styling->base_style = &props->text_style;
    }
}

static inline void
n00b_string_style_without_copying(n00b_string_t    *s,
                                  n00b_box_props_t *props)
{
    return n00b_internal_style_range(s, props, 0, s->codepoints);
}

#endif
