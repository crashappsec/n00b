#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_dict_t  *n00b_theme_repository;
n00b_dict_t  *n00b_tag_repository;
n00b_theme_t *n00b_installed_theme;

const n00b_border_theme_t n00b_border_plain = {
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
};

const n00b_border_theme_t n00b_border_bold = {
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
};

const n00b_border_theme_t n00b_border_double = {
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
};

const n00b_border_theme_t n00b_border_dash = {
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
};

const n00b_border_theme_t n00b_border_bold_dash = {
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
};

const n00b_border_theme_t n00b_border_dash2 = {
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
};

const n00b_border_theme_t n00b_border_bold_dash2 = {
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
};

const n00b_border_theme_t n00b_border_asterisk = {
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
};

const n00b_border_theme_t n00b_border_ascii = {
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
};

static inline n00b_theme_t *
initialize_default_theme(void)
{
    // Box props for the default style.
    n00b_box_props_t *table_props;
    n00b_box_props_t *table_cell;
    n00b_box_props_t *table_cell_alt;
    n00b_box_props_t *ornate_props;
    n00b_box_props_t *ornate_cell;
    n00b_box_props_t *ornate_cell_alt;
    n00b_box_props_t *simple_props;
    n00b_box_props_t *simple_cell;
    n00b_box_props_t *callout_cell;
    n00b_box_props_t *table_head;
    n00b_box_props_t *list_base;
    n00b_box_props_t *list_bullet;
    n00b_box_props_t *list_cell;

    table_head                             = n00b_new(n00b_type_box_props());
    table_head->text_style.fg_palate_index = N00B_THEME_ACCENT_1_LIGHTER;
    table_head->text_style.bg_palate_index = N00B_THEME_LIGHTER_BACKGROUND;
    table_head->text_style.text_case       = N00B_TEXT_UPPER;
    table_head->text_style.bold            = N00B_3_YES;
    //    table_head->text_style.underline       = N00B_3_YES;
    table_head->left_pad                   = 1;
    table_head->right_pad                  = 1;
    table_head->top_pad                    = 1;

    table_props = n00b_new(n00b_type_box_props(),
                           n00b_kw("border_theme",
                                   &n00b_border_bold_dash2,
                                   "borders",
                                   n00b_ka(N00B_BORDER_INFO_TYPICAL)));

    table_props->text_style.bg_palate_index = N00B_THEME_ACCENT_2_DARKER;
    table_props->default_header_rows        = 1;
    table_props->left_pad                   = 1;
    table_props->right_pad                  = 1;
    table_props->bottom_pad                 = 1;
    table_props->alignment                  = N00B_ALIGN_MID_LEFT;

    // This is used for captions and titles too.
    table_head->alignment = N00B_ALIGN_BOTTOM_CENTER;

    table_cell = n00b_new(n00b_type_box_props(),
                          n00b_kw("border_theme",
                                  &n00b_border_bold_dash2,
                                  "borders",
                                  n00b_ka(N00B_BORDER_INFO_SIDES)));

    table_cell->text_style.fg_palate_index = N00B_THEME_DARKEST_TEXT;
    table_cell->text_style.bg_palate_index = N00B_THEME_ACCENT_2;
    table_cell->left_pad                   = 1;
    table_cell->right_pad                  = 1;
    table_cell_alt                         = n00b_new(n00b_type_box_props(),
                              n00b_kw("border_theme",
                                      n00b_ka(N00B_BORDER_INFO_SIDES)));

    table_cell_alt->text_style.fg_palate_index = N00B_THEME_DARKER_TEXT;
    table_cell_alt->text_style.bg_palate_index = N00B_THEME_ACCENT_2_LIGHTER;
    table_cell_alt->text_style.bold            = N00B_3_YES;
    table_cell_alt->left_pad                   = 1;
    table_cell_alt->right_pad                  = 1;
    ornate_props                               = n00b_new(n00b_type_box_props(),
                            n00b_kw("border_theme",
                                    &n00b_border_bold_dash2,
                                    "borders",
                                    n00b_ka(N00B_BORDER_INFO_ALL)));
    ornate_props->alignment                    = N00B_ALIGN_MID_LEFT;
    ornate_props->bottom_pad                   = 1;
    ornate_cell                                = table_cell;
    ornate_cell_alt                            = table_cell_alt;

    simple_props                             = n00b_new(n00b_type_box_props(),
                            n00b_kw("border_theme", NULL));
    simple_props->alignment                  = N00B_ALIGN_MID_LEFT;
    simple_props->bottom_pad                 = 1;
    simple_props->text_style.bg_palate_index = N00B_THEME_ACCENT_2;

    simple_cell                             = n00b_new(n00b_type_box_props(),

                           n00b_kw("border_theme", NULL));
    simple_cell->text_style.fg_palate_index = N00B_THEME_PRIMARY_TEXT;
    simple_cell->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    simple_cell->left_pad                   = 1;
    simple_cell->right_pad                  = 1;
    simple_cell->alignment                  = N00B_ALIGN_MID_LEFT;

    callout_cell                             = n00b_new(n00b_type_box_props(),
                            n00b_kw("border_theme",
                                    &n00b_border_bold,
                                    "borders",
                                    n00b_ka(N00B_BORDER_INFO_ALL)));
    callout_cell->text_style.fg_palate_index = N00B_THEME_DARKER_TEXT;
    callout_cell->text_style.bg_palate_index = N00B_THEME_ACCENT_2;
    callout_cell->text_style.bold            = true;
    callout_cell->text_style.italic          = true;
    callout_cell->left_pad                   = 1;
    callout_cell->right_pad                  = 1;
    callout_cell->top_pad                    = 1;
    callout_cell->bottom_pad                 = 1;
    callout_cell->alignment                  = N00B_ALIGN_MID_CENTER;

    list_base             = n00b_new(n00b_type_box_props());
    list_base->alignment  = N00B_ALIGN_MID_LEFT;
    list_base->left_pad   = 0;
    list_base->right_pad  = 1;
    list_base->top_pad    = 1;
    list_base->bottom_pad = 1;

    list_cell                             = n00b_new(n00b_type_box_props());
    list_cell->text_style.fg_palate_index = N00B_THEME_PRIMARY_TEXT;
    list_cell->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    list_cell->alignment                  = N00B_ALIGN_TOP_LEFT;
    list_cell->left_pad                   = 1;

    list_bullet                             = n00b_new(n00b_type_box_props());
    list_bullet->text_style.fg_palate_index = N00B_THEME_ACCENT_3;
    list_bullet->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    list_bullet->alignment                  = N00B_ALIGN_TOP_RIGHT;
    list_bullet->left_pad                   = 0;
    list_bullet->right_pad                  = 0;

    n00b_theme_t *n00b_theme;
    n00b_theme_t *result;

    // C0_Primary_Text = E6E2DC
    // C0_Darker_Accent1 = B3FF00 // Atomic lime
    // C0_Darker_Accent2 = FF2F8E // Jazzberry
    // C0_Darker_Accent3 = 523AA4 // Fandango

#ifdef N00B_DEBUG
    table_head->debug_name      = "table head cell";
    callout_cell->debug_name    = "callout";
    table_props->debug_name     = "normal table";
    ornate_props->debug_name    = "ornate table";
    simple_props->debug_name    = "simple table";
    table_cell->debug_name      = "normal cell";
    table_cell_alt->debug_name  = "alt cell";
    simple_cell->debug_name     = "simple cell";
    ornate_cell->debug_name     = "ornate cell";
    ornate_cell_alt->debug_name = "ornate alt";
#endif

    n00b_theme = n00b_new(n00b_type_theme(),
                          n00b_kw("name",
                                  n00b_cstring("n00b-bright"),
                                  "author",
                                  n00b_cstring("John Viega"),
                                  "description",
                                  n00b_cstring("Default installed theme"),
                                  "primary_text",
                                  n00b_ka(0xe6e2dc),
                                  "lighter_text",
                                  n00b_ka(0xeeebe8),
                                  "lightest_text",
                                  n00b_ka(0xfcfcfb),
                                  "darker_text",
                                  n00b_ka(0x232937),
                                  "darkest_text",
                                  n00b_ka(0x0b1221),
                                  "primary_background",
                                  n00b_ka(0x0b1221),
                                  "lighter_background",
                                  n00b_ka(0x232937),
                                  "lightest_background",
                                  n00b_ka(0x3c414d),
                                  "darker_background",
                                  n00b_ka(0x050610),
                                  "darkest_background",
                                  n00b_ka(0x000000),
                                  "accent_1",
                                  n00b_ka(0xbfff33),
                                  "accent_1_lighter",
                                  n00b_ka(0xcfff66),
                                  "accent_1_darker",
                                  n00b_ka(0xb3ff00),
                                  "accent_2",
                                  n00b_ka(0xff59a1),
                                  "accent_2_lighter",
                                  n00b_ka(0xffacd0),
                                  "accent_2_darker",
                                  n00b_ka(0xff2f8e),
                                  "accent_3",
                                  n00b_ka(0x7561b3),
                                  "accent_3_lighter",
                                  n00b_ka(0x9789c6),
                                  "accent_3_darker",
                                  n00b_ka(0x523aa4),
                                  "grey",
                                  n00b_ka(0x6d717a), // in-pallate
                                  "pink",
                                  n00b_ka(0xff82b9), // lighter jazzberry
                                  "purple",
                                  n00b_ka(0x523aa4), // fandango
                                  "green",
                                  n00b_ka(0xc7ff4c), // atomic lime
                                  "orange",
                                  n00b_ka(0xffbf00), // fluorescent orange
                                  "red",
                                  n00b_ka(0xff0800), // candy apple
                                  "yellow",
                                  n00b_ka(0xfdfd96), // pastel yellow
                                  "blue",
                                  n00b_ka(0x00bfff), // sky blue
                                  "brown",
                                  n00b_ka(0xa0522d), // sienna
                                  "white",
                                  n00b_ka(0xfcfcfb),
                                  "black",
                                  n00b_ka(0x0b1221),
                                  "base_table_props",
                                  table_props,
                                  "ornate_table_props",
                                  ornate_props,
                                  "simple_table_props",
                                  simple_props,
                                  "table_cell_props",
                                  table_cell,
                                  "ornate_cell_props",
                                  ornate_cell,
                                  "simple_cell_props",
                                  simple_cell,
                                  "table_alt_cell",
                                  table_cell_alt,
                                  "ornate_alt_cell",
                                  ornate_cell_alt,
                                  "flow_props",
                                  simple_cell,
                                  "callout_props",
                                  callout_cell,
                                  "ol_base",
                                  list_base,
                                  "ol_cell",
                                  list_cell,
                                  "ol_bullet",
                                  list_bullet,
                                  "ul_base",
                                  list_base,
                                  "ul_cell",
                                  list_cell,
                                  "ul_bullet",
                                  list_bullet));

    n00b_theme->box_info[N00B_BOX_THEME_TABLE_HEADER]  = table_head;
    n00b_theme->box_info[N00B_BOX_THEME_ORNATE_HEADER] = table_head;

    n00b_theme->tree_props.pad                         = ' ';
    n00b_theme->tree_props.tchar                       = 0x251c;
    n00b_theme->tree_props.lchar                       = 0x2514;
    n00b_theme->tree_props.hchar                       = 0x2500;
    n00b_theme->tree_props.vchar                       = 0x2502;
    n00b_theme->tree_props.vpad                        = 2;
    n00b_theme->tree_props.ipad                        = 1;
    n00b_theme->tree_props.remove_newlines             = true;
    n00b_theme->tree_props.text_style.fg_palate_index  = N00B_THEME_LIGHTEST_TEXT;
    n00b_theme->tree_props.text_style.bg_palate_index  = N00B_THEME_DARKEST_BACKGROUND;
    n00b_theme->tree_props.text_style.bold             = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.italic           = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.underline        = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.double_underline = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.strikethrough    = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.reverse          = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.text_case        = N00B_TEXT_AS_IS;

    result = n00b_theme;
    // Second theme...

    n00b_theme = n00b_new(n00b_type_theme(),
                          n00b_kw("name",
                                  n00b_cstring("n00b-dark"),
                                  "author",
                                  n00b_cstring("John Viega"),
                                  "description",
                                  n00b_cstring("Less flashy"),
                                  "primary_text",
                                  n00b_ka(0xe6e2dc),
                                  "lighter_text",
                                  n00b_ka(0xeeebe8),
                                  "lightest_text",
                                  n00b_ka(0xfcfcfb),
                                  "darker_text",
                                  n00b_ka(0x232937),
                                  "darkest_text",
                                  n00b_ka(0x0b1221),
                                  "primary_background",
                                  n00b_ka(0x0b1221),
                                  "lighter_background",
                                  n00b_ka(0x232937),
                                  "lightest_background",
                                  n00b_ka(0x3c414d),
                                  "darker_background",
                                  n00b_ka(0x050610),
                                  "darkest_background",
                                  n00b_ka(0x000000),
                                  "accent_1",
                                  n00b_ka(0xbfff33),
                                  "accent_1_lighter",
                                  n00b_ka(0xcfff66),
                                  "accent_1_darker",
                                  n00b_ka(0xb3ff00),
                                  "accent_3",
                                  n00b_ka(0xff59a1),
                                  "accent_3_lighter",
                                  n00b_ka(0xffacd0),
                                  "accent_3_darker",
                                  n00b_ka(0xff2f8e),
                                  "accent_2",
                                  n00b_ka(0x7561b3),
                                  "accent_2_lighter",
                                  n00b_ka(0x9789c6),
                                  "accent_2_darker",
                                  n00b_ka(0x523aa4),
                                  "grey",
                                  n00b_ka(0x6d717a), // in-pallate
                                  "pink",
                                  n00b_ka(0xff82b9), // lighter jazzberry
                                  "purple",
                                  n00b_ka(0x523aa4), // fandango
                                  "green",
                                  n00b_ka(0xc7ff4c), // atomic lime
                                  "orange",
                                  n00b_ka(0xffbf00), // fluorescent orange
                                  "red",
                                  n00b_ka(0xff0800), // candy apple
                                  "yellow",
                                  n00b_ka(0xfdfd96), // pastel yellow
                                  "blue",
                                  n00b_ka(0x00bfff), // sky blue
                                  "brown",
                                  n00b_ka(0xa0522d), // sienna
                                  "white",
                                  n00b_ka(0xfcfcfb),
                                  "black",
                                  n00b_ka(0x0b1221),
                                  "base_table_props",
                                  table_props,
                                  "ornate_table_props",
                                  ornate_props,
                                  "simple_table_props",
                                  simple_props,
                                  "table_cell_props",
                                  table_cell,
                                  "ornate_cell_props",
                                  ornate_cell,
                                  "simple_cell_props",
                                  simple_cell,
                                  "table_alt_cell",
                                  table_cell_alt,
                                  "ornate_alt_cell",
                                  ornate_cell_alt,
                                  "flow_props",
                                  simple_cell,
                                  "callout_props",
                                  callout_cell,
                                  "ol_base",
                                  list_base,
                                  "ol_cell",
                                  list_cell,
                                  "ol_bullet",
                                  list_bullet,
                                  "ul_base",
                                  list_base,
                                  "ul_cell",
                                  list_cell,
                                  "ul_bullet",
                                  list_bullet));

    n00b_theme->box_info[N00B_BOX_THEME_TABLE_HEADER]  = table_head;
    n00b_theme->box_info[N00B_BOX_THEME_ORNATE_HEADER] = table_head;

    n00b_theme->tree_props.pad                         = ' ';
    n00b_theme->tree_props.tchar                       = 0x251c;
    n00b_theme->tree_props.lchar                       = 0x2514;
    n00b_theme->tree_props.hchar                       = 0x2500;
    n00b_theme->tree_props.vchar                       = 0x2502;
    n00b_theme->tree_props.vpad                        = 2;
    n00b_theme->tree_props.ipad                        = 1;
    n00b_theme->tree_props.remove_newlines             = true;
    n00b_theme->tree_props.text_style.fg_palate_index  = N00B_THEME_LIGHTEST_TEXT;
    n00b_theme->tree_props.text_style.bg_palate_index  = N00B_THEME_DARKEST_BACKGROUND;
    n00b_theme->tree_props.text_style.bold             = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.italic           = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.underline        = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.double_underline = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.strikethrough    = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.reverse          = N00B_3_UNSPECIFIED;
    n00b_theme->tree_props.text_style.text_case        = N00B_TEXT_AS_IS;

    return result;
}

