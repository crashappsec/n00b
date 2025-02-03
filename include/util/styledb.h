#pragma once

#include "n00b.h"

extern void                 n00b_set_style(n00b_utf8_t *,
                                           n00b_render_style_t *);
extern n00b_render_style_t *n00b_lookup_cell_style(n00b_utf8_t *);
extern void                 n00b_install_default_styles();

static inline n00b_render_style_t *
n00b_copy_render_style(const n00b_render_style_t *style)
{
    n00b_render_style_t *result = n00b_new(n00b_type_render_style());

    if (!style) {
        return result;
    }

    memcpy(result, style, sizeof(n00b_render_style_t));

    return result;
}
static inline n00b_style_t
n00b_str_style(n00b_render_style_t *style)
{
    if (!style) {
        return 0;
    }
    return style->base_style;
}

static inline n00b_style_t
n00b_lookup_text_style(n00b_utf8_t *name)
{
    return n00b_str_style(n00b_lookup_cell_style(name));
}

static inline void
n00b_style_set_fg_color(n00b_render_style_t *style, n00b_color_t color)
{
    style->base_style &= N00B_STY_CLEAR_FG;
    style->base_style |= N00B_STY_FG;
    style->base_style |= color;
}

static inline void
n00b_style_set_bg_color(n00b_render_style_t *style, n00b_color_t color)
{
    style->base_style &= N00B_STY_CLEAR_BG;
    style->base_style |= N00B_STY_BG;
    style->base_style |= (((int64_t)color) << (int64_t)N00B_BG_SHIFT);
}

static inline void
n00b_style_bold_on(n00b_render_style_t *style)
{
    style->base_style |= N00B_STY_BOLD;
}

static inline void
n00b_style_bold_off(n00b_render_style_t *style)
{
    style->base_style &= ~N00B_STY_BOLD;
}

static inline bool
n00b_style_get_bold(n00b_render_style_t *style)
{
    return (bool)(style->base_style & N00B_STY_BOLD);
}

static inline void
n00b_style_italic_on(n00b_render_style_t *style)
{
    style->base_style |= N00B_STY_ITALIC;
}

static inline void
n00b_style_italic_off(n00b_render_style_t *style)
{
    style->base_style &= ~N00B_STY_ITALIC;
}

static inline bool
n00b_style_get_italic(n00b_render_style_t *style)
{
    return (bool)(style->base_style & N00B_STY_ITALIC);
}

static inline void
n00b_style_strikethru_on(n00b_render_style_t *style)
{
    style->base_style |= N00B_STY_ST;
}

static inline void
n00b_style_strikethru_off(n00b_render_style_t *style)
{
    style->base_style &= ~N00B_STY_ST;
}

static inline bool
n00b_style_get_strikethru(n00b_render_style_t *style)
{
    return (bool)(style->base_style & N00B_STY_ST);
}

static inline void
n00b_style_underline_off(n00b_render_style_t *style)
{
    style->base_style &= ~(N00B_STY_UL | N00B_STY_UUL);
}

static inline void
n00b_style_underline_on(n00b_render_style_t *style)
{
    n00b_style_underline_off(style);
    style->base_style |= N00B_STY_UL;
}

static inline bool
n00b_style_get_underline(n00b_render_style_t *style)
{
    return (bool)(style->base_style & N00B_STY_UL);
}

static inline void
n00b_style_double_underline_on(n00b_render_style_t *style)
{
    n00b_style_underline_off(style);
    style->base_style |= N00B_STY_UUL;
}

static inline bool
n00b_style_get_double_underline(n00b_render_style_t *style)
{
    return (bool)(style->base_style & N00B_STY_UUL);
}

static inline void
n00b_style_reverse_on(n00b_render_style_t *style)
{
    style->base_style |= N00B_STY_REV;
}

static inline void
n00b_style_reverse_off(n00b_render_style_t *style)
{
    style->base_style &= ~N00B_STY_REV;
}

static inline bool
n00b_style_get_reverse(n00b_render_style_t *style)
{
    return (bool)(style->base_style & ~N00B_STY_REV);
}

static inline void
n00b_style_casing_off(n00b_render_style_t *style)
{
    style->base_style &= ~N00B_STY_TITLE;
}

