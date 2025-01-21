#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_dict_t *style_dictionary = NULL;

static const n00b_border_theme_t border_ascii = {
    .name            = "ascii",
    .horizontal_rule = '-',
    .vertical_rule   = '|',
    .upper_left      = '/',
    .upper_right     = '\\',
    .lower_left      = '\\',
    .lower_right     = '/',
    .cross           = '+',
    .top_t           = '-',
    .bottom_t        = '-',
    .left_t          = '|',
    .right_t         = '|',
    .next_style      = NULL,
};

static const n00b_border_theme_t border_asterisk = {
    .name            = "asterisk",
    .horizontal_rule = '*',
    .vertical_rule   = '*',
    .upper_left      = '*',
    .upper_right     = '*',
    .lower_left      = '*',
    .lower_right     = '*',
    .cross           = '*',
    .top_t           = '*',
    .bottom_t        = '*',
    .left_t          = '*',
    .right_t         = '*',
    .next_style      = (n00b_border_theme_t *)&border_ascii,
};

static const n00b_border_theme_t border_bold_dash2 = {
    .name            = "bold_dash2",
    .horizontal_rule = 0x2509,
    .vertical_rule   = 0x250b,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (n00b_border_theme_t *)&border_asterisk,
};

static const n00b_border_theme_t border_dash2 = {
    .name            = "dash2",
    .horizontal_rule = 0x2508,
    .vertical_rule   = 0x250a,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (n00b_border_theme_t *)&border_bold_dash2,
};

static const n00b_border_theme_t border_bold_dash = {
    .name            = "bold_dash",
    .horizontal_rule = 0x2505,
    .vertical_rule   = 0x2507,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (n00b_border_theme_t *)&border_dash2,
};

static const n00b_border_theme_t border_dash = {
    .name            = "dash",
    .horizontal_rule = 0x2504,
    .vertical_rule   = 0x2506,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (n00b_border_theme_t *)&border_bold_dash,
};

static const n00b_border_theme_t border_double = {
    .name            = "double",
    .horizontal_rule = 0x2550,
    .vertical_rule   = 0x2551,
    .upper_left      = 0x2554,
    .upper_right     = 0x2557,
    .lower_left      = 0x255a,
    .lower_right     = 0x255d,
    .cross           = 0x256c,
    .top_t           = 0x2566,
    .bottom_t        = 0x2569,
    .left_t          = 0x2560,
    .right_t         = 0x2563,
    .next_style      = (n00b_border_theme_t *)&border_dash,
};

static const n00b_border_theme_t border_bold = {
    .name            = "bold",
    .horizontal_rule = 0x2501,
    .vertical_rule   = 0x2503,
    .upper_left      = 0x250f,
    .upper_right     = 0x2513,
    .lower_left      = 0x2517,
    .lower_right     = 0x251b,
    .cross           = 0x254b,
    .top_t           = 0x2533,
    .bottom_t        = 0x253b,
    .left_t          = 0x2523,
    .right_t         = 0x252b,
    .next_style      = (n00b_border_theme_t *)&border_double,
};

static const n00b_border_theme_t border_plain = {
    .name            = "plain",
    .horizontal_rule = 0x2500,
    .vertical_rule   = 0x2502,
    .upper_left      = 0x250c,
    .upper_right     = 0x2510,
    .lower_left      = 0x2514,
    .lower_right     = 0x2518,
    .cross           = 0x253c,
    .top_t           = 0x252c,
    .bottom_t        = 0x2534,
    .left_t          = 0x251c,
    .right_t         = 0x2524,
    .next_style      = (n00b_border_theme_t *)&border_bold,
};

const n00b_border_theme_t *n00b_registered_borders = (n00b_border_theme_t *)&border_plain;

