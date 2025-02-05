#include "n00b.h"
#pragma once

#define N00B_BORDER_TOP          0x01
#define N00B_BORDER_BOTTOM       0x02
#define N00B_BORDER_LEFT         0x04
#define N00B_BORDER_RIGHT        0x08
#define N00B_INTERIOR_HORIZONTAL 0x10
#define N00B_INTERIOR_VERTICAL   0x20

typedef enum : int8_t {
    N00B_ALIGN_IGNORE = 0,
    N00B_ALIGN_LEFT   = 1,
    N00B_ALIGN_RIGHT  = 2,
    N00B_ALIGN_CENTER = 4,
    N00B_ALIGN_TOP    = 8,
    N00B_ALIGN_BOTTOM = 16,
    N00B_ALIGN_MIDDLE = 32,

    N00B_ALIGN_TOP_LEFT   = N00B_ALIGN_LEFT | N00B_ALIGN_TOP,
    N00B_ALIGN_TOP_RIGHT  = N00B_ALIGN_RIGHT | N00B_ALIGN_TOP,
    N00B_ALIGN_TOP_CENTER = N00B_ALIGN_CENTER | N00B_ALIGN_TOP,

    N00B_ALIGN_MID_LEFT   = N00B_ALIGN_LEFT | N00B_ALIGN_MIDDLE,
    N00B_ALIGN_MID_RIGHT  = N00B_ALIGN_RIGHT | N00B_ALIGN_MIDDLE,
    N00B_ALIGN_MID_CENTER = N00B_ALIGN_CENTER | N00B_ALIGN_MIDDLE,

    N00B_ALIGN_BOTTOM_LEFT   = N00B_ALIGN_LEFT | N00B_ALIGN_BOTTOM,
    N00B_ALIGN_BOTTOM_RIGHT  = N00B_ALIGN_RIGHT | N00B_ALIGN_BOTTOM,
    N00B_ALIGN_BOTTOM_CENTER = N00B_ALIGN_CENTER | N00B_ALIGN_BOTTOM,
} n00b_alignment_t;

#define N00B_HORIZONTAL_MASK \
    (N00B_ALIGN_LEFT | N00B_ALIGN_CENTER | N00B_ALIGN_RIGHT)
#define N00B_VERTICAL_MASK \
    (N00B_ALIGN_TOP | N00B_ALIGN_MIDDLE | N00B_ALIGN_BOTTOM)

typedef enum : uint8_t {
    N00B_DIM_UNSET,
    N00B_DIM_AUTO,
    N00B_DIM_PERCENT_TRUNCATE,
    N00B_DIM_PERCENT_ROUND,
    N00B_DIM_FLEX_UNITS,
    N00B_DIM_ABSOLUTE,
    N00B_DIM_ABSOLUTE_RANGE,
    N00B_DIM_FIT_TO_TEXT
} n00b_dimspec_kind_t;

typedef struct border_theme_t {
    struct border_theme_t *next_style;
    char                  *name;
    int32_t                horizontal_rule;
    int32_t                vertical_rule;
    int32_t                upper_left;
    int32_t                upper_right;
    int32_t                lower_left;
    int32_t                lower_right;
    int32_t                cross;
    int32_t                top_t;
    int32_t                bottom_t;
    int32_t                left_t;
    int32_t                right_t;
} n00b_border_theme_t;

typedef uint8_t n00b_border_set_t;