static inline void
n00b_style_lowercase_on(n00b_render_style_t *style)
{
    n00b_style_casing_off(style);
    style->base_style |= N00B_STY_LOWER;
}

static inline void
n00b_style_uppercase_on(n00b_render_style_t *style)
{
    n00b_style_casing_off(style);
    style->base_style |= N00B_STY_UPPER;
}

static inline void
n00b_style_titlecase_on(n00b_render_style_t *style)
{
    n00b_style_casing_off(style);
    style->base_style |= N00B_STY_TITLE;
}

static inline bool
n00b_style_get_upper(n00b_render_style_t *style)
{
    return (style->base_style & N00B_STY_TITLE) == N00B_STY_UPPER;
}

static inline bool
n00b_style_get_lower(n00b_render_style_t *style)
{
    return (style->base_style & N00B_STY_TITLE) == N00B_STY_LOWER;
}

static inline bool
n00b_style_get_title(n00b_render_style_t *style)
{
    return (style->base_style & N00B_STY_TITLE) == N00B_STY_TITLE;
}

static inline n00b_dimspec_kind_t
n00b_style_col_kind(n00b_render_style_t *style)
{
    return style->dim_kind;
}

extern const n00b_border_theme_t *n00b_registered_borders;

static inline bool
n00b_style_set_border_theme(n00b_render_style_t *style, char *name)
{
    n00b_border_theme_t *cur = (n00b_border_theme_t *)n00b_registered_borders;

    while (cur != NULL) {
        if (!strcmp((char *)name, cur->name)) {
            style->border_theme = cur;
            return true;
        }
        cur = cur->next_style;
    }
    return false;
}

static inline void
n00b_style_set_flex_size(n00b_render_style_t *style, int64_t size)
{
    n00b_assert(size >= 0);
    style->dim_kind   = N00B_DIM_FLEX_UNITS;
    style->dims.units = (uint64_t)size;
}

static inline void
n00b_style_set_absolute_size(n00b_render_style_t *style, int64_t size)
{
    n00b_assert(size >= 0);
    style->dim_kind   = N00B_DIM_ABSOLUTE;
    style->dims.units = (uint64_t)size;
}

static inline void
n00b_style_set_size_range(n00b_render_style_t *style, int32_t min, int32_t max)
{
    n00b_assert(min >= 0 && max >= min);
    style->dim_kind      = N00B_DIM_ABSOLUTE_RANGE;
    style->dims.range[0] = min;
    style->dims.range[1] = max;
}

static inline void
n00b_style_set_fit_to_text(n00b_render_style_t *style)
{
    style->dim_kind = N00B_DIM_FIT_TO_TEXT;
}

static inline void
n00b_style_set_auto_size(n00b_render_style_t *style)
{
    style->dim_kind = N00B_DIM_AUTO;
}

static inline void
n00b_style_set_size_as_percent(n00b_render_style_t *style, double pct, bool round)
{
    n00b_assert(pct >= 0);
    if (round) {
        style->dim_kind = N00B_DIM_PERCENT_ROUND;
    }
    else {
        style->dim_kind = N00B_DIM_PERCENT_TRUNCATE;
    }
    style->dims.percent = pct;
}

static inline void
n00b_style_set_top_pad(n00b_render_style_t *style, int8_t pad)
{
    n00b_assert(pad >= 0);
    style->top_pad  = pad;
    style->tpad_set = 1;
}

static inline void
n00b_style_set_bottom_pad(n00b_render_style_t *style, int8_t pad)
{
    n00b_assert(pad >= 0);
    style->bottom_pad = pad;
    style->bpad_set   = 1;
}

static inline void
n00b_style_set_left_pad(n00b_render_style_t *style, int8_t pad)
{
    n00b_assert(pad >= 0);
    style->left_pad = pad;
    style->lpad_set = 1;
}

static inline void
n00b_style_set_right_pad(n00b_render_style_t *style, int8_t pad)
{
    n00b_assert(pad >= 0);
    style->right_pad = pad;
    style->rpad_set  = 1;
}

static inline void
n00b_style_set_wrap_hang(n00b_render_style_t *style, int8_t hang)
{
    n00b_assert(hang >= 0);
    style->wrap         = hang;
    style->disable_wrap = 0;
    style->hang_set     = 1;
}