// Used for border drawing and background (pad color).
static const n00b_render_style_t default_table = {
    .borders        = N00B_BORDER_TOP | N00B_BORDER_BOTTOM | N00B_BORDER_LEFT | N00B_BORDER_RIGHT | N00B_INTERIOR_HORIZONTAL | N00B_INTERIOR_VERTICAL,
    .border_theme   = (n00b_border_theme_t *)&border_bold_dash,
    .dim_kind       = N00B_DIM_AUTO,
    .alignment      = N00B_ALIGN_MID_LEFT,
    .base_style     = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .pad_color      = 0x2f3f3f,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

static const n00b_render_style_t col_borders_table = {
    .borders        = N00B_BORDER_TOP | N00B_BORDER_BOTTOM | N00B_BORDER_LEFT | N00B_BORDER_RIGHT | N00B_INTERIOR_VERTICAL,
    .dim_kind       = N00B_DIM_AUTO,
    .alignment      = N00B_ALIGN_MID_LEFT,
    .base_style     = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .pad_color      = 0x2f3f3f,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

static const n00b_render_style_t default_tr = {
    .alignment = N00B_ALIGN_TOP_LEFT,
};

static n00b_render_style_t default_tr_even = {
    .alignment  = N00B_ALIGN_TOP_LEFT,
    .base_style = 0x1f2f2f00f2f3f4ull | N00B_STY_BG | N00B_STY_FG,
    .pad_color  = 0xa9a9a9,
    .weight_bg  = 15,
    .weight_fg  = 10,
};

static n00b_render_style_t default_tr_odd = {
    .alignment  = N00B_ALIGN_TOP_LEFT,
    .base_style = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .pad_color  = 0x2f3f3f,
    .weight_bg  = 15,
    .weight_fg  = 10,
};

static const n00b_render_style_t default_th = {
    .base_style   = N00B_STY_UPPER | 0xb3ff00 | N00B_STY_BG | N00B_STY_FG | N00B_STY_BOLD,
    .alignment    = N00B_ALIGN_MID_CENTER,
    .weight_bg    = 20,
    .weight_fg    = 20,
    .weight_flags = 10,
};

static const n00b_render_style_t default_td = {
    .left_pad  = 1,
    .right_pad = 1,
};

static const n00b_render_style_t default_tcol = {
    .dim_kind     = N00B_DIM_AUTO,
    .weight_width = 10,
    .left_pad     = 1,
    .right_pad    = 1,
};

static const n00b_render_style_t default_snap_col = {
    .dim_kind     = N00B_DIM_FIT_TO_TEXT,
    .left_pad     = 1,
    .right_pad    = 1,
    .weight_width = 10,
};

static const n00b_render_style_t default_full_snap_col = {
    .dim_kind     = N00B_DIM_FIT_TO_TEXT,
    .left_pad     = 1,
    .right_pad    = 1,
    .weight_width = 10,
};

static const n00b_render_style_t default_flex_col = {
    .dim_kind     = N00B_DIM_FLEX_UNITS,
    .left_pad     = 1,
    .right_pad    = 1,
    .dims.units   = 1,
    .weight_width = 10,
};

static const n00b_render_style_t default_list_grid = {
    .base_style   = 0x2f3f3ff8f8fful | N00B_STY_BG | N00B_STY_FG,
    .bottom_pad   = 1,
    .dim_kind     = N00B_DIM_AUTO,
    .alignment    = N00B_ALIGN_MID_LEFT,
    .weight_bg    = 10,
    .weight_fg    = 20,
    .weight_width = 100,
};

static const n00b_render_style_t default_ordered_list_grid = {
    .base_style   = 0x2f3f3ff8f8fful | N00B_STY_BG | N00B_STY_FG,
    .bottom_pad   = 1,
    .dim_kind     = N00B_DIM_AUTO,
    .alignment    = N00B_ALIGN_MID_LEFT,
    .weight_bg    = 10,
    .weight_fg    = 20,
    .weight_width = 1,
};

static const n00b_render_style_t default_bullet_column = {
    .dim_kind     = N00B_DIM_ABSOLUTE,
    .base_style   = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG | N00B_STY_BOLD,
    .left_pad     = 1,
    .dims.units   = 1,
    .alignment    = N00B_ALIGN_TOP_RIGHT,
    .weight_bg    = 1,
    .weight_fg    = 20,
    .weight_width = 10,
};

static const n00b_render_style_t default_list_text_column = {
    .base_style   = 0x2f3f3f00f8f8fful | N00B_STY_BG | N00B_STY_FG,
    .dim_kind     = N00B_DIM_AUTO,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = N00B_ALIGN_TOP_LEFT,
    .weight_bg    = 1,
    .weight_fg    = 20,
    .weight_width = 10,
};

static const n00b_render_style_t default_tree_item = {
    .base_style   = 0x2f3f3f00f8f8fful | N00B_STY_BG | N00B_STY_FG,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = N00B_ALIGN_TOP_LEFT,
    .disable_wrap = true,
    .pad_color    = 0x2f3f3f,
    .weight_bg    = 10,
    .weight_fg    = 7,
};

static const n00b_render_style_t default_h1 = {
    .base_style = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_BOLD | 0x343434000ff2f8eULL,
    .top_pad    = 2,
    .alignment  = N00B_ALIGN_BOTTOM_CENTER,
    .weight_fg  = 10,
    .weight_bg  = 5,
};

static const n00b_render_style_t default_h2 = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_BOLD | 0x60606000b3ff00ULL,
    .top_pad      = 1,
    .alignment    = N00B_ALIGN_BOTTOM_CENTER,
    .weight_fg    = 10,
    .weight_bg    = 3,
    .weight_align = 10,

};

static const n00b_render_style_t default_h3 = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_BOLD | 0x45454500ee82eeULL,
    .top_pad      = 1,
    .alignment    = N00B_ALIGN_BOTTOM_CENTER,
    .weight_fg    = 10,
    .weight_bg    = 3,
    .weight_align = 10,

};