static inline void
initialize_tag_repo(void)
{
    // Setup props for default styles used primarily for text.  These
    // *can* be overridden, but generally shouldn't be. The only
    // practical exception is changing the padding for headings for
    // when they're used in a table context.

    // This is the default style for text.
    n00b_box_props_t *p_props       = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h1_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h2_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h3_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h4_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h5_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *h6_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em1_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em2_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em3_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em4_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em5_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *em6_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *bold_props    = n00b_new(n00b_type_box_props());
    n00b_box_props_t *italic_props  = n00b_new(n00b_type_box_props());
    n00b_box_props_t *u_props       = n00b_new(n00b_type_box_props());
    n00b_box_props_t *uu_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *st_props      = n00b_new(n00b_type_box_props());
    n00b_box_props_t *rev_props     = n00b_new(n00b_type_box_props());
    n00b_box_props_t *title_props   = n00b_new(n00b_type_box_props());
    n00b_box_props_t *caption_props = n00b_new(n00b_type_box_props());
    n00b_box_props_t *head_props    = n00b_new(n00b_type_box_props());

    p_props->text_style.fg_palate_index = N00B_THEME_PRIMARY_TEXT;
    p_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;

    h1_props->text_style.fg_palate_index = N00B_THEME_ACCENT_1;
    h1_props->text_style.bg_palate_index = N00B_THEME_LIGHTER_BACKGROUND;
    h1_props->text_style.text_case       = N00B_TEXT_UPPER;
    h1_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h1_props->top_pad                    = 1;
    h1_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h1_props->text_style.bold            = N00B_3_YES;

    h2_props->text_style.fg_palate_index = N00B_THEME_ACCENT_2;
    h2_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    h2_props->text_style.text_case       = N00B_TEXT_UPPER;
    h2_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h2_props->top_pad                    = 1;
    h2_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h2_props->text_style.bold            = N00B_3_YES;

    h3_props->text_style.fg_palate_index = N00B_THEME_ACCENT_3;
    h3_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    h3_props->text_style.text_case       = N00B_TEXT_UPPER;
    h3_props->top_pad                    = 1;
    h3_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h3_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h3_props->text_style.bold            = N00B_3_YES;

    h4_props->text_style.fg_palate_index = N00B_THEME_ACCENT_1;
    h4_props->text_style.bg_palate_index = N00B_THEME_LIGHTER_BACKGROUND;
    h4_props->text_style.reverse         = N00B_3_YES;
    h4_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h4_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h4_props->text_style.bold            = N00B_3_YES;

    h5_props->text_style.fg_palate_index = N00B_THEME_ACCENT_2;
    h5_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    h5_props->text_style.reverse         = N00B_3_YES;
    h5_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h5_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h5_props->text_style.bold            = N00B_3_YES;

    h6_props->text_style.fg_palate_index = N00B_THEME_ACCENT_3;
    h6_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    h6_props->text_style.reverse         = N00B_3_YES;
    h6_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;
    h6_props->cols_to_span               = N00B_COL_SPAN_ALL;
    h6_props->text_style.bold            = N00B_3_YES;

    bold_props->text_style.bold           = N00B_3_YES;
    italic_props->text_style.italic       = N00B_3_YES;
    u_props->text_style.underline         = N00B_3_YES;
    uu_props->text_style.double_underline = N00B_3_YES;
    st_props->text_style.strikethrough    = N00B_3_YES;
    rev_props->text_style.reverse         = N00B_3_YES;

    em1_props->text_style.fg_palate_index = N00B_THEME_ACCENT_1;
    em1_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em1_props->text_style.bold            = N00B_3_YES;

    em2_props->text_style.fg_palate_index = N00B_THEME_ACCENT_2;
    em2_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em2_props->text_style.bold            = N00B_3_YES;

    em3_props->text_style.fg_palate_index = N00B_THEME_ACCENT_3;
    em3_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em3_props->text_style.bold            = N00B_3_YES;

    em4_props->text_style.fg_palate_index = N00B_THEME_ACCENT_1;
    em4_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em4_props->text_style.reverse         = N00B_3_YES;
    em4_props->text_style.bold            = N00B_3_YES;

    em5_props->text_style.fg_palate_index = N00B_THEME_ACCENT_2;
    em5_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em5_props->text_style.reverse         = N00B_3_YES;
    em5_props->text_style.bold            = N00B_3_YES;

    em6_props->text_style.fg_palate_index = N00B_THEME_ACCENT_3;
    em6_props->text_style.bg_palate_index = N00B_THEME_PRIMARY_BACKGROUND;
    em6_props->text_style.reverse         = N00B_3_YES;
    em6_props->text_style.bold            = N00B_3_YES;

    // Right now, this stuff isn't respected.
    title_props->text_style.fg_palate_index = N00B_THEME_DARKEST_TEXT;
    title_props->text_style.bg_palate_index = N00B_THEME_LIGHTEST_BACKGROUND;
    title_props->text_style.text_case       = N00B_TEXT_ALL_CAPS;
    title_props->text_style.bold            = N00B_3_YES;
    title_props->top_pad                    = 1;
    title_props->cols_to_span               = N00B_COL_SPAN_ALL;
    title_props->alignment                  = N00B_ALIGN_BOTTOM_CENTER;

    head_props->text_style.fg_palate_index = N00B_THEME_ACCENT_1_LIGHTER;
    head_props->text_style.bg_palate_index = N00B_THEME_LIGHTER_BACKGROUND;
    head_props->text_style.text_case       = N00B_TEXT_UPPER;
    head_props->text_style.bold            = N00B_3_YES;
    head_props->left_pad                   = 1;
    head_props->right_pad                  = 1;
    head_props->top_pad                    = 1;

    caption_props->text_style.fg_palate_index = N00B_THEME_DARKEST_TEXT;
    caption_props->text_style.bg_palate_index = N00B_THEME_LIGHTEST_BACKGROUND;
    caption_props->text_style.italic          = N00B_3_YES;
    caption_props->cols_to_span               = N00B_COL_SPAN_ALL;
    caption_props->alignment                  = N00B_ALIGN_TOP_CENTER;

    hatrack_dict_put(n00b_tag_repository,
                     n00b_string_from_codepoint('p'),
                     p_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_string_from_codepoint('u'),
                     u_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_string_from_codepoint('b'),
                     bold_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_string_from_codepoint('i'),
                     italic_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_cstring("underline"),
                     u_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("bold"), bold_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("italic"), italic_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h1"), h1_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h2"), h2_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h3"), h3_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h4"), h4_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h5"), h5_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("h6"), h6_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("uu"), uu_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("reverse"), rev_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em"), em1_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em1"), em1_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em2"), em2_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em3"), em3_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em4"), em4_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em5"), em5_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("em6"), em6_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("title"), title_props);
    hatrack_dict_put(n00b_tag_repository, n00b_cstring("head"), head_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_cstring("caption"),
                     caption_props);
    hatrack_dict_put(n00b_tag_repository,
                     n00b_cstring("strikethrough"),
                     st_props);
}

