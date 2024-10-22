#pragma once

#include "n00b.h"

// Flags in the style `info` bitfield.

#define N00B_STY_BOLD         (0x1ULL << 63)
#define N00B_STY_ITALIC       (0x1ULL << 62)
#define N00B_STY_ST           (0x1ULL << 61)
#define N00B_STY_UL           (0x1ULL << 60)
#define N00B_STY_UUL          (0x1ULL << 59)
#define N00B_STY_REV          (0x1ULL << 58)
#define N00B_STY_PROTECT_CASE (0x1ULL << 57)
#define N00B_STY_BG           (0x1ULL << 56)
#define N00B_STY_LOWER        (0x1ULL << 30)
#define N00B_STY_UPPER        (0x1ULL << 29)
#define N00B_STY_FG           (0x1ULL << 24)

#define N00B_STY_TITLE       (N00B_STY_UPPER | N00B_STY_LOWER)
#define N00B_STY_BAD         (0xffffffffffffffffULL)
#define N00B_STY_FG_BITS     (0x0ffffffULL)
#define N00B_BG_SHIFT        32ULL
#define N00B_STY_BG_BITS     (N00B_STY_FG_BITS << N00B_BG_SHIFT)
#define N00B_STY_CLEAR_FG    ~N00B_STY_FG_BITS
#define N00B_STY_CLEAR_BG    ~N00B_STY_BG_BITS
#define N00B_STY_CLEAR_FLAGS (N00B_STY_FG_BITS | N00B_STY_BG_BITS)

#define N00B_OFFSET_BG_RED   48
#define N00B_OFFSET_BG_GREEN 40
#define N00B_OFFSET_BG_BLUE  32
#define N00B_OFFSET_FG_RED   16
#define N00B_OFFSET_FG_GREEN 8
#define N00B_OFFSET_FG_BLUE  0

extern n00b_style_t n00b_apply_bg_color(n00b_style_t style, n00b_utf8_t *name);
extern n00b_style_t n00b_apply_fg_color(n00b_style_t style, n00b_utf8_t *name);
extern void        n00b_style_gaps(n00b_str_t *, n00b_style_t);
extern void        n00b_str_layer_style(n00b_str_t *, n00b_style_t, n00b_style_t);

static inline void
n00b_style_debug(char *prefix, const n00b_str_t *p)
{
    if (!p)
        return;

    if (p->styling == NULL) {
        printf("debug (%s): len: %lld styles: nil\n",
               prefix,
               (long long)n00b_str_codepoint_len(p));
        return;
    }
    else {
        printf("debug (%s): len: %lld # styles: %lld\n",
               prefix,
               (long long)n00b_str_codepoint_len(p),
               (long long)p->styling->num_entries);
    }
    for (int i = 0; i < p->styling->num_entries; i++) {
        n00b_style_entry_t entry = p->styling->styles[i];
        printf("%d: %llx (%lld:%lld)\n",
               i + 1,
               (long long)p->styling->styles[i].info,
               entry.start,
               entry.end);
    }
}

// The remaining 5 flags will currently be used for fonts. Might
// add another word in for other bits, not sure.

static inline size_t
n00b_style_size(uint64_t num_entries)
{
    return sizeof(n00b_style_info_t) + (sizeof(n00b_style_entry_t) * (num_entries + 1));
}

static inline size_t
n00b_alloc_style_len(n00b_str_t *s)
{
    return sizeof(n00b_style_info_t) + (s->styling->num_entries + 1) * sizeof(n00b_style_entry_t);
}

static inline int64_t
n00b_style_num_entries(n00b_str_t *s)
{
    if (s->styling == NULL) {
        return 0;
    }
    return s->styling->num_entries;
}

static inline void
n00b_alloc_styles(n00b_str_t *s, int n)
{
    if (n <= 0) {
        s->styling              = n00b_gc_flex_alloc(n00b_style_info_t,
                                       n00b_style_entry_t,
                                       1,
                                       N00B_GC_SCAN_ALL);
        s->styling->num_entries = 0;
    }
    else {
        s->styling              = n00b_gc_flex_alloc(n00b_style_info_t,
                                       n00b_style_entry_t,
                                       n + 1,
                                       N00B_GC_SCAN_ALL);
        s->styling->num_entries = n;
    }
}

static inline void
n00b_copy_style_info(const n00b_str_t *from_str, n00b_str_t *to_str)
{
    if (from_str->styling == NULL) {
        return;
    }
    int n = from_str->styling->num_entries;

    n00b_alloc_styles(to_str, n);

    for (int i = 0; i < n; i++) {
        n00b_style_entry_t s        = from_str->styling->styles[i];
        to_str->styling->styles[i] = s;
    }

    to_str->styling->num_entries = from_str->styling->num_entries;
}

static inline void
n00b_str_set_style(n00b_str_t *s, n00b_style_t style)
{
    n00b_alloc_styles(s, 1);
    s->styling->styles[0].start = 0;
    s->styling->styles[0].end   = n00b_str_codepoint_len(s);
    s->styling->styles[0].info  = style;
}