static const n00b_render_style_t default_h4 = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_UL | 0xff2f8e00343434ULL,
    .alignment    = N00B_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const n00b_render_style_t default_h5 = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_UL | 0xb3ff0000606060ULL,
    .alignment    = N00B_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const n00b_render_style_t default_h6 = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_UL | 0x00ee82ee00454545ULL,
    .alignment    = N00B_ALIGN_BOTTOM_LEFT,
    .weight_fg    = 10,
    .weight_bg    = 10,
    .weight_align = 10,
};

static const n00b_render_style_t default_flow = {
    .base_style   = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .left_pad     = 1,
    .right_pad    = 1,
    .alignment    = N00B_ALIGN_TOP_LEFT,
    .pad_color    = 0x2f3f3f,
    .weight_bg    = 5,
    .weight_fg    = 5,
    .weight_align = 5,
    .weight_width = 5,
};

static const n00b_render_style_t default_code = {
    .base_style = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .left_pad   = 1,
    .right_pad  = 1,
    .alignment  = N00B_ALIGN_TOP_LEFT,
    .pad_color  = 0x2f3f3f,
};

static const n00b_render_style_t default_error_grid = {
    .base_style     = 0x2f3f3f00f8f8ffull | N00B_STY_BG | N00B_STY_FG,
    .alignment      = N00B_ALIGN_TOP_LEFT,
    .weight_bg      = 5,
    .weight_fg      = 5,
    .weight_align   = 5,
    .weight_width   = 5,
    .weight_borders = 10,
};

static const n00b_render_style_t default_em = {
    .base_style   = N00B_STY_ITALIC | N00B_STY_FG | 0xff2f8eULL,
    .weight_flags = 10,
    .weight_fg    = 20,
};

static const n00b_render_style_t default_link = {
    .base_style   = N00B_STY_BOLD | N00B_STY_FG | 0x0000ee,
    .weight_flags = 10,
    .weight_fg    = 20,
};

static const n00b_render_style_t default_callout_cell = {
    .top_pad      = 1,
    .bottom_pad   = 1,
    .left_pad     = 0,
    .right_pad    = 0,
    .alignment    = N00B_ALIGN_BOTTOM_CENTER,
    .dim_kind     = N00B_DIM_PERCENT_TRUNCATE,
    .dims.percent = 90,
    .weight_width = 20,
};