void
n00b_theme_initialization(void)
{
    if (n00b_startup_complete) {
        return;
    }
    n00b_gc_register_root(&n00b_theme_repository, 1);
    n00b_gc_register_root(&n00b_tag_repository, 1);
    n00b_gc_register_root(&n00b_installed_theme, 1);

    n00b_theme_repository = n00b_dict(n00b_type_string(), n00b_type_theme());
    n00b_tag_repository   = n00b_dict(n00b_type_string(), n00b_type_box_props());

    initialize_tag_repo();

    initialize_default_theme();
    n00b_installed_theme = n00b_lookup_theme(N00B_DEFAULT_THEME);
}

static void
n00b_text_element_init(n00b_text_element_t *element, va_list args)
{
    keywords
    {
        n00b_fg_palate_ix_t fg_color         = N00B_COLOR_PALATE_UNSPECIFIED;
        n00b_fg_palate_ix_t bg_color         = N00B_COLOR_PALATE_UNSPECIFIED;
        n00b_tristate_t     underline        = N00B_3_UNSPECIFIED;
        n00b_tristate_t     double_underline = N00B_3_UNSPECIFIED;
        n00b_tristate_t     bold             = N00B_3_UNSPECIFIED;
        n00b_tristate_t     italic           = N00B_3_UNSPECIFIED;
        n00b_tristate_t     strikethrough    = N00B_3_UNSPECIFIED;
        n00b_tristate_t     reverse          = N00B_3_UNSPECIFIED;
        n00b_text_case_t    text_case        = N00B_TEXT_UNSPECIFIED;
    }

    element->fg_palate_index  = fg_color;
    element->bg_palate_index  = bg_color;
    element->underline        = underline;
    element->double_underline = double_underline;
    element->bold             = bold;
    element->italic           = italic;
    element->strikethrough    = strikethrough;
    element->reverse          = reverse;
    element->text_case        = text_case;
}

