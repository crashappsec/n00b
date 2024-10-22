#pragma once
#include "n00b.h"

typedef uint64_t n00b_style_t;

typedef struct {
    int64_t     start;
    int64_t     end;
    n00b_style_t info; // 16 bits of flags, 24 bits bg color, 24 bits fg color
} n00b_style_entry_t;

typedef struct {
    int64_t           num_entries;
    n00b_style_entry_t styles[];
} n00b_style_info_t;

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

// This is a 'repository' for style info that can be applied to grids
// or strings. When we apply to strings, anything grid-related gets
// ignored.

// This compacts grid style info, but currently that is internal.
// Some of the items apply to text, some apply to renderables.

typedef struct {
    struct n00b_str_t   *name;
    n00b_border_theme_t *border_theme;
    n00b_color_t         pad_color;
    n00b_style_t         base_style;

    union {
        float    percent;
        uint64_t units;
        int32_t  range[2];
    } dims;

    int8_t top_pad;
    int8_t bottom_pad;
    int8_t left_pad;
    int8_t right_pad;
    int8_t wrap;

    int weight_fg;
    int weight_bg;
    int weight_flags;
    int weight_align;
    int weight_width;
    int weight_borders;

    // Eventually we'll add more in like z-ordering and transparency.
    n00b_alignment_t    alignment     : 7;
    n00b_dimspec_kind_t dim_kind      : 3;
    n00b_border_set_t   borders       : 6;
    uint8_t            pad_color_set : 1;
    uint8_t            disable_wrap  : 1;
    uint8_t            tpad_set      : 1; // These prevent overrides when
    uint8_t            bpad_set      : 1; // the pad value is 0. If it's not
    uint8_t            lpad_set      : 1; // 0, this gets ignored.
    uint8_t            rpad_set      : 1;
    uint8_t            hang_set      : 1;
} n00b_render_style_t;