static const n00b_render_style_t default_callout = {
    .base_style     = N00B_STY_ITALIC | N00B_STY_FG | N00B_STY_BG | N00B_STY_BOLD | 0xff2f8e00b3ff00UL,
    .top_pad        = 2,
    .bottom_pad     = 2,
    .alignment      = N00B_ALIGN_BOTTOM_CENTER,
    .dim_kind       = N00B_DIM_FIT_TO_TEXT,
    .weight_borders = 10,
    .weight_fg      = 4,
    .weight_bg      = 4,
    .weight_align   = 5,
    .weight_width   = 10,
};

void
n00b_rs_gc_bits(uint64_t *bitfield, n00b_render_style_t *style)
{
    n00b_set_bit(bitfield, 0);
    n00b_set_bit(bitfield, 1);
}

static inline void
init_style_db()
{
    if (style_dictionary == NULL) {
        style_dictionary = n00b_dict(n00b_type_utf8(), n00b_type_ref());
        n00b_gc_register_root(&style_dictionary, 1);
    }
}

void
n00b_set_style(n00b_utf8_t *name, n00b_render_style_t *style)
{
    n00b_push_heap(n00b_thread_master_heap);
    init_style_db();
    hatrack_dict_put(style_dictionary, name, style);
    n00b_pop_heap();
}

// Returns a COPY of the style so that it doesn't get accidentially
// changed by reference.
n00b_render_style_t *
n00b_lookup_cell_style(n00b_utf8_t *name)
{
    n00b_push_heap(n00b_thread_master_heap);
    init_style_db();

    n00b_render_style_t *entry = hatrack_dict_get(style_dictionary,
                                                  name,
                                                  NULL);

    if (!entry) {
        n00b_pop_heap();
        return NULL;
    }

    n00b_render_style_t *result = n00b_new(n00b_type_render_style());

    memcpy(result, entry, sizeof(n00b_render_style_t));
    n00b_pop_heap();
    return result;
}