static void
n00b_box_props_init(n00b_box_props_t *props, va_list args)
{
    keywords
    {
        n00b_border_theme_t *border_theme = NULL;
        n00b_dict_t         *col_info     = NULL;
        n00b_text_element_t *text_style   = NULL;
        int                  cols_to_span = 0;
        int                  left_pad     = -1;
        int                  right_pad    = -1;
        int                  top_pad      = -1;
        int                  bottom_pad   = -1;
        n00b_tristate_t      wrap         = N00B_3_UNSPECIFIED;
        n00b_border_t        borders      = N00B_BORDER_INFO_UNSPECIFIED;
        int                  hang         = -1;
        n00b_alignment_t     alignment    = N00B_ALIGN_IGNORE;
    }

    props->border_theme = border_theme;
    props->col_info     = col_info;
    props->cols_to_span = cols_to_span;
    props->left_pad     = left_pad;
    props->right_pad    = right_pad;
    props->top_pad      = top_pad;
    props->bottom_pad   = bottom_pad;
    props->wrap         = wrap;
    props->borders      = borders;
    props->hang         = hang;
    props->alignment    = alignment;

    if (text_style) {
        memcpy(&props->text_style, text_style, sizeof(n00b_text_element_t));
    }
    else {
        props->text_style.fg_palate_index  = N00B_COLOR_PALATE_UNSPECIFIED;
        props->text_style.bg_palate_index  = N00B_COLOR_PALATE_UNSPECIFIED;
        props->text_style.underline        = N00B_3_UNSPECIFIED;
        props->text_style.double_underline = N00B_3_UNSPECIFIED;
        props->text_style.bold             = N00B_3_UNSPECIFIED;
        props->text_style.italic           = N00B_3_UNSPECIFIED;
        props->text_style.strikethrough    = N00B_3_UNSPECIFIED;
        props->text_style.reverse          = N00B_3_UNSPECIFIED;
        props->text_style.text_case        = N00B_TEXT_UNSPECIFIED;
    }
}