typedef enum {
    N00B_THEME_PRIMARY_BACKGROUND,
    N00B_THEME_PRIMARY_TEXT,
    N00B_THEME_LIGHTER_BACKGROUND,
    N00B_THEME_LIGHTEST_BACKGROUND,
    N00B_THEME_DARKER_BACKGROUND,
    N00B_THEME_DARKEST_BACKGROUND,
    N00B_THEME_LIGHTER_TEXT,
    N00B_THEME_LIGHTEST_TEXT,
    N00B_THEME_DARKER_TEXT,
    N00B_THEME_DARKEST_TEXT,
    N00B_THEME_ACCENT_1,
    N00B_THEME_ACCENT_2,
    N00B_THEME_ACCENT_3,
    N00B_THEME_ACCENT_1_LIGHTER,
    N00B_THEME_ACCENT_2_LIGHTER,
    N00B_THEME_ACCENT_3_LIGHTER,
    N00B_THEME_ACCENT_1_DARKER,
    N00B_THEME_ACCENT_2_DARKER,
    N00B_THEME_ACCENT_3_DARKER,
    N00B_THEME_GREY,
    N00B_THEME_PINK,
    N00B_THEME_PURPLE,
    N00B_THEME_GREEN,
    N00B_THEME_ORANGE,
    N00B_THEME_RED,
    N00B_THEME_YELLOW,
    N00B_THEME_BLUE,
    N00B_THEME_BROWN,
    N00B_THEME_WHITE,
    N00B_THEME_BLACK,
    N00B_COLOR_PALATE_SIZE,
    N00B_COLOR_PALATE_UNSPECIFIED,
} n00b_fg_palate_ix_t;

typedef enum {
    N00B_BOX_THEME_FLOW,
    N00B_BOX_THEME_CALLOUT,
    N00B_BOX_THEME_TABLE_BASE,
    N00B_BOX_THEME_ORNATE_BASE,
    N00B_BOX_THEME_SIMPLE_BASE,
    N00B_BOX_THEME_VHEADER_BASE,
    N00B_BOX_THEME_OL_BASE,
    N00B_BOX_THEME_UL_BASE,
    N00B_BOX_THEME_TABLE_CELL_BASE,
    N00B_BOX_THEME_ORNATE_CELL_BASE,
    N00B_BOX_THEME_SIMPLE_CELL_BASE,
    N00B_BOX_THEME_VHEADER_CELL_BASE,
    N00B_BOX_THEME_TABLE_CELL_ALT,
    N00B_BOX_THEME_ORNATE_CELL_ALT,
    N00B_BOX_THEME_SIMPLE_CELL_ALT,
    N00B_BOX_THEME_VHEADER_CELL_ALT,
    N00B_BOX_THEME_TABLE_HEADER,
    N00B_BOX_THEME_ORNATE_HEADER,
    N00B_BOX_THEME_SIMPLE_HEADER,
    N00B_BOX_THEME_VHEADER_HEADER,
    N00B_BOX_UL_CELL_BASE,
    N00B_BOX_UL_BULLET,
    N00B_BOX_OL_CELL_BASE,
    N00B_BOX_OL_BULLET,
    N00B_BOX_THEME_TITLE,
    N00B_BOX_THEME_CAPTION,
    N00B_BOX_PALATE_SIZE,
    N00B_BOX_PALATE_UNSPECIFIED,
} n00b_box_props_ix_t;

typedef enum {
    N00B_BORDER_INFO_UNSPECIFIED = -1,
    N00B_BORDER_INFO_NONE        = 0,
    N00B_BORDER_INFO_LEFT        = 1,
    N00B_BORDER_INFO_RIGHT       = 2,
    N00B_BORDER_INFO_SIDES       = 3,
    N00B_BORDER_INFO_TOP         = 8,
    N00B_BORDER_INFO_BOTTOM      = 16,
    N00B_BORDER_INFO_VINTERIOR   = 32,
    N00B_BORDER_INFO_TYPICAL     = 63,
    N00B_BORDER_INFO_HINTERIOR   = 64,
    N00B_BORDER_INFO_ALL         = 127,
} n00b_border_t;

typedef enum n00b_tristate_t {
    N00B_3_NO,
    N00B_3_YES,
    N00B_3_UNSPECIFIED,
} n00b_tristate_t;

typedef enum n00b_text_case_t {
    N00B_TEXT_AS_IS,
    N00B_TEXT_UPPER,
    N00B_TEXT_LOWER,
    N00B_TEXT_ALL_CAPS,
    N00B_TEXT_UNSPECIFIED,
} n00b_text_case_t;

