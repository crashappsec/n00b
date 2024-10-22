#pragma once
#include "n00b.h"

typedef struct {
    n00b_utf8_t *name;
    union {
        n00b_style_t style;
        n00b_color_t color;
        int         kw_ix;
    } contents;
    uint32_t    flags;
    n00b_style_t prev_style;

} n00b_tag_item_t;

typedef struct n00b_fmt_frame_t {
    struct n00b_fmt_frame_t *next;
    n00b_utf8_t             *raw_contents;
    int32_t                 start;
    int32_t                 end;
    n00b_style_t             style; // Calculated incremental style from a tag.
} n00b_fmt_frame_t;

typedef struct {
    n00b_fmt_frame_t *start_frame;
    n00b_fmt_frame_t *cur_frame;
    n00b_list_t      *style_directions;
    n00b_utf8_t      *style_text;
    n00b_tag_item_t **stack;
    n00b_utf8_t      *raw;
    n00b_style_t      cur_style;
    int              stack_ix;
} n00b_style_ctx;

typedef struct {
    n00b_utf8_t      *raw;
    n00b_list_t      *tokens;
    n00b_fmt_frame_t *cur_frame;
    n00b_style_ctx   *style_ctx;
    n00b_utf8_t      *not_matched;
    int              num_toks;
    int              tok_ix;
    int              num_atoms;
    bool             negating;
    bool             at_start;
    bool             got_percent;
    n00b_style_t      cur_style;
} n00b_tag_parse_ctx;

#define N00B_F_NEG         (1 << 1)
#define N00B_F_BGCOLOR     (1 << 2)
#define N00B_F_STYLE_KW    (1 << 3)
#define N00B_F_STYLE_CELL  (1 << 4)
#define N00B_F_STYLE_COLOR (1 << 5)
#define N00B_F_TAG_START   (1 << 6)
#define N00B_F_POPPED      (1 << 7)