#define set_default_color(dst, src)                     \
    if (src != N00B_NO_COLOR && dst == N00B_NO_COLOR) { \
        dst = src;                                      \
    }

static void
n00b_theme_init(n00b_theme_t *theme, va_list args)
{
    keywords
    {
        n00b_string_t    *name                = NULL;
        n00b_string_t    *author              = NULL;
        n00b_string_t    *description         = NULL;
        // The default background.
        n00b_color_t      primary_background  = N00B_NO_COLOR;
        // Usually used for status bars and other accenty things.
        // Also used for h1 fg / h4 bg color
        n00b_color_t      lighter_background  = N00B_NO_COLOR;
        n00b_color_t      lightest_background = N00B_NO_COLOR;
        // Usually used for text selection and paired with 'lighter text'
        n00b_color_t      darker_background   = N00B_NO_COLOR;
        n00b_color_t      darkest_background  = N00B_NO_COLOR;
        n00b_color_t      primary_text        = N00B_NO_COLOR;
        // Generally used w/ the DARKER background.
        n00b_color_t      lighter_text        = N00B_NO_COLOR;
        n00b_color_t      lightest_text       = N00B_NO_COLOR;
        // Generally used w/ the LIGHTER background.
        n00b_color_t      darker_text         = N00B_NO_COLOR;
        n00b_color_t      darkest_text        = N00B_NO_COLOR;
        // Used for primary table row background
        n00b_color_t      accent_1            = N00B_NO_COLOR;
        n00b_color_t      accent_1_lighter    = N00B_NO_COLOR;
        n00b_color_t      accent_1_darker     = N00B_NO_COLOR;
        // Used for striped table row background
        n00b_color_t      accent_2            = N00B_NO_COLOR;
        n00b_color_t      accent_2_lighter    = N00B_NO_COLOR;
        n00b_color_t      accent_2_darker     = N00B_NO_COLOR;
        // Used for h2 fg / h5 bg color; other color is PRIMARY_TEXT or PRIMARY_BG
        n00b_color_t      accent_3            = N00B_NO_COLOR;
        n00b_color_t      accent_3_lighter    = N00B_NO_COLOR;
        n00b_color_t      accent_3_darker     = N00B_NO_COLOR;
        // Used for h3 fg / h6 bg color
        n00b_color_t      grey                = N00B_NO_COLOR;
        n00b_color_t      pink                = N00B_NO_COLOR;
        n00b_color_t      purple              = N00B_NO_COLOR;
        n00b_color_t      green               = N00B_NO_COLOR;
        n00b_color_t      orange              = N00B_NO_COLOR;
        n00b_color_t      red                 = N00B_NO_COLOR;
        n00b_color_t      yellow              = N00B_NO_COLOR;
        n00b_color_t      blue                = N00B_NO_COLOR;
        n00b_color_t      brown               = N00B_NO_COLOR;
        n00b_color_t      white               = N00B_NO_COLOR;
        n00b_color_t      black               = N00B_NO_COLOR;
        n00b_box_props_t *base_table_props    = NULL;
        n00b_box_props_t *ornate_table_props  = NULL;
        n00b_box_props_t *simple_table_props  = NULL;
        n00b_box_props_t *table_cell_props    = NULL;
        n00b_box_props_t *ornate_cell_props   = NULL;
        n00b_box_props_t *simple_cell_props   = NULL;
        n00b_box_props_t *table_alt_cell      = NULL;
        n00b_box_props_t *ornate_alt_cell     = NULL;
        n00b_box_props_t *simple_alt_cell     = NULL;
        n00b_box_props_t *flow_props          = NULL;
        n00b_box_props_t *callout_props       = NULL;
        n00b_box_props_t *table_title         = NULL;
        n00b_box_props_t *table_caption       = NULL;
        n00b_box_props_t *ol_base             = NULL;
        n00b_box_props_t *ol_cell             = NULL;
        n00b_box_props_t *ol_bullet           = NULL;
        n00b_box_props_t *ul_base             = NULL;
        n00b_box_props_t *ul_cell             = NULL;
        n00b_box_props_t *ul_bullet           = NULL;
    }

    set_default_color(lighter_background, primary_background);
    set_default_color(lightest_background, lighter_background);
    set_default_color(darker_background, primary_background);
    set_default_color(darkest_background, darker_background);
    set_default_color(lighter_text, primary_text);
    set_default_color(lightest_text, lighter_text);
    set_default_color(darker_text, primary_text);
    set_default_color(darkest_text, darker_text);
    set_default_color(accent_1_lighter, accent_1);
    set_default_color(accent_2_lighter, accent_2);
    set_default_color(accent_3_lighter, accent_3);
    set_default_color(accent_1_darker, accent_1);
    set_default_color(accent_2_darker, accent_2);
    set_default_color(accent_3_darker, accent_3);

    theme->name                                      = name;
    theme->author                                    = author;
    theme->description                               = description;
    theme->palate[N00B_THEME_PRIMARY_BACKGROUND]     = primary_background;
    theme->palate[N00B_THEME_PRIMARY_TEXT]           = primary_text;
    theme->palate[N00B_THEME_LIGHTER_BACKGROUND]     = lighter_background;
    theme->palate[N00B_THEME_LIGHTEST_BACKGROUND]    = lightest_background;
    theme->palate[N00B_THEME_DARKER_BACKGROUND]      = darker_background;
    theme->palate[N00B_THEME_DARKEST_BACKGROUND]     = darkest_background;
    theme->palate[N00B_THEME_LIGHTER_TEXT]           = lighter_text;
    theme->palate[N00B_THEME_LIGHTEST_TEXT]          = lightest_text;
    theme->palate[N00B_THEME_DARKER_TEXT]            = darker_text;
    theme->palate[N00B_THEME_DARKEST_TEXT]           = darkest_text;
    theme->palate[N00B_THEME_ACCENT_1]               = accent_1;
    theme->palate[N00B_THEME_ACCENT_2]               = accent_2;
    theme->palate[N00B_THEME_ACCENT_3]               = accent_3;
    theme->palate[N00B_THEME_ACCENT_1_LIGHTER]       = accent_1_lighter;
    theme->palate[N00B_THEME_ACCENT_2_LIGHTER]       = accent_2_lighter;
    theme->palate[N00B_THEME_ACCENT_3_LIGHTER]       = accent_3_lighter;
    theme->palate[N00B_THEME_ACCENT_1_DARKER]        = accent_1_darker;
    theme->palate[N00B_THEME_ACCENT_2_DARKER]        = accent_2_darker;
    theme->palate[N00B_THEME_ACCENT_3_DARKER]        = accent_3_darker;
    theme->palate[N00B_THEME_GREY]                   = grey;
    theme->palate[N00B_THEME_PINK]                   = pink;
    theme->palate[N00B_THEME_PURPLE]                 = purple;
    theme->palate[N00B_THEME_GREEN]                  = green;
    theme->palate[N00B_THEME_ORANGE]                 = orange;
    theme->palate[N00B_THEME_RED]                    = red;
    theme->palate[N00B_THEME_YELLOW]                 = yellow;
    theme->palate[N00B_THEME_BLUE]                   = blue;
    theme->palate[N00B_THEME_BROWN]                  = brown;
    theme->palate[N00B_THEME_WHITE]                  = white;
    theme->palate[N00B_THEME_BLACK]                  = black;
    theme->box_info[N00B_BOX_THEME_FLOW]             = flow_props;
    theme->box_info[N00B_BOX_THEME_CALLOUT]          = callout_props;
    theme->box_info[N00B_BOX_THEME_TABLE_BASE]       = base_table_props;
    theme->box_info[N00B_BOX_THEME_ORNATE_BASE]      = ornate_table_props;
    theme->box_info[N00B_BOX_THEME_SIMPLE_BASE]      = simple_table_props;
    theme->box_info[N00B_BOX_THEME_TABLE_CELL_BASE]  = table_cell_props;
    theme->box_info[N00B_BOX_THEME_ORNATE_CELL_BASE] = ornate_cell_props;
    theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_BASE] = simple_cell_props;
    theme->box_info[N00B_BOX_THEME_TABLE_CELL_ALT]   = table_alt_cell;
    theme->box_info[N00B_BOX_THEME_ORNATE_CELL_ALT]  = ornate_alt_cell;
    theme->box_info[N00B_BOX_THEME_SIMPLE_CELL_ALT]  = simple_alt_cell;
    theme->box_info[N00B_BOX_THEME_TITLE]            = table_title;
    theme->box_info[N00B_BOX_THEME_CAPTION]          = table_caption;
    theme->box_info[N00B_BOX_THEME_UL_BASE]          = ul_base;
    theme->box_info[N00B_BOX_UL_CELL_BASE]           = ul_cell;
    theme->box_info[N00B_BOX_UL_BULLET]              = ul_bullet;
    theme->box_info[N00B_BOX_THEME_OL_BASE]          = ol_base;
    theme->box_info[N00B_BOX_OL_CELL_BASE]           = ol_cell;
    theme->box_info[N00B_BOX_OL_BULLET]              = ol_bullet;

    if (name) {
        hatrack_dict_put(n00b_theme_repository, name, theme);
    }
}