void
n00b_style_init(n00b_render_style_t *style, va_list args)
{
    n00b_color_t fg_color        = -1;
    n00b_color_t bg_color        = -1;
    bool         bold            = false;
    bool         italic          = false;
    bool         strikethru      = false;
    bool         underline       = false;
    bool         duline          = false;
    bool         reverse         = false;
    bool         fit_text        = false;
    bool         disable_wrap    = false;
    double       width_pct       = -1;
    int64_t      flex_units      = -1;
    int32_t      min_size        = -1;
    int32_t      max_size        = -1;
    int32_t      top_pad         = -1;
    int32_t      bottom_pad      = -1;
    int32_t      left_pad        = -1;
    int32_t      right_pad       = -1;
    int32_t      wrap_hang       = -1;
    n00b_color_t pad_color       = 0xffffffff;
    int32_t      alignment       = -1;
    int32_t      enabled_borders = -1;
    char        *border_theme    = NULL;
    char        *tag             = NULL;

    n00b_karg_va_init(args);
    n00b_kw_int32("fg_color", fg_color);
    n00b_kw_int32("bg_color", bg_color);
    n00b_kw_bool("bold", bold);
    n00b_kw_bool("italic", italic);
    n00b_kw_bool("strike", strikethru);
    n00b_kw_bool("underline", underline);
    n00b_kw_bool("double_underline", duline);
    n00b_kw_bool("reverse", reverse);
    n00b_kw_bool("fit_text", fit_text);
    n00b_kw_bool("disable_wrap", disable_wrap);
    n00b_kw_float("width_pct", width_pct);
    n00b_kw_int64("flex_units", flex_units);
    n00b_kw_int32("min_size", min_size);
    n00b_kw_int32("max_size", max_size);
    n00b_kw_int32("top_pad", top_pad);
    n00b_kw_int32("bottom_pad", bottom_pad);
    n00b_kw_int32("left_pad", left_pad);
    n00b_kw_int32("right_pad", right_pad);
    n00b_kw_int32("wrap_hand", wrap_hang);
    n00b_kw_int32("pad_color", pad_color);
    n00b_kw_int32("alignment", alignment);
    n00b_kw_int32("enabled_borders", enabled_borders);
    n00b_kw_ptr("border_theme", border_theme);
    n00b_kw_ptr("tag", tag);

    style->name     = n00b_new_utf8(tag);
    // Use basic math to make sure overlaping cell sizing strategies
    // aren't requested in one call.
    int32_t sz_test = width_pct + flex_units + min_size + max_size + fit_text;

    if (sz_test != -5) {
        if ((width_pct != -1 && (flex_units + min_size + max_size + fit_text) != -3) || (flex_units != -1 && (min_size + max_size + (int)fit_text) != -2) || (fit_text && (min_size + max_size) != -2)) {
            N00B_CRAISE("Can't specify two cell sizing strategies.");
        }
    }

    if (wrap_hang != -1 && disable_wrap) {
        N00B_CRAISE("Cannot set 'wrap_hang' and 'disable_wrap' at once.\n");
    }

    if (fg_color != -1) {
        n00b_style_set_fg_color(style, fg_color);
    }
    if (bg_color != -1) {
        n00b_style_set_bg_color(style, bg_color);
    }
    if (bold) {
        n00b_style_bold_on(style);
    }
    if (italic) {
        n00b_style_italic_on(style);
    }
    if (strikethru) {
        n00b_style_strikethru_on(style);
    }
    if (duline) {
        n00b_style_double_underline_on(style);
    }
    else {
        if (underline) {
            n00b_style_underline_on(style);
        }
    }
    if (reverse) {
        n00b_style_reverse_on(style);
    }

    if (border_theme != NULL) {
        n00b_style_set_border_theme(style, border_theme);
    }

    if (width_pct != -1) {
        n00b_style_set_size_as_percent(style, width_pct, true);
    }

    if (flex_units != -1) {
        n00b_style_set_flex_size(style, flex_units);
    }

    if (min_size >= 0 || max_size >= 0) {
        if (min_size < 0) {
            min_size = 0;
        }
        if (max_size < 0) {
            max_size = 0x7fffffff;
        }
        n00b_style_set_size_range(style, min_size, max_size);
    }

    if (fit_text) {
        n00b_style_set_fit_to_text(style);
    }

    if (top_pad != -1) {
        n00b_style_set_top_pad(style, top_pad);
    }

    if (bottom_pad != -1) {
        n00b_style_set_bottom_pad(style, bottom_pad);
    }

    if (left_pad != -1) {
        n00b_style_set_left_pad(style, left_pad);
    }

    if (right_pad != -1) {
        n00b_style_set_right_pad(style, right_pad);
    }

    if (wrap_hang != -1) {
        n00b_style_set_wrap_hang(style, wrap_hang);
    }

    if (disable_wrap) {
        n00b_style_disable_line_wrap(style);
    }

    if (pad_color != -1) {
        n00b_set_pad_color(style, pad_color);
    }

    if (alignment != -1) {
        n00b_style_set_alignment(style, (n00b_alignment_t)alignment);
    }

    if (enabled_borders != -1) {
        n00b_style_set_borders(style,
                               (n00b_border_set_t)enabled_borders);
    }

    if (tag != NULL) {
        n00b_set_style(n00b_new_utf8(tag), style);
    }
}

