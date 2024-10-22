#pragma once

#include "n00b.h"

/** Grid Layout objects.

 * For our initial implementation, we are going to keep it as simple
 * as possible, and then go from there. Each grid is a collection of n
 * x m cells that form a rectangle.
 *
 * The individual rows and columns in a single grid must all be of
 * uniform size.
 *
 * Each cell can contain:
 *
 * 1) Another grid.
 * 2) Simple text.
 * 3) More later? (notcurses content?)
 *
 * Grids contained inside other grids can span cells, but they must
 * be rectangular (currently only col spans work actually);
 *
 * An individual grid has the following properties:
 *
 * - Rows and columns.
 * - Widths (per column) -- see below
 * - Heights (per row)
 * - L/R and top/bottom alignment of contents.
 * - outer pad (left, right, top, bottom) -- a measure of how much space to
 *   put around the outside of the grid.
 * - inner pad -- the *default* padding for any interior cells. This is applied
 *   automatically to text, and is applied to inner grids if they do not
 *   specify an explicit outer pad.
 * - Whether borders should be drawn outside the box.
 * - Whether interior lines that don't go through a sub-grid should be used.
 *   If not, they are not counted when making room for cells.
 * - The border style.
 * - FG and BG color for borders.
 * - Default FG and BG for contents (can be overridden by cells).
 * - Default alignment for inner contents.
 * - Some kind of "hide-if-not-fully-renderable" property.
 *
 * All widths and hights can be specified with:
 *
 * a) % of total render width. (like % in html)
 * b) Absolute number of characters (like px in html)
 * c) Flex-units (like fr in CSS Grid layout)
 * d) Auto (the system divides space between all auto-items evenly).
 *    for heights, the default for rows is to give an arbitrary amount
 *    of space for a row to render, but then will ask each cell to
 *    render to that one height.
 * e) Semi-automatic-- you can specify a minimum, maximum or both,
 *    _in absolute sizes only_.
 *
 * Note that all absolute widths are measured based on `characters`;
 * this assumes we are using a terminal with a fixed width font, where
 * the character is essentially an indivisible unit.
 *
 * When we consider the overall grid dimensions, we mean the amount of
 * space we have available for rendering the grid. Most commonly, if
 * we call 'print' on the grid, we generally will want to render with
 * a width equal to the current terminal width, and an unbounded
 * length (relying on the terminal or a pager to scroll up / down).
 *
 * However, we could also render into a plane with a scroll bar around
 * it, in which case we might not show the whole table at once.
 *
 * Still, the overall table dimensions are used when computing the
 * dimensions or its cells. Currently, each column and each row may be
 * independently sized, but not individual cells (e.g., you cannot
 * currently do a 'mason' layout (a la pintrest).
 *
 * Given the constraints provided for rows or columns, it can
 * certainly be the case where there is not enough available table
 * space, and rows / columns may be assigned absolute widths of 0, in
 * which case they will not be rendered.
 *
 * The grid is set up to facilitate re-rendering. Each item in the
 * grid is expected to cache a rendering in its current dimensions;
 * when the grid is asked to render, it asks each item that will get
 * rendered to hand it a pre-rendering, which will usually be cached.
 *
 * If a grid's size has changed (or if any other property that affects
 * rendering has changed), it will send a re-render event to cells
 * (not implemented yet).
 *
 * Items can, of course, change what they contain in between grid
 * renderings.
 *
 * When the grid asks cells for their rendering, the expectations are:
 *
 * 1) The item provides an array of `n00b_str_t *` objects, one for each
 *    row of the requested height (with no newline type characters).
 *
 * 2) Each item in that array is padded (generally with spaces, but
 *    with whatever 'pad' is set) to the requested width.
 *
 * 3) All alignment and coloring rules are respected in what is passed
 *    back. That means the n00b_str_t *'s styling info will be set
 *    appropriately, and that padd will be added based on any
 *    alignment properties.
 *
 * 4) To be clear, the returned array of n00b_str_t's returned will contain
 *    the number of rows asked for, even if some rows are 100% pad.
 *
 * Of course, for any property beyond render dimensions, the cell's
 * contents can override.
 *
 * Currently, when the grid asks cells to render, the renderbox merges
 * styles.
 *
 * This abstraction is built to supporting dynamic re-rendering, for
 * instance, as a window is resized.
 *
 * Currently, the render box's cache is expected to only be updated
 * when rendering occurs, though. The state accessed in doing the
 * rendering should be built in a way that it can be updated from
 * multiple threads if useful, but if so, that should be done in a way
 * so as not to interfere with the rendering.
 */