static n00b_box_props_t *
pallate_box(int ix)
{
    n00b_text_element_t *ts = n00b_new(n00b_type_text_element(),
                                       n00b_kw("fg_color", (int64_t)ix));
    return n00b_new(n00b_type_box_props(), n00b_kw("text_style", ts));
}

n00b_box_props_t *
n00b_lookup_style_by_tag(n00b_string_t *tag)
{
    n00b_box_props_t *result = hatrack_dict_get(n00b_tag_repository, tag, NULL);

    if (!result && n00b_string_codepoint_len(tag)) {
        switch (tag->data[0]) {
        case 'b':
            if (!strncmp(tag->data, "black", 5)) {
                return pallate_box(N00B_THEME_BLACK);
            }
            if (!strncmp(tag->data, "blue", 4)) {
                return pallate_box(N00B_THEME_BLUE);
            }
            if (!strncmp(tag->data, "brown", 5)) {
                return pallate_box(N00B_THEME_BROWN);
            }
            break;
        case 'g':
            if (!strncmp(tag->data, "gray", 4)) {
                return pallate_box(N00B_THEME_GREY);
            }
            if (!strncmp(tag->data, "green", 5)) {
                return pallate_box(N00B_THEME_GREEN);
            }
            break;
        case 'o':
            if (!strncmp(tag->data, "orange", 6)) {
                return pallate_box(N00B_THEME_ORANGE);
            }
            break;
        case 'p':
            if (!strncmp(tag->data, "pink", 4)) {
                return pallate_box(N00B_THEME_PINK);
            }
            if (!strncmp(tag->data, "purple", 6)) {
                return pallate_box(N00B_THEME_PURPLE);
            }
            break;
        case 'r':
            if (!strncmp(tag->data, "red", 3)) {
                return pallate_box(N00B_THEME_RED);
            }
            break;
        case 'w':
            if (!strncmp(tag->data, "white", 5)) {
                return pallate_box(N00B_THEME_WHITE);
            }
            break;
        case 'y':
            if (!strncmp(tag->data, "yellow", 6)) {
                return pallate_box(N00B_THEME_YELLOW);
            }
            break;
        }
    }

    if (!result) {
        N00B_RAISE(n00b_cformat("Invalid style: [|#|]", tag));
    }

    return result;
}