n00b_render_style_t *
n00b_layer_styles(n00b_render_style_t *s1,
                  n00b_render_style_t *s2)
{
    if (!s1 || !s2) {
        return NULL;
    }
    n00b_render_style_t *res = n00b_new(n00b_type_render_style());

    if (s1 == NULL || s1 == s2) {
        if (s2 == NULL) {
            return NULL;
        }
        memcpy(res, s2, sizeof(n00b_render_style_t));
        return res;
    }
    if (s2 == NULL) {
        memcpy(res, s1, sizeof(n00b_render_style_t));
        return res;
    }

    if (s1->weight_fg >= s2->weight_fg) {
        n00b_style_set_fg_color(res, n00b_style_get_fg_color(s1));
        res->weight_fg = s1->weight_fg;
    }
    else {
        n00b_style_set_fg_color(res, n00b_style_get_fg_color(s2));
        res->weight_fg = s2->weight_fg;
    }

    if (s1->weight_bg >= s2->weight_bg) {
        n00b_style_set_bg_color(res, n00b_style_get_bg_color(s1));
        res->pad_color     = s1->pad_color;
        res->pad_color_set = s1->pad_color_set;
        res->weight_bg     = s1->weight_bg;
    }
    else {
        n00b_style_set_bg_color(res, n00b_style_get_bg_color(s2));
        res->pad_color     = s2->pad_color;
        res->pad_color_set = s2->pad_color_set;
        res->weight_bg     = s2->weight_bg;
    }

    static uint64_t mask = ~(N00B_STY_CLEAR_FLAGS | N00B_STY_FG | N00B_STY_BG);
    if (s1->weight_flags >= s2->weight_flags) {
        res->base_style |= s1->base_style & mask;
        res->weight_flags = s1->weight_flags;
    }
    else {
        res->base_style |= s2->base_style & mask;
        res->weight_flags = s2->weight_flags;
    }

    if (s1->weight_align >= s2->weight_align) {
        res->alignment    = s1->alignment;
        res->weight_align = s1->weight_align;
    }
    else {
        res->alignment    = s2->alignment;
        res->weight_align = s2->weight_align;
    }

    if (s1->weight_width >= s2->weight_width) {
        res->dim_kind     = s1->dim_kind;
        res->dims         = s1->dims;
        res->disable_wrap = s1->disable_wrap;
        res->top_pad      = s1->top_pad;
        res->bottom_pad   = s1->bottom_pad;
        res->left_pad     = s1->left_pad;
        res->right_pad    = s1->right_pad;
        res->wrap         = s1->wrap;
        res->hang_set     = s1->hang_set;
        res->tpad_set     = s1->tpad_set;
        res->bpad_set     = s1->bpad_set;
        res->lpad_set     = s1->lpad_set;
        res->rpad_set     = s1->rpad_set;
        res->weight_width = s1->weight_width;
    }
    else {
        res->dim_kind     = s2->dim_kind;
        res->dims         = s2->dims;
        res->disable_wrap = s2->disable_wrap;
        res->top_pad      = s2->top_pad;
        res->bottom_pad   = s2->bottom_pad;
        res->left_pad     = s2->left_pad;
        res->right_pad    = s2->right_pad;
        res->wrap         = s2->wrap;
        res->hang_set     = s2->hang_set;
        res->tpad_set     = s2->tpad_set;
        res->bpad_set     = s2->bpad_set;
        res->lpad_set     = s2->lpad_set;
        res->rpad_set     = s2->rpad_set;
        res->weight_width = s2->weight_width;
    }

    if (s1->weight_borders >= s2->weight_borders) {
        res->borders        = s1->borders;
        res->border_theme   = s1->border_theme;
        res->weight_borders = s1->weight_borders;
    }
    else {
        res->borders        = s2->borders;
        res->border_theme   = s2->border_theme;
        res->weight_borders = s2->weight_borders;
    }

    return res;
}

bool
n00b_style_exists(n00b_utf8_t *name)
{
    if (name == NULL) {
        return 0;
    }

    init_style_db();
    return hatrack_dict_get(style_dictionary, name, NULL) != NULL;
}

static void
static_style(char *name, n00b_render_style_t *s)
{
    n00b_push_heap(n00b_thread_master_heap);
    n00b_render_style_t *copy = n00b_new(n00b_type_render_style());
    memcpy(copy, s, sizeof(n00b_render_style_t));
    copy->name = n00b_new_utf8(name);

    n00b_set_style(copy->name, copy);
    n00b_pop_heap();
}