static inline void
n00b_style_disable_line_wrap(n00b_render_style_t *style)
{
    style->disable_wrap = 1;
    style->hang_set     = 1;
}

static inline void
n00b_set_pad_color(n00b_render_style_t *style, n00b_color_t color)
{
    style->pad_color     = color;
    style->pad_color_set = 1;
}

static inline void
n00b_style_clear_fg_color(n00b_render_style_t *style)
{
    style->base_style &= ~(N00B_STY_FG | N00B_STY_FG_BITS);
}

static inline void
n00b_style_clear_bg_color(n00b_render_style_t *style)
{
    style->base_style &= ~(N00B_STY_BG | N00B_STY_BG_BITS);
}

static inline void
n00b_style_set_alignment(n00b_render_style_t *style, n00b_alignment_t alignment)
{
    style->alignment = alignment;
}

static inline n00b_alignment_t
n00b_style_get_alignment(n00b_render_style_t *style)
{
    return style->alignment;
}

static inline void
n00b_style_set_borders(n00b_render_style_t *style, n00b_border_set_t borders)
{
    style->borders = borders;
}

static inline bool
n00b_style_is_bg_color_on(n00b_render_style_t *style)
{
    return style->base_style & N00B_STY_BG;
}

static inline bool
n00b_style_is_fg_color_on(n00b_render_style_t *style)
{
    return style->base_style & N00B_STY_FG;
}

static inline bool
n00b_style_is_pad_color_on(n00b_render_style_t *style)
{
    return style->pad_color_set;
}

static inline n00b_color_t
n00b_style_get_fg_color(n00b_render_style_t *style)
{
    return (n00b_color_t)style->base_style & ~N00B_STY_CLEAR_FG;
}

static inline n00b_color_t
n00b_style_get_bg_color(n00b_render_style_t *style)
{
    uint64_t extract = style->base_style & ~N00B_STY_CLEAR_BG;
    return (n00b_color_t)(extract >> N00B_BG_SHIFT);
}

static inline n00b_style_t
n00b_style_get_pad_color(n00b_render_style_t *style)
{
    if (style->pad_color_set) {
        return style->pad_color;
    }
    return n00b_style_get_bg_color(style);
}

static inline n00b_border_theme_t *
n00b_style_get_border_theme(n00b_render_style_t *style)
{
    n00b_border_theme_t *result = style->border_theme;

    if (!result) {
        result = (n00b_border_theme_t *)n00b_registered_borders;
    }
    return result;
}

static inline int64_t
n00b_style_get_top_pad(n00b_render_style_t *style)
{
    return (int64_t)style->top_pad;
}

static inline int64_t
n00b_style_get_bottom_pad(n00b_render_style_t *style)
{
    return (int64_t)style->bottom_pad;
}

static inline int64_t
n00b_style_get_left_pad(n00b_render_style_t *style)
{
    return (int64_t)style->left_pad;
}

static inline int64_t
n00b_style_get_right_pad(n00b_render_style_t *style)
{
    return (int64_t)style->right_pad;
}

static inline int64_t
n00b_style_get_wrap(n00b_render_style_t *style)
{
    if (style->disable_wrap) {
        return -1;
    }

    return (int64_t)style->wrap;
}

static inline bool
n00b_style_hang_set(n00b_render_style_t *style)
{
    return style->hang_set;
}

static inline bool
n00b_style_tpad_set(n00b_render_style_t *style)
{
    return style->tpad_set;
}

static inline bool
n00b_style_bpad_set(n00b_render_style_t *style)
{
    return style->bpad_set;
}

static inline bool
n00b_style_lpad_set(n00b_render_style_t *style)
{
    return style->lpad_set;
}

static inline bool
n00b_style_rpad_set(n00b_render_style_t *style)
{
    return style->rpad_set;
}

static inline n00b_render_style_t *
n00b_new_render_style()
{
    return n00b_new(n00b_type_render_style());
}

extern bool                 n00b_style_exists(n00b_utf8_t *name);
extern n00b_render_style_t *n00b_layer_styles(n00b_render_style_t *,
                                              n00b_render_style_t *);
extern void
n00b_override_style(n00b_render_style_t *,
                    n00b_render_style_t *);

extern n00b_grid_t *n00b_style_details(n00b_render_style_t *);