n00b_box_props_t *
n00b_lookup_style_by_hash(hatrack_hash_t hash)
{
    // Not sure I'm going to use this, but adding it anyway.
    return crown_get(&n00b_tag_repository->crown_instance, hash, NULL);
}

n00b_string_t *
n00b_preview_theme(n00b_theme_t *theme)
{
    n00b_string_t *title = n00b_cstring("Style overview for ");

    title = n00b_string_concat(title, theme->name);

    n00b_table_t *table = n00b_new(n00b_type_table(),
                                   n00b_kw("title", n00b_ka(title)));
    uint64_t      view_len;

    hatrack_dict_item_t *items = hatrack_dict_items_sort(n00b_tag_repository,
                                                         &view_len);

    n00b_table_add_cell(table, n00b_cstring("Tag"));
    n00b_table_add_cell(table, n00b_cstring("Example"));
    n00b_table_end_row(table);
    n00b_string_t *example = n00b_cstring(
        "Lorem ipsum dolor sit amet"
#if 1
        ", consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua."
#else
        "."
#endif
    );

    n00b_table_add_cell(table, n00b_cstring("[Default]"));

    n00b_string_t *s = n00b_string_style(example,
                                         &theme->box_info[N00B_BOX_THEME_FLOW]->text_style);
    n00b_table_add_cell(table, s);
    n00b_table_end_row(table);

    for (uint64_t i = 0; i < view_len; i++) {
        n00b_string_t       *style = items[i].key;
        n00b_box_props_t    *props = items[i].value;
        n00b_text_element_t *txt   = &props->text_style;

        n00b_table_add_cell(table, style);
        s = n00b_string_style(example, txt);
        n00b_table_add_cell(table, s);
        n00b_table_end_row(table);
    }

    return n00b_table_to_string(table);
}