void
n00b_install_default_styles()
{
    static_style("table", (n00b_render_style_t *)&default_table);
    static_style("table2", (n00b_render_style_t *)&col_borders_table);
    static_style("tr", (n00b_render_style_t *)&default_tr);
    static_style("tr.even", (n00b_render_style_t *)&default_tr_even);
    static_style("tr.odd", (n00b_render_style_t *)&default_tr_odd);
    static_style("td", (n00b_render_style_t *)&default_td);
    static_style("text", (n00b_render_style_t *)&default_td);
    static_style("th", (n00b_render_style_t *)&default_th);
    static_style("tcol", (n00b_render_style_t *)&default_tcol);
    static_style("snap", (n00b_render_style_t *)&default_snap_col);
    static_style("full_snap", (n00b_render_style_t *)&default_full_snap_col);
    static_style("flex", (n00b_render_style_t *)&default_flex_col);
    static_style("ul", (n00b_render_style_t *)&default_list_grid);
    static_style("ol", (n00b_render_style_t *)&default_ordered_list_grid);
    static_style("bullet", (n00b_render_style_t *)&default_bullet_column);
    static_style("li", (n00b_render_style_t *)&default_list_text_column);
    static_style("tree_item", (n00b_render_style_t *)&default_tree_item);
    static_style("h1", (n00b_render_style_t *)&default_h1);
    static_style("h2", (n00b_render_style_t *)&default_h2);
    static_style("h3", (n00b_render_style_t *)&default_h3);
    static_style("h4", (n00b_render_style_t *)&default_h4);
    static_style("h5", (n00b_render_style_t *)&default_h5);
    static_style("h6", (n00b_render_style_t *)&default_h6);
    static_style("table", (n00b_render_style_t *)&default_table);
    static_style("flow", (n00b_render_style_t *)&default_flow);
    static_style("code", (n00b_render_style_t *)&default_code);
    static_style("error_grid", (n00b_render_style_t *)&default_error_grid);
    static_style("em", (n00b_render_style_t *)&default_em);
    static_style("link", (n00b_render_style_t *)&default_link);
    static_style("callout_cell", (n00b_render_style_t *)&default_callout_cell);
    static_style("callout", (n00b_render_style_t *)&default_callout);
}