static inline int
n00b_copy_and_offset_styles(n00b_str_t *from_str,
                           n00b_str_t *to_str,
                           int        dst_style_ix,
                           int        offset)
{
    if (from_str->styling == NULL || from_str->styling->num_entries == 0) {
        return dst_style_ix;
    }

    for (int i = 0; i < from_str->styling->num_entries; i++) {
        n00b_style_entry_t style = from_str->styling->styles[i];

        if (style.end <= style.start) {
            break;
        }

        style.start += offset;
        style.end += offset;
        to_str->styling->styles[dst_style_ix++] = style;

        style = to_str->styling->styles[dst_style_ix - 1];
    }

    return dst_style_ix;
}

static inline void
n00b_str_apply_style(n00b_str_t *s, n00b_style_t style, bool replace)
{
    if (replace) {
        n00b_str_set_style(s, style);
    }
    else {
        n00b_str_layer_style(s, style, 0);
    }
}

static inline n00b_style_t
n00b_set_bg_color(n00b_style_t style, n00b_color_t color)
{
    style &= N00B_STY_CLEAR_BG;
    style |= N00B_STY_BG;
    style |= (((uint64_t)color) << N00B_BG_SHIFT);

    return style;
}

static inline n00b_style_t
n00b_set_fg_color(n00b_style_t style, n00b_color_t color)
{
    style &= N00B_STY_CLEAR_FG;
    style |= N00B_STY_FG;
    style |= color;

    return style;
}

extern n00b_style_t default_style;

static inline void
n00b_set_default_style(n00b_style_t s)
{
    default_style = s;
}

static inline n00b_style_t
n00b_get_default_style()
{
    return default_style;
}

static inline n00b_style_t
n00b_new_style()
{
    return (uint64_t)0;
}

static inline n00b_style_t
n00b_add_bold(n00b_style_t style)
{
    return style | N00B_STY_BOLD;
}

static inline n00b_style_t
n00b_remove_bold(n00b_style_t style)
{
    return style & ~N00B_STY_BOLD;
}

static inline n00b_style_t
n00b_add_inverse(n00b_style_t style)
{
    return style | N00B_STY_REV;
}

static inline n00b_style_t
n00b_remove_inverse(n00b_style_t style)
{
    return style & ~N00B_STY_REV;
}

static inline n00b_style_t
n00b_add_strikethrough(n00b_style_t style)
{
    return style | N00B_STY_ST;
}

static inline n00b_style_t
n00b_remove_strikethrough(n00b_style_t style)
{
    return style & ~N00B_STY_ST;
}

static inline n00b_style_t
n00b_add_italic(n00b_style_t style)
{
    return style | N00B_STY_ITALIC;
}

static inline n00b_style_t
n00b_remove_italic(n00b_style_t style)
{
    return style & ~N00B_STY_ITALIC;
}

static inline n00b_style_t
n00b_add_underline(n00b_style_t style)
{
    return (style | N00B_STY_UL) & ~N00B_STY_UUL;
}

static inline n00b_style_t
n00b_add_double_underline(n00b_style_t style)
{
    return (style | N00B_STY_UUL) & ~N00B_STY_UL;
}

static inline n00b_style_t
n00b_remove_underline(n00b_style_t style)
{
    return style & ~(N00B_STY_UL | N00B_STY_UUL);
}

static inline n00b_style_t
n00b_add_upper_case(n00b_style_t style)
{
    return (style & ~N00B_STY_TITLE) | N00B_STY_UPPER;
}
static inline n00b_style_t
n00b_add_lower_case(n00b_style_t style)
{
    return (style & ~N00B_STY_TITLE) | N00B_STY_LOWER;
}

static inline n00b_style_t
n00b_add_title_case(n00b_style_t style)
{
    return style | N00B_STY_TITLE;
}

static inline n00b_style_t
n00b_remove_case(n00b_style_t style)
{
    return style & ~N00B_STY_TITLE;
}

static inline n00b_style_t
n00b_remove_bg_color(n00b_style_t style)
{
    return ((uint64_t)style) & (N00B_STY_CLEAR_BG & ~N00B_STY_BG);
}

static inline n00b_style_t
n00b_remove_fg_color(n00b_style_t style)
{
    return ((uint64_t)style) & (N00B_STY_CLEAR_FG & ~N00B_STY_FG);
}

static inline n00b_style_t
n00b_remove_all_color(n00b_style_t style)
{
    // This should mainly constant fold down to a single AND.
    return style & ~(N00B_STY_FG_BITS | N00B_STY_BG_BITS);
}

// After the slice, remove dead styles.
// This isn't being used, but it's a reasonable debugging tool.
static inline void
n00b_clean_styles(n00b_str_t *s)
{
    if (!s->styling) {
        return;
    }
    int l = s->styling->num_entries;

    if (l < 2) {
        return;
    }

    int move = 0;

    n00b_style_entry_t prev = s->styling->styles[0];
    n00b_style_entry_t cur;

    for (int i = 1; i < s->styling->num_entries; i++) {
        cur = s->styling->styles[i];
        if (cur.end <= prev.end) {
            move++;
            continue;
        }
        if (prev.end > cur.start) {
            cur.start = prev.end;
        }
        if (cur.start >= cur.end) {
            move++;
            continue;
        }

        s->styling->styles[i - move] = cur;

        prev = cur;
    }
    s->styling->num_entries -= move;
}