typedef struct n00b_text_element_t {
    int8_t           fg_palate_index; // UNSPECIFIED == "don't care"
    int8_t           bg_palate_index;
    n00b_tristate_t  underline        : 2;
    n00b_tristate_t  double_underline : 2;
    n00b_tristate_t  bold             : 2;
    n00b_tristate_t  italic           : 2;
    n00b_tristate_t  strikethrough    : 2;
    n00b_tristate_t  reverse          : 2;
    n00b_text_case_t text_case        : 3;
    // The way we cache codes can potentially lead to race conditions
    // if you're using the same string w/ different themes, etc across
    // threads.
    //
    // I chose this over other options because that seems like bad
    // behavior anyway.
    char            *ansi_cache;
    int              ansi_len;
    void            *last_theme;
    void            *last_inherit;
} n00b_text_element_t;

typedef struct n00b_box_props_t {
#if defined(N00B_DEBUG)
    char *debug_name;
#endif
    n00b_border_theme_t *border_theme;
    n00b_dict_t         *col_info; // Dict of # to n00b_col_kind_t
    n00b_text_element_t  text_style;
    int8_t               cols_to_span;
    int8_t               left_pad; // -1 == "don't care"
    int8_t               right_pad;
    int8_t               top_pad;
    int8_t               bottom_pad;
    int8_t               default_header_rows;
    int8_t               default_header_cols;
    n00b_tristate_t      wrap;
    n00b_border_t        borders;
    int8_t               hang;
    n00b_alignment_t     alignment;
} n00b_box_props_t;

typedef struct n00b_tree_props_t {
    n00b_codepoint_t    pad;
    n00b_codepoint_t    tchar;
    n00b_codepoint_t    lchar;
    n00b_codepoint_t    hchar;
    n00b_codepoint_t    vchar;
    int8_t              vpad;
    int8_t              ipad;
    n00b_text_element_t text_style;
    bool                remove_newlines;
} n00b_tree_props_t;

typedef struct n00b_theme_t {
    n00b_string_t    *name;
    n00b_string_t    *author;
    n00b_string_t    *description;
    n00b_tree_props_t tree_props;
    n00b_box_props_t *box_info[N00B_BOX_PALATE_SIZE];
    n00b_color_t      palate[N00B_COLOR_PALATE_SIZE];
} n00b_theme_t;

extern n00b_box_props_t         *n00b_lookup_style_by_tag(n00b_string_t *);
extern n00b_box_props_t         *n00b_lookup_style_by_hash(hatrack_hash_t);
extern void                      n00b_set_current_theme(n00b_theme_t *);
extern n00b_theme_t             *n00b_lookup_theme(n00b_string_t *);
extern n00b_theme_t             *n00b_get_current_theme(void);
extern n00b_string_t            *n00b_preview_theme(n00b_theme_t *);
extern n00b_string_t            *n00b_theme_palate_table(n00b_theme_t *);
extern const n00b_border_theme_t n00b_border_plain;
extern const n00b_border_theme_t n00b_border_bold;
extern const n00b_border_theme_t n00b_border_double;
extern const n00b_border_theme_t n00b_border_dash;
extern const n00b_border_theme_t n00b_border_bold_dash;
extern const n00b_border_theme_t n00b_border_dash2;
extern const n00b_border_theme_t n00b_border_bold_dash2;
extern const n00b_border_theme_t n00b_border_asterisk;
extern const n00b_border_theme_t n00b_border_ascii;

#define N00B_DEFAULT_THEME n00b_cstring("n00b-default")

static inline n00b_tree_props_t *
n00b_get_current_tree_formatting(void)
{
    n00b_theme_t *theme = n00b_get_current_theme();
    if (theme->tree_props.pad == 0) {
        theme = n00b_lookup_theme(n00b_cstring("n00b-default"));
    }

    return &theme->tree_props;
}

#ifdef N00B_USE_INTERNAL_API
extern void n00b_theme_initialization(void);

#endif

#ifndef N00B_DEFAULT_TABLE_PAD
#define N00B_DEFAULT_TABLE_PAD 0
#endif