n00b_grid_t *
n00b_style_details(n00b_render_style_t *s)
{
    n00b_grid_t *grid = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_cols",
                                         n00b_ka(2),
                                         "header_cols",
                                         n00b_ka(1),
                                         "stripe",
                                         n00b_ka(true)));

    n00b_list_t *row  = n00b_new_table_row();
    n00b_utf8_t *nope = n00b_rich_lit("[i]Not set[/]");

    n00b_list_append(row, n00b_new_utf8("Name: "));

    if (s->name == NULL) {
        n00b_list_append(row, nope);
    }
    else {
        n00b_list_append(row, s->name);
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Border Theme: "));
    n00b_border_theme_t *theme = n00b_style_get_border_theme(s);

    if (theme == NULL) {
        n00b_list_append(row, nope);
    }
    else {
        n00b_list_append(row, n00b_new_utf8(theme->name));
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Base FG Color: "));

    if (!n00b_style_is_fg_color_on(s)) {
        n00b_list_append(row, nope);
    }
    else {
        n00b_list_append(row, n00b_cstr_format("{:x}", n00b_style_get_fg_color(s)));
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Base BG Color: "));

    if (!n00b_style_is_bg_color_on(s)) {
        n00b_list_append(row, nope);
    }
    else {
        n00b_list_append(row, n00b_cstr_format("{:x}", n00b_style_get_bg_color(s)));
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Pad Color: "));

    if (!n00b_style_is_pad_color_on(s)) {
        n00b_list_append(row, nope);
    }
    else {
        n00b_list_append(row, n00b_cstr_format("{:x}", n00b_style_get_pad_color(s)));
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_t *text_items = n00b_list(n00b_type_utf8());
    n00b_utf8_t *ti;

    if (n00b_style_get_bold(s)) {
        n00b_list_append(text_items, n00b_new_utf8("bold"));
    }

    if (n00b_style_get_italic(s)) {
        n00b_list_append(text_items, n00b_new_utf8("italic"));
    }

    if (n00b_style_get_strikethru(s)) {
        n00b_list_append(text_items, n00b_new_utf8("strikethru"));
    }

    if (n00b_style_get_underline(s)) {
        n00b_list_append(text_items, n00b_new_utf8("underline"));
    }

    if (n00b_style_get_double_underline(s)) {
        n00b_list_append(text_items, n00b_new_utf8("2xunderline"));
    }

    if (n00b_style_get_reverse(s)) {
        n00b_list_append(text_items, n00b_new_utf8("reverse"));
    }

    if (n00b_style_get_upper(s)) {
        n00b_list_append(text_items, n00b_new_utf8("upper"));
    }

    if (n00b_style_get_lower(s)) {
        n00b_list_append(text_items, n00b_new_utf8("lower"));
    }

    if (n00b_style_get_title(s)) {
        n00b_list_append(text_items, n00b_new_utf8("title"));
    }

    if (n00b_list_len(text_items) == 0) {
        ti = nope;
    }
    else {
        ti = n00b_to_utf8(n00b_str_join(text_items, n00b_new_utf8(", ")));
    }

    n00b_list_append(row, n00b_new_utf8("Text styling: "));
    n00b_list_append(row, ti);
    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Cell padding: "));
    n00b_utf8_t *pads = n00b_cstr_format(
        "t: {}, b: {}, l: {}, r: {}",
        n00b_style_tpad_set(s) ? (void *)n00b_style_get_top_pad(s) : nope,
        n00b_style_bpad_set(s) ? (void *)n00b_style_get_bottom_pad(s) : nope,
        n00b_style_lpad_set(s) ? (void *)n00b_style_get_left_pad(s) : nope,
        n00b_style_rpad_set(s) ? (void *)n00b_style_get_right_pad(s) : nope);

    n00b_list_append(row, pads);
    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_list_append(row, n00b_new_utf8("Wrap: "));
    int64_t      wrap = n00b_style_get_wrap(s);
    n00b_utf8_t *hang = n00b_style_hang_set(s) ? n00b_new_utf8("y") : n00b_new_utf8("n");

    switch (wrap) {
    case -1:
        n00b_list_append(row, n00b_new_utf8("[i]Disabled (will overflow)[/]"));
        break;
    case 0:
        n00b_list_append(row, nope);
        break;
    default:
        n00b_list_append(row, n00b_cstr_format("{} cols (hang = {})", wrap, hang));
        break;
    }

    n00b_grid_add_row(grid, row);
    row = n00b_new_table_row();

    n00b_utf8_t *col_kind;

    switch (n00b_style_col_kind(s)) {
    case N00B_DIM_AUTO:
        col_kind = n00b_new_utf8("Auto-fit");
        break;
    case N00B_DIM_PERCENT_TRUNCATE:
        col_kind = n00b_cstr_format("{}% (truncate)");
        break;
    case N00B_DIM_PERCENT_ROUND:
        col_kind = n00b_cstr_format("{}% (round)");
        break;
    case N00B_DIM_FLEX_UNITS:
        col_kind = n00b_cstr_format("{} flex units");
        break;
    case N00B_DIM_ABSOLUTE:
        col_kind = n00b_cstr_format("{} columns");
        break;
    case N00B_DIM_ABSOLUTE_RANGE:
        col_kind = n00b_cstr_format("{} - {} columns");
        break;
    case N00B_DIM_FIT_TO_TEXT:
        col_kind = n00b_cstr_format("size to longest cell (fit-to-text)");
        break;
    default: // N00B_DIM_UNSET
        col_kind = nope;
        break;
    }

    n00b_list_append(row, n00b_new_utf8("Column setup: "));
    n00b_list_append(row, col_kind);
    n00b_grid_add_row(grid, row);

    return grid;
}

const n00b_vtable_t n00b_render_style_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_style_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]   = NULL,
    },
};