static inline n00b_string_t *
palate_text(n00b_fg_palate_ix_t index)
{
    n00b_box_props_t *props           = n00b_gc_alloc_mapped(n00b_box_props_t,
                                                   N00B_GC_SCAN_ALL);
    props->text_style.bg_palate_index = index;

    return n00b_string_style(n00b_cached_space(), &props->text_style);
}

static inline void
row_if_set(char               *txt,
           n00b_table_t       *table,
           n00b_theme_t       *theme,
           n00b_fg_palate_ix_t index)
{
    if (theme->palate[index] == N00B_NO_COLOR) {
        return;
    }

    n00b_table_add_cell(table, n00b_cstring(txt));
    n00b_table_add_cell(table, palate_text(index));
    n00b_table_end_row(table);
}

static inline void
possible_row(char               *txt,
             n00b_table_t       *table,
             n00b_theme_t       *theme,
             n00b_fg_palate_ix_t index,
             n00b_fg_palate_ix_t anchor)
{
    if (theme->palate[index] == theme->palate[anchor]) {
        return;
    }

    row_if_set(txt, table, theme, index);
}

n00b_string_t *
n00b_theme_palate_table(n00b_theme_t *theme)
{
    n00b_table_t  *table = n00b_new(n00b_type_table());
    n00b_string_t *title = n00b_cstring("Palate for theme ");
    title                = n00b_string_concat(title, theme->name);

    n00b_table_add_cell(table, n00b_cstring("Description"));
    n00b_table_add_cell(table, n00b_cstring("Example"));
    n00b_table_end_row(table);
    row_if_set("Primary Background",
               table,
               theme,
               N00B_THEME_PRIMARY_BACKGROUND);
    possible_row("Lighter Background",
                 table,
                 theme,
                 N00B_THEME_LIGHTER_BACKGROUND,
                 N00B_THEME_PRIMARY_BACKGROUND);
    possible_row("Lightest Background",
                 table,
                 theme,
                 N00B_THEME_LIGHTEST_BACKGROUND,
                 N00B_THEME_LIGHTER_BACKGROUND);
    possible_row("Darker Background",
                 table,
                 theme,
                 N00B_THEME_DARKER_BACKGROUND,
                 N00B_THEME_PRIMARY_BACKGROUND);
    possible_row("Darkest Background",
                 table,
                 theme,
                 N00B_THEME_DARKEST_BACKGROUND,
                 N00B_THEME_DARKER_BACKGROUND);
    row_if_set("Primary Text", table, theme, N00B_THEME_PRIMARY_TEXT);
    possible_row("Lighter Text",
                 table,
                 theme,
                 N00B_THEME_LIGHTER_TEXT,
                 N00B_THEME_PRIMARY_TEXT);
    possible_row("Lightest Text",
                 table,
                 theme,
                 N00B_THEME_LIGHTEST_TEXT,
                 N00B_THEME_LIGHTER_TEXT);
    possible_row("Darker Text",
                 table,
                 theme,
                 N00B_THEME_DARKER_TEXT,
                 N00B_THEME_PRIMARY_TEXT);
    possible_row("Darkest Text",
                 table,
                 theme,
                 N00B_THEME_DARKEST_TEXT,
                 N00B_THEME_DARKER_TEXT);
    row_if_set("Accent 1", table, theme, N00B_THEME_ACCENT_1);
    possible_row("Lighter Accent 1",
                 table,
                 theme,
                 N00B_THEME_ACCENT_1_LIGHTER,
                 N00B_THEME_ACCENT_1);
    possible_row("Darker Accent 1",
                 table,
                 theme,
                 N00B_THEME_ACCENT_1_DARKER,
                 N00B_THEME_ACCENT_1);
    row_if_set("Accent 2", table, theme, N00B_THEME_ACCENT_2);
    possible_row("Lighter Accent 2",
                 table,
                 theme,
                 N00B_THEME_ACCENT_2_LIGHTER,
                 N00B_THEME_ACCENT_2);
    possible_row("Darker Accent 2",
                 table,
                 theme,
                 N00B_THEME_ACCENT_2_DARKER,
                 N00B_THEME_ACCENT_2);
    row_if_set("Accent 3", table, theme, N00B_THEME_ACCENT_3);
    possible_row("Lighter Accent 3",
                 table,
                 theme,
                 N00B_THEME_ACCENT_3_LIGHTER,
                 N00B_THEME_ACCENT_3);
    possible_row("Darker Accent 3",
                 table,
                 theme,
                 N00B_THEME_ACCENT_3_DARKER,
                 N00B_THEME_ACCENT_3);
    row_if_set("grey", table, theme, N00B_THEME_GREY);
    row_if_set("pink", table, theme, N00B_THEME_PINK);
    row_if_set("purple", table, theme, N00B_THEME_PURPLE);
    row_if_set("green", table, theme, N00B_THEME_GREEN);
    row_if_set("orange", table, theme, N00B_THEME_ORANGE);
    row_if_set("red", table, theme, N00B_THEME_RED);
    row_if_set("yellow", table, theme, N00B_THEME_YELLOW);
    row_if_set("blue", table, theme, N00B_THEME_BLUE);
    row_if_set("brown", table, theme, N00B_THEME_BROWN);
    row_if_set("white", table, theme, N00B_THEME_WHITE);
    row_if_set("black", table, theme, N00B_THEME_BLACK);

    return n00b_table_to_string(table);
}

void
n00b_set_current_theme(n00b_theme_t *theme)
{
    n00b_installed_theme = theme;
}

n00b_theme_t *
n00b_lookup_theme(n00b_string_t *name)
{
    return hatrack_dict_get(n00b_theme_repository, name, NULL);
}

n00b_theme_t *
n00b_get_current_theme(void)
{
    return n00b_installed_theme;
}

const n00b_vtable_t n00b_text_element_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_text_element_init,
    },
};

const n00b_vtable_t n00b_box_props_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_box_props_init,
    },
};

const n00b_vtable_t n00b_theme_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_theme_init,
    },
};
