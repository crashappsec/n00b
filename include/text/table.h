#pragma once
#include "n00b.h"

typedef struct n00b_cell_t {
    void             *contents; // Anything that has a render() method.
    n00b_box_props_t *formatting;
    // Used temporarily while rendering to hold horizontally padded
    // lines, before we pad vertically.
    n00b_list_t      *ccache;
    int               wcache;
    // currently we only support column spans.
    int               span;
    // Cached with actual calculated pad.
    int               top_pad;
    int               bottom_pad;
} n00b_cell_t;

typedef enum {
    N00B_TABLE_DEFAULT,
    N00B_TABLE_ORNATE,
    N00B_TABLE_SIMPLE,
    N00B_TABLE_VHEADER,
    N00B_TABLE_UL,
    N00B_TABLE_OL,
    // Flow forces everything into a single column.
    N00B_TABLE_FLOW,
    // Meant for single-cell rendering as a callout.
    N00B_TABLE_CALLOUT,
} n00b_decoration_style_t;

struct n00b_table_t {
    // The theme object is where we get: 1. The dressing style, Used
    // for outer pad, all borders, column widths, titles, captions,
    // etc. This is the "table base", "ornate base", "simple base" or
    // "flow base", depending on the style we set when we initialize
    // the table (allowing us to have consistent theming across
    // multiple types of tables).
    //
    // You can only override this on a per-cell basis.
    n00b_theme_t           *theme;
    // Note that streaming currently only emits a row at a time. We
    // need to always wait for the first row to be able to fix a
    // number of columns and compute column widths.
    //
    // Theoretically, we could stream a cell at a time when column
    // height is limited to 1, but meh. Currently, we're not limiting
    // the column height!
    struct n00b_stream_t   *outstream;
    //
    // The stashed title, if any (printed to span the entire table).
    // If it exists when we
    n00b_string_t          *title;
    // The stashed caption, if any (printed to span the table);
    n00b_string_t          *caption;
    n00b_list_t            *stored_cells;
    n00b_list_t            *current_row;
    // These are for internal use, used to hold the list object we're
    // iterating over in case we need to unlock when throwing an
    // exception.
    n00b_list_t            *locked_row;
    n00b_list_t            *locked_list;
    //
    // Also for internal use, when we call table_render, we buffer up
    // strings as the streamer emits them in this list, and then combine
    // them at the end.
    n00b_list_t            *render_cache;
    // Cache the formatted internal horizontal border, if and only if
    // the border is set.
    n00b_string_t          *hborder_cache;
    // Cache of the vertical bar.
    n00b_string_t          *vborder_cache;
    // Cache the pad and left/right border we'll print per-line or a table.
    n00b_string_t          *left_cache;
    n00b_string_t          *right_cache;
    n00b_string_t          *outer_pad_cache;
    // If this is true, whenever we render something, we clear our
    // internal state. This is always on by default for right now.
    // Eventually when we want to render a table over time (in a
    // curses-like interface) we might want to change that.
    bool                    eject_on_render;
    // We fully lock the table state itself.
    n00b_mutex_t            lock;
    // This indicates the insertion index.
    int                     row_cursor;
    // We don't just track list size due to column spans.
    int                     col_cursor;
    // When streaming and not ejecting...
    int                     next_row_to_output;
    // We allow sparse columns at the back, which we will fill with
    // spaces if necessary. Until we lock in our column count, we keep
    // track of the max # of columns we see.
    int                     current_num_cols;
    // When this is non-zero, we've locked into a number of columns.
    // Anything past that will get truncated automatically.
    int                     max_col;
    // This keeps track of the total width given to us when last asked
    // to render.
    int                     total_width;
    // If the spec for the table doesn't take up all the space we've been
    // given, we need to pad it out more, based on the vertical alignment
    // of the table.
    int                     add_left;
    int                     add_right;
    // Summing up the set column widths once we start rendering.
    int                     column_width_used;
    // If col specs overflow the available width, how much to truncate.
    int                     right_overhang;
    // There's a 'default' style, a 'simple' style and an 'ornate'
    // style.
    //
    // Simple generally involves primarily aligning text, and should
    // generally skip borders.
    n00b_decoration_style_t decoration_style;
    // Cache the border info while rendering.
    n00b_border_t           outer_borders;
    //
    // Column width information.
    // First, what's been set by the user.
    n00b_tree_node_t       *column_specs;
    // Now, what's active for the layout we're currently rendering.
    n00b_tree_node_t       *column_actuals;
    // Current column widths; pulled from column_actuals, but should
    // go away; it's vestigial unported code.
    n00b_list_t            *cur_widths; // int64_t
    bool                    did_setup;
};

extern bool           _n00b_table_add_cell(n00b_table_t *, void *, ...);
extern void           n00b_table_empty_cell(n00b_table_t *);
extern void           n00b_table_add_row(n00b_table_t *, n00b_list_t *);
extern void           n00b_table_end_row(n00b_table_t *);
extern void           n00b_table_end(n00b_table_t *);
extern bool           n00b_table_add_contents(n00b_table_t *, n00b_list_t *);
extern n00b_list_t   *n00b_table_render(n00b_table_t *, int, int);
extern n00b_string_t *n00b_table_to_string(n00b_table_t *);

// These are in utils/table_utils.c
extern n00b_table_t *n00b_tree_format(n00b_tree_node_t *,
                                      void *,
                                      void *,
                                      bool);
extern n00b_table_t *n00b_ordered_list(n00b_list_t *, n00b_string_t *);
extern n00b_table_t *n00b_unordered_list(n00b_list_t *, n00b_string_t *);

#define n00b_table_add_cell(t, c, ...) \
    _n00b_table_add_cell(t, c, N00B_VA(__VA_ARGS__))

#define N00B_COL_SPAN_ALL -1

// In this API, you don't specify a column number. Will probably
// change this, but it's the easiest for getting it working.
#define n00b_table_next_column_default n00b_table_next_column_fit

extern int64_t n00b_table_next_column_fit(n00b_table_t *);
extern int64_t n00b_table_next_column_flex(n00b_table_t *, int64_t);
extern int64_t n00b_table_next_column_set_width_range(n00b_table_t *,
                                                      int64_t,
                                                      int64_t);
extern int64_t n00b_table_next_column_set_width_pct(n00b_table_t *,
                                                    double,
                                                    double);

#if defined(N00B_USE_INTERNAL_API)

#define n00b_table_acquire(t)               \
    {                                       \
        n00b_lock_acquire(&t->lock);        \
        defer(n00b_lock_release(&t->lock)); \
    }

extern void n00b_table_set_column_priority(n00b_table_t *, int64_t, int64_t);

#define N00B_MIN_WIDTH_BEFORE_PADDING 2
#endif

#define n00b_table(...) \
    n00b_new(n00b_type_table(), __VA_ARGS__ __VA_OPT__(, ) NULL)