typedef struct n00b_grid_t n00b_grid_t;

// The outer grid draws borders assembling cells, and does not draw
// borders if they would go THROUGH a cell; that is left to the
// contained cell, if it's done at all.
//
// However, if per-row/col colors have been set, then the current row
// color will override interior vertical lines, and the current column
// color will override interior horizontal lines by default. This
// allows one to shade header rows or columns / etc.
//
// For the border color, we use a style_t, where we only use the
// color bits from style.h: FG_COLOR_ON and BG_COLOR_ON get used, and
// the bottom 48 bits are color.
//
// When used in the context of a column color, we might add a bit
// specific to this type to *not* to the above override, but for now
// it always happens if the row/column provides color info.
//
// The text_style_override field is used to indicate what a cell is
// ALLOWED to override. The bitfield is the only thing checked, and
// anything bit set is allowed to be overridden. By default, colors
// are disallowed, but no other text styling.
//
// This is what can be set on a per-row or per-column basis. Indivudal
// cells can either get their own, or inherit from an outer container.
// The exception is 'text_style_override' gets merged with any style.
// Note that when applying these across a table, they copy data in
// if it has
//
// 'Wrap' may seem like it applies to text; but wrap requires the
// notion of a container, just like alignment does. Here, the value
// should be -2 for 'inherit' (i.e., no specific override) -1 for NO
// wrap, meaning the text will be asked to be rendered for whatever
// the maximum width of a line is, and then will be CROPPED to the
// render width.
//
// Otherwise, the value is interpreted as how much 'hang' to leave
// on subsequent lines.

typedef struct {
    char               *container_tag;
    n00b_render_style_t *current_style;
    n00b_list_t         *render_cache;
    n00b_obj_t           raw_item; // Currently, a n00b_grid_t * or n00b_str_t *.
    int64_t             start_col;
    int64_t             start_row;
    int64_t             end_col;
    int64_t             end_row;
    uint64_t            render_width;
    uint64_t            render_height;
} n00b_renderable_t;

struct n00b_grid_t {
    n00b_renderable_t  *self;
    n00b_renderable_t **cells;     // A 2d array of renderable_objects, by ref
    n00b_dict_t        *col_props; // dict of int:n00b_render_style_t **
    n00b_dict_t        *row_props;
    // Per-render info, which includes any adding added to perform
    // alignment of the grid within the dimensions we're given.
    // Negative widths are possible and will cause us to crop to the
    // dimensions of the drawing space.
    n00b_utf8_t        *td_tag_name;
    n00b_utf8_t        *th_tag_name;
    int64_t            num_cols;
    int64_t            num_rows;
    uint64_t           spare_rows;
    int16_t            width;  // In chars.
    int16_t            height; // In chars.
    uint16_t           row_cursor;
    uint16_t           col_cursor;
    // When we add renderables, if we have no explicit tag for them,
    // we will apply the 'th' tag to anything in these row/columns.
    int8_t             header_cols;
    int8_t             header_rows;
    int8_t             stripe;
    int8_t             text_bg_priority;
    int8_t             text_fg_priority;
};

#define N00B_GRID_TERMINAL_DIM  ((int64_t) - 1)
#define N00B_GRID_UNBOUNDED_DIM ((int64_t) - 2)
#define N00B_GRID_USE_STORED    ((int64_t) - 3)
