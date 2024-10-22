// TODO
// 1. Search.
// 2. Now we're ready to add a more generic `print()`.
// 3. I'd like to do the debug console soon-ish.

// Not soon, but should eventually get done:
// 1. Row spans (column spans are there; row spans only stubbed).
// 2. Style doesn't pick up properly w/ col spans ending exactly on middle.
// 3. Also not soon, but should consider allowing style info to "resolve"
//    as a better way to fix issues w/ split.

#include "n00b.h"

#define SPAN_NONE  0
#define SPAN_HERE  1
#define SPAN_BELOW 2

static n00b_render_style_t *
grid_style(n00b_grid_t *grid)
{
    if (!grid->self->current_style) {
        grid->self->current_style = n00b_lookup_cell_style(
            n00b_new_utf8("table"));
    }

    assert(n00b_in_heap(grid->self->current_style));
    return grid->self->current_style;
}

void
n00b_apply_container_style(n00b_renderable_t *item, n00b_utf8_t *tag)

{
    n00b_render_style_t *tag_style = n00b_lookup_cell_style(tag);
    if (!tag_style || tag_style->base_style == N00B_STY_BAD) {
        return;
    }

    if (item->current_style == NULL) {
        item->current_style = tag_style;
    }
    else {
        item->current_style = n00b_layer_styles(tag_style,
                                               item->current_style);
    }
}

void
n00b_enforce_container_style(n00b_obj_t obj, n00b_utf8_t *tag, bool recurse)
{
    n00b_type_t *t = n00b_get_my_type(obj);
    if (!tag) {
        return;
    }

    switch (t->typeid) {
    case N00B_T_UTF8:
    case N00B_T_UTF32:;
        n00b_str_t *s = (n00b_str_t *)obj;
        n00b_str_apply_style(s, n00b_lookup_text_style(tag), false);
        return;
    case N00B_T_RENDERABLE:;
        n00b_renderable_t *r = (n00b_renderable_t *)obj;
        r->current_style    = n00b_lookup_cell_style(tag);
        n00b_enforce_container_style(r->raw_item, tag, recurse);
        return;
    case N00B_T_GRID:;
        n00b_grid_t *g = (n00b_grid_t *)obj;
        for (int i = 0; i < g->num_rows; i++) {
            for (int j = 0; j < g->num_cols; j++) {
                n00b_renderable_t *r = g->cells[(i * g->num_cols) + j];
                if (recurse) {
                    n00b_enforce_container_style(r, tag, true);
                }
                else {
                    t = n00b_get_my_type(r->raw_item);
                    if (n00b_type_is_string(t)) {
                        n00b_str_apply_style((n00b_str_t *)r->raw_item,
                                            n00b_lookup_text_style(tag),
                                            false);
                    }
                }
            }
        }
        return;
    default:
        n00b_print(t);
        N00B_CRAISE("Bad grid item type.");
    }
}

static inline n00b_utf32_t *
styled_repeat(n00b_codepoint_t c, uint32_t width, n00b_style_t style)
{
    n00b_utf32_t *result = n00b_utf32_repeat(c, width);

    if (n00b_str_codepoint_len(result) != 0) {
        n00b_str_set_style(result, style);
    }

    return result;
}

static inline n00b_utf32_t *
get_styled_pad(uint32_t width, n00b_style_t style)
{
    return styled_repeat(' ', width, style);
}

static n00b_list_t *
pad_lines_vertically(n00b_render_style_t *gs,
                     n00b_list_t         *list,
                     int32_t             height,
                     int32_t             width)
{
    int32_t      len  = n00b_list_len(list);
    int32_t      diff = height - len;
    n00b_utf32_t *pad;
    n00b_list_t  *res;

    if (len == 0) {
        pad = get_styled_pad(width, n00b_style_get_pad_color(gs));
    }
    else {
        pad            = n00b_utf32_repeat(' ', width);
        n00b_utf32_t *l = n00b_to_utf32(n00b_list_get(list, len - 1, NULL));

        pad->styling = l->styling;
    }
    switch (gs->alignment) {
    case N00B_ALIGN_BOTTOM:
        res = n00b_new(n00b_type_list(n00b_type_utf32()),
                      n00b_kw("length", n00b_ka(height)));

        for (int i = 0; i < diff; i++) {
            n00b_list_append(res, pad);
        }
        n00b_list_plus_eq(res, list);
        return res;

    case N00B_ALIGN_MIDDLE:
        res = n00b_new(n00b_type_list(n00b_type_utf32()),
                      n00b_kw("length", n00b_ka(height)));

        for (int i = 0; i < diff / 2; i++) {
            n00b_list_append(res, pad);
        }

        n00b_list_plus_eq(res, list);

        for (int i = 0; i < diff / 2; i++) {
            n00b_list_append(res, pad);
        }

        if (diff % 2 != 0) {
            n00b_list_append(res, pad);
        }

        return res;
    default:
        for (int i = 0; i < diff; i++) {
            n00b_list_append(list, pad);
        }
        return list;
    }
}

static void
renderable_init(n00b_renderable_t *item, va_list args)
{
    n00b_obj_t  *obj              = NULL;
    n00b_utf8_t *tag              = NULL;
    bool        prefer_str_style = true;

    n00b_karg_va_init(args);

    n00b_kw_ptr("obj", obj);
    n00b_kw_ptr("tag", tag);
    n00b_kw_ptr("prefer_str_style", prefer_str_style);

    item->raw_item = obj;

    if (tag != NULL) {
        n00b_apply_container_style(item, tag);

        if (!prefer_str_style) {
            n00b_enforce_container_style(item, tag, false);
        }
    }
}

bool
n00b_install_renderable(n00b_grid_t       *grid,
                       n00b_renderable_t *cell,
                       int               start_row,
                       int               end_row,
                       int               start_col,
                       int               end_col)
{
    int i, j = 0;

    cell->start_col = start_col;
    cell->end_col   = end_col;
    cell->start_row = start_row;
    cell->end_row   = end_row;

    if (start_col < 0 || start_col >= grid->num_cols) {
        return false;
    }
    if (start_row < 0 || start_row >= grid->num_rows) {
        return false;
    }

    for (i = start_row; i < end_row; i++) {
        for (j = start_col; j < end_col; j++) {
            *n00b_cell_address(grid, i, j) = cell;
        }
    }

    if (i < grid->header_rows || j < grid->header_cols) {
        n00b_apply_container_style(cell, n00b_get_th_tag(grid));
    }
    else {
        n00b_apply_container_style(cell, n00b_get_td_tag(grid));
    }

    return true;
}

void
n00b_expand_columns(n00b_grid_t *grid, uint64_t num)
{
    uint16_t           new_cols = grid->num_cols + num;
    size_t             sz       = new_cols * (grid->num_rows + grid->spare_rows);
    n00b_renderable_t **cells    = n00b_gc_array_alloc(n00b_renderable_t *, sz);
    n00b_renderable_t **p        = grid->cells;

    for (int i = 0; i < grid->num_rows; i++) {
        for (int j = 0; j < grid->num_cols; j++) {
            cells[(i * new_cols) + j] = *p++;
        }
    }

    // This needs a lock.
    grid->cells    = cells;
    grid->num_cols = new_cols;
}

void
n00b_grid_expand_rows(n00b_grid_t *grid, uint64_t num)
{
    if (num <= grid->spare_rows) {
        grid->num_rows += num;
        grid->spare_rows -= num;
        return;
    }

    int                old_num  = grid->num_rows * grid->num_cols;
    uint16_t           new_rows = grid->num_rows + num;
    size_t             sz       = grid->num_cols * (new_rows + grid->spare_rows);
    n00b_renderable_t **cells    = n00b_gc_array_alloc(n00b_renderable_t *, sz);
    for (int i = 0; i < old_num; i++) {
        cells[i] = grid->cells[i];
    }

    grid->cells    = cells;
    grid->num_rows = new_rows;
}

void
n00b_grid_add_row(n00b_grid_t *grid, n00b_obj_t container)
{
    if (grid->row_cursor == grid->num_rows) {
        n00b_grid_expand_rows(grid, 1);
    }
    if (grid->col_cursor != 0) {
        grid->row_cursor++;
        grid->col_cursor = 0;
    }

    switch (n00b_base_type(container)) {
    case N00B_T_RENDERABLE:
        n00b_install_renderable(grid,
                               (n00b_renderable_t *)container,
                               grid->row_cursor,
                               grid->row_cursor + 1,
                               0,
                               grid->num_cols);
        grid->row_cursor++;
        return;

    case N00B_T_GRID:
    case N00B_T_UTF8:
    case N00B_T_UTF32: {
        n00b_renderable_t *r = n00b_new(n00b_type_renderable(),
                                      n00b_kw("obj",
                                             n00b_ka(container),
                                             "tag",
                                             n00b_ka(n00b_new_utf8("td"))));
        n00b_install_renderable(grid,
                               r,
                               grid->row_cursor,
                               grid->row_cursor + 1,
                               0,
                               grid->num_cols);
        grid->row_cursor++;
        return;
    }
    case N00B_T_FLIST:;
        flex_view_t *items = flexarray_view((flexarray_t *)container);

        for (int i = 0; i < grid->num_cols; i++) {
            int       err = false;
            n00b_obj_t x   = flexarray_view_next(items, &err);
            if (err || x == NULL) {
                x = (n00b_obj_t)n00b_to_utf32(n00b_empty_string());
            }
            n00b_grid_set_cell_contents(grid, grid->row_cursor, i, x);
        }
        return;
    case N00B_T_LIST:
        for (int i = 0; i < grid->num_cols; i++) {
            n00b_obj_t x = n00b_list_get((n00b_list_t *)container, i, NULL);
            if (x == NULL) {
                x = (n00b_obj_t)n00b_new_utf8(" ");
            }
            n00b_grid_set_cell_contents(grid, grid->row_cursor, i, x);
        }
        return;

    default:
        N00B_CRAISE("Invalid item type for grid.");
    }
}

static void
grid_init(n00b_grid_t *grid, va_list args)
{
    int32_t     start_rows    = 1;
    int32_t     start_cols    = 1;
    int32_t     spare_rows    = 16;
    n00b_list_t *contents      = NULL;
    n00b_utf8_t *container_tag = n00b_new_utf8("table");
    n00b_utf8_t *th_tag        = NULL;
    n00b_utf8_t *td_tag        = NULL;
    int32_t     header_rows   = 0;
    int32_t     header_cols   = 0;
    bool        stripe        = false;

    n00b_karg_va_init(args);
    n00b_kw_int32("start_rows", start_rows);
    n00b_kw_int32("start_cols", start_cols);
    n00b_kw_int32("spare_rows", spare_rows);
    n00b_kw_ptr("contents", contents);
    n00b_kw_ptr("container_tag", container_tag);
    n00b_kw_ptr("th_tag", th_tag);
    n00b_kw_ptr("td_tag", td_tag);
    n00b_kw_int32("header_rows", header_rows);
    n00b_kw_int32("header_cols", header_cols);
    n00b_kw_bool("stripe", stripe);

    if (start_rows < 1) {
        start_rows = 1;
    }

    if (start_cols < 1) {
        start_cols = 1;
    }

    if (spare_rows < 0) {
        spare_rows = 0;
    }

    grid->spare_rows  = (uint16_t)spare_rows;
    grid->width       = N00B_GRID_TERMINAL_DIM;
    grid->height      = N00B_GRID_UNBOUNDED_DIM;
    grid->td_tag_name = td_tag;
    grid->th_tag_name = th_tag;

    if (stripe) {
        grid->stripe = 1;
    }

    if (contents != NULL) {
        // NOTE: ignoring num_rows and num_cols; could throw an
        // exception here.
        n00b_grid_set_all_contents(grid, contents);
    }

    else {
        grid->num_rows   = (uint16_t)start_rows;
        grid->num_cols   = (uint16_t)start_cols;
        size_t num_cells = (start_rows + spare_rows) * start_cols;
        grid->cells      = n00b_gc_array_alloc(n00b_renderable_t *, num_cells);
    }

    if (!n00b_style_exists(container_tag)) {
        container_tag = n00b_new_utf8("table");
    }

    if (!n00b_style_exists(td_tag)) {
        td_tag = n00b_new_utf8("td");
    }

    if (!n00b_style_exists(th_tag)) {
        td_tag = n00b_new_utf8("th");
    }

    n00b_renderable_t *self = n00b_new(n00b_type_renderable(),
                                     n00b_kw("tag",
                                            n00b_ka(container_tag),
                                            "obj",
                                            n00b_ka(grid)));
    grid->self             = self;

    grid->col_props = NULL;
    grid->row_props = NULL;

    grid->header_rows = header_rows;
    grid->header_cols = header_cols;
}

static inline n00b_render_style_t *
get_row_props(n00b_grid_t *grid, int row)
{
    n00b_render_style_t *result;

    if (grid->row_props != NULL) {
        result = hatrack_dict_get(grid->row_props, (void *)(int64_t)row, NULL);
        if (result != NULL) {
            return result;
        }
    }

    if (grid->stripe) {
        if (row % 2) {
            return n00b_lookup_cell_style(n00b_new_utf8("tr.even"));
        }
        else {
            return n00b_lookup_cell_style(n00b_new_utf8("tr.odd"));
        }
    }
    else {
        return n00b_lookup_cell_style(n00b_new_utf8("tr"));
    }
}

static inline n00b_render_style_t *
get_col_props(n00b_grid_t *grid, int col)
{
    n00b_render_style_t *result;

    if (grid->col_props != NULL) {
        result = hatrack_dict_get(grid->col_props, (void *)(int64_t)col, NULL);

        if (result != NULL) {
            return result;
        }
    }

    return n00b_lookup_cell_style(n00b_new_utf8("td"));
}

void
n00b_grid_set_all_contents(n00b_grid_t *g, n00b_list_t *rows)
{
    int64_t  nrows = n00b_list_len(rows);
    uint64_t ncols = 0;

    for (int64_t i = 0; i < nrows; i++) {
        n00b_list_t *row = (n00b_list_t *)(n00b_list_get(rows, i, NULL));

        uint64_t rlen = n00b_list_len(row);

        if (rlen > ncols) {
            ncols = rlen;
        }
    }

    size_t num_cells = (nrows + g->spare_rows) * ncols;
    g->cells         = n00b_gc_array_alloc(n00b_renderable_t *, num_cells);
    g->num_rows      = nrows;
    g->num_cols      = ncols;

    for (int64_t i = 0; i < nrows; i++) {
        n00b_list_t *view    = n00b_list_get(rows, i, NULL);
        uint64_t    viewlen = n00b_list_len(view);

        for (uint64_t j = 0; j < viewlen; j++) {
            n00b_obj_t         item = n00b_list_get(view, j, NULL);
            n00b_renderable_t *cell = n00b_new(n00b_type_renderable(),
                                             n00b_kw("obj", n00b_ka(item)));

            n00b_install_renderable(g, cell, i, i + 1, j, j + 1);
        }
    }
}

void
n00b_grid_add_col_span(n00b_grid_t       *grid,
                      n00b_renderable_t *contents,
                      int64_t           row,
                      int64_t           start_col,
                      int64_t           num_cols)
{
    int64_t end_col;

    if (num_cols == -1) {
        end_col = grid->num_cols;
    }
    else {
        end_col = n00b_min(start_col + num_cols, grid->num_cols);
    }

    if (row >= grid->num_rows || row < 0 || start_col < 0 || (start_col + num_cols) > grid->num_cols) {
        return; // Later, throw an exception.
    }

    n00b_install_renderable(grid, contents, row, row + 1, start_col, end_col);
}

void
n00b_grid_add_row_span(n00b_grid_t       *grid,
                      n00b_renderable_t *contents,
                      int64_t           col,
                      int64_t           start_row,
                      int64_t           num_rows)
{
    int64_t end_row;

    if (num_rows == -1) {
        end_row = grid->num_rows - 1;
    }
    else {
        end_row = n00b_min(start_row + num_rows - 1, grid->num_rows - 1);
    }

    if (col >= grid->num_cols || col < 0 || start_row < 0 || (start_row + num_rows) > grid->num_rows) {
        return; // Later, throw an exception.
    }

    n00b_install_renderable(grid, contents, start_row, end_row, col, col + 1);
}

static inline int16_t
get_column_render_overhead(n00b_grid_t *grid)
{
    n00b_render_style_t *gs     = grid_style(grid);
    int16_t             result = gs->left_pad + gs->right_pad;

    if (gs->borders & N00B_BORDER_LEFT) {
        result += 1;
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        result += 1;
    }

    if (gs->borders & N00B_INTERIOR_VERTICAL) {
        result += grid->num_cols - 1;
    }

    return result;
}

static inline int
column_text_width(n00b_grid_t *grid, int col)
{
    int        max_width = 0;
    n00b_str_t *s;

    for (int i = 0; i < grid->num_rows; i++) {
        n00b_renderable_t *cell = *n00b_cell_address(grid, i, col);

        // Skip any spans except one-cell vertical spans..
        if (!cell || cell->start_row != i || cell->start_col != col || cell->end_col != col + 1) {
            continue;
        }
        if (!cell || !cell->raw_item) {
            continue;
        }
        switch (n00b_base_type(cell->raw_item)) {
        case N00B_T_UTF8:
        case N00B_T_UTF32:
            s = (n00b_str_t *)cell->raw_item;

            n00b_list_t *arr = n00b_str_split(s, n00b_str_newline());
            int         len = n00b_list_len(arr);

            for (int j = 0; j < len; j++) {
                n00b_utf32_t *item = n00b_to_utf32(n00b_list_get(arr, j, NULL));
                if (item == NULL) {
                    break;
                }
                int cur = n00b_str_render_len(item);
                if (cur > max_width) {
                    max_width = cur;
                }
            }
        default:
            continue;
        }
    }
    return max_width;
}
// Here, render width indicates the actual dimensions that rendering
// will produce, knowing that it might be less than or greater than the
// desired width (which we'll handle by padding or truncating).

int16_t *
n00b_calculate_col_widths(n00b_grid_t *grid, int16_t width, int16_t *render_width)
{
    size_t              term_width;
    int16_t            *result = n00b_gc_array_value_alloc(uint16_t,
                                               grid->num_cols);
    int16_t             sum    = get_column_render_overhead(grid);
    n00b_render_style_t *props;

    for (int i = 0; i < grid->num_cols; i++) {
        props = get_col_props(grid, i);
    }
    if (width == N00B_GRID_USE_STORED) {
        width = grid->width;
    }

    if (width == N00B_GRID_TERMINAL_DIM) {
        n00b_terminal_dimensions(&term_width, NULL);
        if (term_width == 0) {
            term_width = 80;
        }
        width = (int16_t)term_width;
    }

    if (width == N00B_GRID_UNBOUNDED_DIM) {
        for (int i = 0; i < grid->num_cols; i++) {
            props = get_col_props(grid, i);

            switch (props->dim_kind) {
            case N00B_DIM_ABSOLUTE:
                assert(i < grid->num_cols);
                result[i] = (uint16_t)props->dims.units;
                sum += result[i];
                break;
            case N00B_DIM_ABSOLUTE_RANGE:
                assert(i < grid->num_cols);
                result[i] = (uint16_t)props->dims.range[1];
                sum += result[i];
                break;
            default:
                N00B_CRAISE("Invalid col spec for unbounded width.");
            }
        }

        *render_width = sum;
        return result;
    }

    // Width is fixed; start by substracting out what we'll need
    // for OUTER padding and for any borders, which is stored in `sum`.
    //
    // `remaining` counts how many bytes of space remaining we actually
    // have to allocate.
    int16_t remaining = width - sum;

    // Pass 1, for anything that has a fixed width, subtract it out of
    // the total remaining. For absolute ranges, use the min value.
    // For percentages, calculate the percentage based on the absolute
    // width.
    //
    // For auto and flex, we see what's left over at the end, but
    // we do count up how many 'flex' units, where 'auto' always
    // gives a flex unit of 1.

    uint64_t flex_units = 0;
    int16_t  num_flex   = 0;
    bool     has_range  = false;
    uint16_t cur;
    float    pct;

    for (int i = 0; i < grid->num_cols; i++) {
        props = get_col_props(grid, i);

        switch (props->dim_kind) {
        case N00B_DIM_ABSOLUTE:
            cur = (uint16_t)props->dims.units;
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case N00B_DIM_ABSOLUTE_RANGE:
            has_range = true;
            cur       = (uint16_t)props->dims.range[0];
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case N00B_DIM_PERCENT_TRUNCATE:
            pct = (props->dims.percent / 100);
            cur = (uint16_t)(pct * width);
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case N00B_DIM_PERCENT_ROUND:
            pct = (props->dims.percent + 0.5) / 100;
            cur = (uint16_t)(pct * width);
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case N00B_DIM_FIT_TO_TEXT:
            cur = column_text_width(grid, i);
            // Assume minimal padding needed.
            cur += 2;
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        case N00B_DIM_UNSET:
        case N00B_DIM_AUTO:
            flex_units += 1;
            num_flex += 1;
            continue;
        case N00B_DIM_FLEX_UNITS:
            flex_units += props->dims.units;

            // We don't count this if it's set to 0.
            if (props->dims.units != 0) {
                num_flex += 1;
            }
            continue;
        }
    }

    // If we have nothing left, we are done.
    if (remaining <= 0) {
        *render_width = sum;
        return result;
    }

    // Second pass only occurs if 'has_range' is true.  If it is, we
    // try to give ranged cells their maximum, up to the remaining width.
    if (has_range) {
        for (int i = 0; i < grid->num_cols; i++) {
            props = get_col_props(grid, i);

            if (props->dim_kind != N00B_DIM_ABSOLUTE_RANGE) {
                continue;
            }
            int32_t desired = props->dims.range[1] - props->dims.range[0];

            if (desired <= 0) {
                continue;
            }
            cur = n00b_min((uint16_t)desired, (uint16_t)remaining);
            sum += cur;
            remaining -= cur;
            assert(i < grid->num_cols);
            result[i] += cur;
            if (remaining == 0) {
                *render_width = sum;
                return result;
            }
        }
    }

    if (!flex_units || remaining == 0) {
        *render_width = sum;
        return result;
    }

    // Third and final pass is the flex pass. Here, we'll give an even
    // amount to each flex unit, rounding down to the character; if
    // the rounding error gives us leftover space, we give it all to
    // the final column (which is why we're tracking the # remaining).
    float flex_width = remaining / (float)flex_units;

    for (int i = 0; i < grid->num_cols; i++) {
        props = get_col_props(grid, i);

        uint64_t units = 1;

        switch (props->dim_kind) {
        case N00B_DIM_FLEX_UNITS:
            units = props->dims.units;
            if (units == 0) {
                continue;
            }
            /* fallthrough */
        case N00B_DIM_UNSET:
        case N00B_DIM_AUTO:
            if (--num_flex == 0) {
                assert(i < grid->num_cols);
                result[i] += remaining;
                sum += remaining;

                *render_width = sum;
                return result;
            }
            cur = (uint16_t)(units * flex_width);
            assert(i < grid->num_cols);
            result[i] = cur;
            sum += cur;
            remaining -= cur;
            continue;
        default:
            continue;
        }
    }

    *render_width = sum;
    return result;
}

// This takes what's passed in about default style info, and preps the
// default style for a single cell.
//
// the row and column defaults are DEFAULTS; anything set in the cell
// will take precedence.
//
// Also note that the actual text might be allowed to override style
// here. By default, it does not, and that overriding doesn't happen
// in this function. This function just decides the default for the cell.
//
// The task is easy if there's only one available thing set. But if we
// have no cell, but we do have both row and column styles, we will
// try to intellegently merge. For instance, we will BLEND any colors
// both specify (in the future we will probably add an layer ordering
// and an alpha value here, but for now we just do 50%).

static inline n00b_utf32_t *
pad_and_style_line(n00b_grid_t       *grid,
                   n00b_renderable_t *cell,
                   int16_t           width,
                   n00b_utf32_t      *line)
{
    n00b_alignment_t align = cell->current_style->alignment & N00B_HORIZONTAL_MASK;
    int64_t         len   = n00b_str_render_len(line);
    uint8_t         lnum  = cell->current_style->left_pad;
    uint8_t         rnum  = cell->current_style->right_pad;
    int64_t         diff  = width - len - lnum - rnum;
    n00b_utf32_t    *lpad;
    n00b_utf32_t    *rpad;

    if (diff > 0) {
        switch (align) {
        case N00B_ALIGN_RIGHT:
            lnum += diff;
            break;
        case N00B_ALIGN_CENTER:
            lnum += (diff / 2);
            rnum += (diff / 2);

            if (diff % 2 == 1) {
                rnum += 1;
            }
            break;
        default:
            rnum += diff;
            break;
        }
    }

    n00b_style_t cell_style = cell->current_style->base_style;
    n00b_style_t lpad_style = n00b_style_get_pad_color(
        cell->current_style);
    n00b_style_t  rpad_style = lpad_style;
    n00b_utf32_t *copy       = n00b_str_copy(line);

    n00b_style_gaps(copy, cell_style);

    // Temporarily force in the cell's background if there is one.
    // Eventually should make this configurable.
    if (cell_style & N00B_STY_BG) {
        n00b_str_layer_style(copy,
                            cell_style & (N00B_STY_BG | N00B_STY_BG_BITS),
                            0);
    }

    int last_style = copy->styling->num_entries - 1;

    lpad_style = copy->styling->styles[0].info;
    rpad_style = copy->styling->styles[last_style].info;

    lpad = get_styled_pad(lnum, lpad_style);
    rpad = get_styled_pad(rnum, rpad_style);

    return n00b_str_concat(n00b_str_concat(lpad, copy), rpad);
}

static inline uint16_t
str_render_cell(n00b_grid_t       *grid,
                n00b_utf32_t      *s,
                n00b_renderable_t *cell,
                int16_t           width,
                int16_t           height)
{
    n00b_render_style_t *col_style = get_col_props(grid,
                                                  cell->start_col);
    n00b_render_style_t *row_style = get_row_props(grid,
                                                  cell->start_row);
    n00b_render_style_t *cs        = n00b_layer_styles(col_style,
                                              row_style);
    n00b_list_t         *res       = n00b_new(
        n00b_type_list(n00b_type_utf32()));

    cs                  = n00b_layer_styles(cs, cell->current_style);
    cell->current_style = cs;

    int               pad      = cs->left_pad + cs->right_pad;
    n00b_utf32_t      *pad_line = pad_and_style_line(grid,
                                               cell,
                                               width,
                                               n00b_empty_string());
    n00b_break_info_t *line_starts;
    n00b_utf32_t      *line;
    int               i;

    for (i = 0; i < cs->top_pad; i++) {
        n00b_list_append(res, pad_line);
    }

    if (cs->disable_wrap) {
        n00b_list_t *arr = n00b_str_split(s, n00b_str_newline());
        bool        err;

        for (i = 0; i < n00b_list_len(arr); i++) {
            n00b_utf32_t *one = n00b_to_utf32(n00b_list_get(arr, i, &err));
            if (one == NULL) {
                break;
            }
            n00b_list_append(res,
                            n00b_str_truncate(one,
                                             width,
                                             n00b_kw("use_render_width",
                                                    n00b_ka(true))));
        }
    }
    else {
        line_starts = n00b_wrap_text(s, width - pad, cs->wrap);
        for (i = 0; i < line_starts->num_breaks - 1; i++) {
            line = n00b_str_slice(s,
                                 line_starts->breaks[i],
                                 line_starts->breaks[i + 1]);
            line = n00b_str_strip(line);
            n00b_list_append(res, pad_and_style_line(grid, cell, width, line));
        }

        if (i == (line_starts->num_breaks - 1)) {
            int b = line_starts->breaks[i];
            line  = n00b_str_slice(s, b, n00b_str_codepoint_len(s));
            // line  = n00b_str_strip(line);
            n00b_list_append(res, pad_and_style_line(grid, cell, width, line));
        }
    }

    for (i = 0; i < cs->bottom_pad; i++) {
        n00b_list_append(res, pad_line);
    }

    cell->render_cache = res;

    return n00b_list_len(res);
}

// Renders to the exact width, and via the height. For now, we're just going
// to handle text objects, and then sub-grids.
static uint16_t
render_to_cache(n00b_grid_t       *grid,
                n00b_renderable_t *cell,
                int16_t           width,
                int16_t           height)
{
    switch (n00b_base_type(cell->raw_item)) {
    case N00B_T_UTF8:
    case N00B_T_UTF32: {
        n00b_str_t *r = (n00b_str_t *)cell->raw_item;

        if (r == NULL || n00b_str_codepoint_len(r) == 0) {
            r = n00b_utf32_repeat(' ', 1);
        }

        if (cell->end_col - cell->start_col != 1) {
            return str_render_cell(grid, n00b_to_utf32(r), cell, width, height);
        }
        else {
            return str_render_cell(grid, n00b_to_utf32(r), cell, width, height);
        }
    }

    case N00B_T_GRID:
        cell->render_cache = n00b_grid_render(cell->raw_item,
                                             n00b_kw("width",
                                                    n00b_ka(width),
                                                    "height",
                                                    n00b_ka(height)));
        return n00b_list_len(cell->render_cache);

    default:
        N00B_CRAISE("Type is not grid-renderable.");
    }

    return 0;
}

static inline void
grid_add_blank_cell(n00b_grid_t *grid,
                    uint16_t    row,
                    uint16_t    col,
                    int16_t     width,
                    int16_t     height)
{
    n00b_utf32_t      *empty = n00b_to_utf32(n00b_empty_string());
    n00b_renderable_t *cell  = n00b_new(n00b_type_renderable(),
                                     n00b_kw("obj",
                                            n00b_ka(empty)));

    n00b_install_renderable(grid, cell, row, row + 1, col, col + 1);
    render_to_cache(grid, cell, width, height);
}

static inline int16_t *
grid_pre_render(n00b_grid_t *grid, int16_t *col_widths)
{
    int16_t            *row_heights = n00b_gc_array_value_alloc(int16_t *,
                                                    grid->num_rows);
    n00b_render_style_t *gs          = grid_style(grid);

    // Run through and tell the individual items to render.
    // For now we tell them all to render to whatever height.
    for (int16_t i = 0; i < grid->num_rows; i++) {
        int16_t row_height = 1;
        int16_t width;
        int16_t cell_height = 0;

        for (int16_t j = 0; j < grid->num_cols; j++) {
            n00b_renderable_t *cell = *n00b_cell_address(grid, i, j);

            if (cell == NULL) {
                grid_add_blank_cell(grid, i, j, col_widths[j], 1);
                cell = *n00b_cell_address(grid, i, j);
            }

            if (cell->start_row != i || cell->start_col != j) {
                continue;
            }

            width = 0;

            for (int16_t k = j; k < cell->end_col; k++) {
                width += col_widths[k];
            }

            // Make sure to account for borders in spans.
            if (gs->borders & N00B_INTERIOR_VERTICAL) {
                width += cell->end_col - j - 1;
            }

            cell->render_width = width;
            cell_height        = render_to_cache(grid, cell, width, -1);

            if (cell_height > row_height) {
                row_height = cell_height;
            }
        }

        row_heights[i] = row_height;

        for (int16_t j = 0; j < grid->num_cols; j++) {
            n00b_renderable_t *cell = *n00b_cell_address(grid, i, j);

            if (cell == NULL) {
                grid_add_blank_cell(grid, i, j, col_widths[j], cell_height);
                continue;
            }

            if (cell->start_row != i || cell->start_col != j) {
                continue;
            }
            // TODO: handle vertical spans properly; this does
            // not.  Right now we're assuming all heights are
            // dynamic to the longest content.
            cell->render_cache = pad_lines_vertically(gs,
                                                      cell->render_cache,
                                                      row_height,
                                                      cell->render_width);
        }
    }
    return row_heights;
}

static inline void
grid_add_top_pad(n00b_grid_t *grid, n00b_list_t *lines, int16_t width)
{
    n00b_render_style_t *gs  = grid_style(grid);
    int                 top = gs->top_pad;

    if (!top) {
        return;
    }

    n00b_utf32_t *pad = get_styled_pad(width,
                                      n00b_style_get_pad_color(gs));

    for (int i = 0; i < top; i++) {
        n00b_list_append(lines, pad);
    }
}

static inline void
grid_add_bottom_pad(n00b_grid_t *grid, n00b_list_t *lines, int16_t width)
{
    n00b_render_style_t *gs     = grid_style(grid);
    int                 bottom = gs->bottom_pad;

    if (!bottom) {
        return;
    }

    n00b_utf32_t *pad = get_styled_pad(width,
                                      n00b_style_get_pad_color(gs));

    for (int i = 0; i < bottom; i++) {
        n00b_list_append(lines, pad);
    }
}

static inline int
find_spans(n00b_grid_t *grid, int row, int col)
{
    int result = SPAN_NONE;

    if (col + 1 == grid->num_rows) {
        return result;
    }

    n00b_renderable_t *cell = *n00b_cell_address(grid, row, col + 1);
    if (cell->start_col != col + 1) {
        result |= SPAN_HERE;
    }

    if (row != grid->num_rows - 1) {
        n00b_renderable_t *cell = *n00b_cell_address(grid, row + 1, col + 1);
        if (cell->start_col != col + 1) {
            result |= SPAN_BELOW;
        }
    }

    return result;
}

static inline void
grid_add_top_border(n00b_grid_t *grid, n00b_list_t *lines, int16_t *col_widths)
{
    n00b_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    n00b_border_theme_t *draw_chars;
    n00b_utf32_t        *s, *lpad, *rpad;
    n00b_codepoint_t    *p;
    n00b_style_t         pad_color;

    if (!(gs->borders & N00B_BORDER_TOP)) {
        return;
    }

    draw_chars = n00b_style_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & N00B_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & N00B_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = n00b_new(n00b_type_utf32(), n00b_kw("length", n00b_ka(border_width)));
    p = (n00b_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & N00B_BORDER_LEFT) {
        *p++ = draw_chars->upper_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
        for (int j = 0; j < col_widths[i]; j++) {
            *p++ = draw_chars->horizontal_rule;
        }

        if (vertical_borders && (i + 1 != grid->num_cols)) {
            if (find_spans(grid, 0, i) == SPAN_HERE) {
                *p++ = draw_chars->horizontal_rule;
            }
            else {
                *p++ = draw_chars->top_t;
            }
        }
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        *p++ = draw_chars->upper_right;
    }

    n00b_str_set_style(s, n00b_str_style(gs));

    pad_color = n00b_style_get_pad_color(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    n00b_list_append(lines, n00b_str_concat(n00b_str_concat(lpad, s), rpad));
}

static inline void
grid_add_bottom_border(n00b_grid_t *grid,
                       n00b_list_t *lines,
                       int16_t    *col_widths)
{
    n00b_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    n00b_border_theme_t *draw_chars;
    n00b_utf32_t        *s, *lpad, *rpad;
    n00b_codepoint_t    *p;
    n00b_style_t         pad_color;

    if (!(gs->borders & N00B_BORDER_BOTTOM)) {
        return;
    }

    draw_chars = n00b_style_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & N00B_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & N00B_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = n00b_new(n00b_type_utf32(), n00b_kw("length", n00b_ka(border_width)));
    p = (n00b_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & N00B_BORDER_LEFT) {
        *p++ = draw_chars->lower_left;
    }

    for (int i = 0; i < grid->num_cols; i++) {
        for (int j = 0; j < col_widths[i]; j++) {
            *p++ = draw_chars->horizontal_rule;
        }

        if (vertical_borders && (i + 1 != grid->num_cols)) {
            if (find_spans(grid, grid->num_rows - 1, i) == SPAN_HERE) {
                *p++ = draw_chars->horizontal_rule;
            }
            else {
                *p++ = draw_chars->bottom_t;
            }
        }
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        *p++ = draw_chars->lower_right;
    }

    n00b_str_set_style(s, n00b_str_style(gs));

    pad_color = n00b_style_get_pad_color(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    n00b_list_append(lines, n00b_str_concat(n00b_str_concat(lpad, s), rpad));
}

static inline void
grid_add_horizontal_rule(n00b_grid_t *grid,
                         int         row,
                         n00b_list_t *lines,
                         int16_t    *col_widths)
{
    n00b_render_style_t *gs           = grid_style(grid);
    int32_t             border_width = 0;
    int                 vertical_borders;
    n00b_border_theme_t *draw_chars;
    n00b_utf32_t        *s, *lpad, *rpad;
    n00b_codepoint_t    *p;
    n00b_style_t         pad_color;

    if (!(gs->borders & N00B_INTERIOR_HORIZONTAL)) {
        return;
    }

    draw_chars = n00b_style_get_border_theme(gs);

    for (int i = 0; i < grid->num_cols; i++) {
        border_width += col_widths[i];
    }

    if (gs->borders & N00B_BORDER_LEFT) {
        border_width++;
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        border_width++;
    }

    vertical_borders = gs->borders & N00B_INTERIOR_VERTICAL;

    if (vertical_borders) {
        border_width += grid->num_cols - 1;
    }

    s = n00b_new(n00b_type_utf32(), n00b_kw("length", n00b_ka(border_width)));
    p = (n00b_codepoint_t *)s->data;

    s->codepoints = border_width;

    if (gs->borders & N00B_BORDER_LEFT) {
        *p++ = draw_chars->left_t;
    }

    for (int i = 0; i < grid->num_cols; i++) {
        for (int j = 0; j < col_widths[i]; j++) {
            *p++ = draw_chars->horizontal_rule;
        }

        if (vertical_borders && (i + 1 != grid->num_cols)) {
            switch (find_spans(grid, row, i)) {
            case SPAN_NONE:
                *p++ = draw_chars->cross;
                break;
            case SPAN_HERE:
                *p++ = draw_chars->top_t;
                break;
            case SPAN_BELOW:
                *p++ = draw_chars->bottom_t;
                break;
            default:
                *p++ = draw_chars->horizontal_rule;
                break;
            }
        }
    }

    if (gs->borders & N00B_BORDER_RIGHT) {
        *p++ = draw_chars->right_t;
    }

    n00b_str_set_style(s, n00b_str_style(gs));

    pad_color = n00b_style_get_pad_color(gs);
    lpad      = get_styled_pad(gs->left_pad, pad_color);
    rpad      = get_styled_pad(gs->right_pad, pad_color);

    n00b_list_append(lines, n00b_str_concat(n00b_str_concat(lpad, s), rpad));
}

static inline n00b_list_t *
grid_add_left_pad(n00b_grid_t *grid, int height)
{
    n00b_render_style_t *gs   = grid_style(grid);
    n00b_list_t         *res  = n00b_new(n00b_type_list(n00b_type_utf32()),
                              n00b_kw("length", n00b_ka(height)));
    n00b_utf32_t        *lpad = n00b_empty_string();

    if (gs->left_pad > 0) {
        lpad = get_styled_pad(gs->left_pad,
                              n00b_style_get_pad_color(gs));
    }

    for (int i = 0; i < height; i++) {
        n00b_list_append(res, lpad);
    }

    return res;
}

static inline void
grid_add_right_pad(n00b_grid_t *grid, n00b_list_t *lines)
{
    n00b_render_style_t *gs = grid_style(grid);

    if (gs->right_pad <= 0) {
        return;
    }

    n00b_utf32_t *rpad = get_styled_pad(gs->right_pad,
                                       n00b_style_get_pad_color(gs));

    for (int i = 0; i < n00b_list_len(lines); i++) {
        n00b_utf32_t *s = n00b_to_utf32(n00b_list_get(lines, i, NULL));
        n00b_list_set(lines, i, n00b_str_concat(s, rpad));
    }
}

static void
add_vertical_bar(n00b_grid_t      *grid,
                 n00b_list_t      *lines,
                 n00b_border_set_t to_match)
{
    n00b_render_style_t *gs = grid_style(grid);
    if (!(gs->borders & to_match)) {
        return;
    }

    n00b_border_theme_t *border_theme = n00b_style_get_border_theme(gs);
    n00b_style_t         border_color = n00b_str_style(gs);
    n00b_utf32_t        *bar;

    bar = styled_repeat(border_theme->vertical_rule, 1, border_color);
    for (int i = 0; i < n00b_list_len(lines); i++) {
        n00b_str_t *n = n00b_list_get(lines, i, NULL);
        if (n == NULL) {
            n = n00b_empty_string();
        }
        n00b_utf32_t *s = n00b_to_utf32(n);
        n00b_list_set(lines, i, n00b_str_concat(s, bar));
    }
}

static inline void
grid_add_left_border(n00b_grid_t *grid, n00b_list_t *lines)
{
    add_vertical_bar(grid, lines, N00B_BORDER_LEFT);
}

static inline void
grid_add_right_border(n00b_grid_t *grid, n00b_list_t *lines)
{
    add_vertical_bar(grid, lines, N00B_BORDER_RIGHT);
}

static inline void
grid_add_vertical_rule(n00b_grid_t *grid, n00b_list_t *lines)
{
    add_vertical_bar(grid, lines, N00B_BORDER_RIGHT);
}

static void
crop_vertically(n00b_grid_t *grid, n00b_list_t *lines, int32_t height)
{
    n00b_render_style_t *gs   = grid_style(grid);
    int32_t             diff = height - n00b_list_len(lines);

    switch (gs->alignment & N00B_VERTICAL_MASK) {
    case N00B_ALIGN_BOTTOM:
        for (int i = 0; i < height; i++) {
            n00b_list_set(lines, i, n00b_list_get(lines, i + diff, NULL));
        }
        break;
    case N00B_ALIGN_MIDDLE:
        for (int i = 0; i < height; i++) {
            n00b_list_set(lines,
                         i,
                         n00b_list_get(lines, i + (diff >> 1), NULL));
        }
        break;
    default:
        break;
    }

    lines->length = height;
}

static inline n00b_utf32_t *
align_and_crop_grid_line(n00b_grid_t *grid, n00b_utf32_t *line, int32_t width)
{
    n00b_render_style_t *gs        = grid_style(grid);
    n00b_alignment_t     align     = gs->alignment;
    n00b_style_t         pad_style = n00b_style_get_pad_color(gs);

    // Called on one grid line if we need to align or crop it.
    int32_t      diff = width - n00b_str_render_len(line);
    n00b_utf32_t *pad;

    if (diff > 0) {
        // We need to pad. Here, we use the alignment info.
        switch (align & N00B_HORIZONTAL_MASK) {
        case N00B_ALIGN_RIGHT:
            pad = get_styled_pad(diff, pad_style);
            return n00b_str_concat(pad, line);
        case N00B_ALIGN_CENTER: {
            pad  = get_styled_pad(diff / 2, pad_style);
            line = n00b_str_concat(pad, line);
            if (diff % 2 != 0) {
                pad = get_styled_pad(1 + diff / 2, pad_style);
            }
            return n00b_str_concat(line, pad);
        }
        default:
            pad = get_styled_pad(diff, pad_style);
            return n00b_str_concat(line, pad);
        }
    }
    else {
        // We need to crop. For now, we ONLY crop from the right.
        return n00b_str_truncate(line,
                                (int64_t)width,
                                n00b_kw("use_render_width",
                                       n00b_ka(1)));
    }
}

static n00b_list_t *
align_and_crop_grid(n00b_grid_t *grid,
                    n00b_list_t *lines,
                    int32_t     width,
                    int32_t     height)
{
    int num_lines = n00b_list_len(lines);

    // For now, width must always be set. Won't be true for height.

    for (int i = 0; i < num_lines; i++) {
        n00b_utf32_t *s = n00b_to_utf32(n00b_list_get(lines, i, NULL));
        if (n00b_str_render_len(s) == width) {
            continue;
        }

        n00b_utf32_t *l = align_and_crop_grid_line(grid, s, width);
        n00b_list_set(lines, i, l);
    }

    if (height != -1) {
        if (num_lines > height) {
            crop_vertically(grid, lines, height);
        }
        else {
            if (num_lines < height) {
                lines = pad_lines_vertically(grid_style(grid),
                                             lines,
                                             height,
                                             width);
            }
        }
    }

    return lines;
}

static inline bool
grid_add_cell_contents(n00b_grid_t *grid,
                       n00b_list_t *lines,
                       uint16_t    r,
                       uint16_t    c,
                       int16_t    *col_widths,
                       int16_t    *row_heights)
{
    // This is the one that fills a single cell.  Returns true if the
    // caller should render vertical interior borders (if wanted). The
    // caller will be on its own in figuring out borders for spans
    // though.

    n00b_renderable_t *cell = *n00b_cell_address(grid, r, c);
    int               i;

    if (cell->end_col - cell->start_col == 1 && cell->end_row - cell->start_row == 1) {
        for (i = 0; i < n00b_list_len(lines); i++) {
            n00b_utf32_t *s = n00b_to_utf32(
                n00b_list_get(lines, i, NULL));
            n00b_utf32_t *piece = n00b_to_utf32(
                n00b_list_get(cell->render_cache,
                             i,
                             NULL));
            if (i < grid->num_cols && !n00b_str_codepoint_len(piece)) {
                n00b_style_t pad_style = n00b_style_get_pad_color(
                    grid_style(grid));
                if (col_widths[i] < 0) {
                    col_widths[i] = 0;
                }

                piece = get_styled_pad(col_widths[i],
                                       pad_style);
            }
            n00b_list_set(lines, i, n00b_str_concat(s, piece));
        }
        return true;
    }

    // For spans, just return the one block of the grid, along with
    // any interior borders.
    uint16_t            row_offset   = r - cell->start_row;
    uint16_t            col_offset   = c - cell->start_col;
    int                 start_width  = 0;
    int                 start_height = 0;
    n00b_render_style_t *gs           = grid_style(grid);

    if (gs->borders & N00B_INTERIOR_VERTICAL) {
        start_width += col_offset;
    }

    if (gs->borders & N00B_INTERIOR_HORIZONTAL) {
        start_height += row_offset;
    }

    for (i = cell->start_col; i < c; i++) {
        start_width += col_widths[i];
    }

    for (i = cell->start_row; i < r; i++) {
        start_height += row_heights[i];
    }

    int num_rows = row_heights[r];
    int num_cols = col_widths[c];

    if ((gs->borders & N00B_INTERIOR_HORIZONTAL) && r + 1 != cell->end_row) {
        num_rows += 1;
    }

    if ((gs->borders & N00B_INTERIOR_VERTICAL) && r + 1 != cell->end_col) {
        num_cols += 1;
    }
    for (i = row_offset; i < row_offset + num_rows; i++) {
        n00b_utf32_t *s     = n00b_to_utf32(n00b_list_get(lines, i, NULL));
        n00b_utf32_t *piece = n00b_to_utf32(n00b_list_get(cell->render_cache,
                                                       i,
                                                       NULL));

        piece             = n00b_str_slice(piece,
                              start_width,
                              start_width + num_cols);
        n00b_utf32_t *line = n00b_str_concat(s, piece);
        n00b_list_set(lines, i, line);
    }

    // This silences a warning... I know I'm not using start_height
    // yet, but the compiler won't shut up about it! So here, I'm
    // using it now, are you happy???
    return ((c + 1) ^ start_height) == ((cell->end_col) ^ start_height);
}

n00b_list_t *
_n00b_grid_render(n00b_grid_t *grid, ...)
{
    // There's a lot of work in here, so I'm keeping the high-level
    // algorithm in this function as simple as possible.  Note that we
    // currently build up one big output string, but I'd also like to
    // have a slight variant that writes to a FILE *.
    //
    // A single streaming implementation doesn't really work, since
    // when writing to a FILE *, we would render the ansi codes as we
    // go.

    int64_t width  = -1;
    int64_t height = -1;

    n00b_karg_only_init(grid);
    n00b_kw_int64("width", width);
    n00b_kw_int64("height", height);

    if (width == -1) {
        width = n00b_terminal_width();
        width = n00b_max(width, 20);
    }

    if (width == 0) {
        return n00b_new(n00b_type_list(n00b_type_utf32()),
                       n00b_kw("length", n00b_ka(0)));
    }

    int16_t *col_widths  = n00b_calculate_col_widths(grid, width, &grid->width);
    int16_t *row_heights = grid_pre_render(grid, col_widths);

    // Right now, we're not going to do the final padding and row
    // heights; we'll just do the padding at the end, and pad all rows
    // out to whatever they render.
    //
    // grid_compute_full_padding(grid, width, height);
    // grid_finalize_row_heights(grid, row_heights, height);

    // Now it's time to output. Each cell will have pre-rendered, even
    // if there are big spans.  So we go through the grid and ask each
    // cell to give us back data, one cell at a time.
    //
    // Span cells know how to return just the contents for the one
    // pard of the grid we're interested in.
    //
    // We also are responsible for outside padding borders here, but
    // we don't draw borders if they would be in the interior of span
    // cells. The function abstractions will do the checking to see if
    // they should do anything.

    n00b_render_style_t *gs      = grid_style(grid);
    uint16_t            h_alloc = grid->num_rows + 1 + gs->top_pad + gs->bottom_pad;

    for (int i = 0; i < grid->num_rows; i++) {
        h_alloc += row_heights[i];
    }

    n00b_list_t *result = n00b_new(n00b_type_list(n00b_type_utf32()),
                                 n00b_kw("length", n00b_ka(h_alloc)));

    grid_add_top_pad(grid, result, width);
    grid_add_top_border(grid, result, col_widths);

    for (int i = 0; i < grid->num_rows; i++) {
        n00b_list_t *row = grid_add_left_pad(grid, row_heights[i]);
        grid_add_left_border(grid, row);

        for (int j = 0; j < grid->num_cols; j++) {
            bool vertical_ok = grid_add_cell_contents(grid,
                                                      row,
                                                      i,
                                                      j,
                                                      col_widths,
                                                      row_heights);

            if (vertical_ok && (j + 1 < grid->num_cols)) {
                grid_add_vertical_rule(grid, row);
            }
        }

        grid_add_right_border(grid, row);
        grid_add_right_pad(grid, row);
        n00b_list_plus_eq(result, row);

        if (i + 1 < grid->num_rows) {
            grid_add_horizontal_rule(grid, i, result, col_widths);
        }
    }

    grid_add_bottom_border(grid, result, col_widths);
    grid_add_bottom_pad(grid, result, width);

    return align_and_crop_grid(grid, result, width, height);
}

n00b_utf32_t *
n00b_grid_to_str(n00b_grid_t *g)
{
    n00b_list_t *l = n00b_grid_render(g);
    // join will force utf32 on the newline.
    return n00b_str_join(l,
                        n00b_str_newline(),
                        n00b_kw("add_trailing", n00b_ka(true)));
}

void
n00b_grid_set_cell_contents(n00b_grid_t *g, int row, int col, n00b_obj_t item)
{
    n00b_renderable_t *cell;

    if (row >= g->num_rows) {
        n00b_grid_expand_rows(g, row - (g->num_rows - 1));
    }

    switch (n00b_base_type(item)) {
    case N00B_T_RENDERABLE:
        cell = (n00b_renderable_t *)item;
        break;
    case N00B_T_GRID: {
        n00b_grid_t *subobj = (n00b_grid_t *)item;
        int         tcells = subobj->num_rows * subobj->num_cols;
        cell               = subobj->self;

        for (int i = 0; i < tcells; i++) {
            n00b_renderable_t *item = subobj->cells[i];
            if (item == NULL) {
                continue;
            }
            n00b_obj_t sub = item->raw_item;

            if (n00b_base_type(sub) == N00B_T_GRID) {
                n00b_render_style_t *sty;

                sty = n00b_layer_styles(((n00b_grid_t *)sub)->self->current_style,
                                       g->self->current_style);

                ((n00b_grid_t *)sub)->self->current_style = sty;
            }
        }

        break;
    }
    case N00B_T_UTF8:
    case N00B_T_UTF32:;
        n00b_utf8_t *tag;
        if (row < g->header_rows || col < g->header_cols) {
            tag = n00b_get_th_tag(g);
        }
        else {
            tag = n00b_get_td_tag(g);
        }

        cell = n00b_new(n00b_type_renderable(),
                       n00b_kw("tag",
                              n00b_ka(tag),
                              "obj",
                              n00b_ka(item)));
        break;

    default:
        N00B_CRAISE("Item passed to grid is not renderable.");
    }

    if (g && g->self && cell) {
        cell->current_style = n00b_layer_styles(g->self->current_style, cell->current_style);
    }
    n00b_install_renderable(g, cell, row, row + 1, col, col + 1);
    if (row >= g->row_cursor) {
        if (col + 1 == g->num_cols) {
            g->row_cursor = row + 1;
            g->col_cursor = 0;
        }
        else {
            g->row_cursor = row;
            g->col_cursor = col + 1;
        }
    }
}

n00b_grid_t *
_n00b_ordered_list(n00b_list_t *items, ...)
{
    n00b_utf8_t *bullet_style = n00b_new_utf8("bullet");
    n00b_utf8_t *item_style   = n00b_new_utf8("li");

    n00b_karg_only_init(items);
    n00b_kw_ptr("bullet_style", bullet_style);
    n00b_kw_ptr("item_style", item_style);

    items = n00b_list_copy(items);

    int64_t      n   = n00b_list_len(items);
    n00b_utf32_t *dot = n00b_utf32_repeat('.', 1);
    n00b_grid_t  *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(n),
                                     "start_cols",
                                     n00b_ka(2),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("ol"))));

    n00b_render_style_t *bp    = n00b_lookup_cell_style(bullet_style);
    float               log   = log10((float)n);
    int                 width = (int)(log + .5) + 1 + 1;

    // Above, one + 1 is because log returns one less than what we
    // need for the int with, and the other is for the period /
    // bullet.

    width += bp->left_pad + bp->right_pad;
    bp->dims.units = width;

    n00b_set_column_props(res, 0, bp);
    n00b_set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
        n00b_utf32_t      *ix     = n00b_to_utf32(n00b_str_from_int(i + 1));
        n00b_utf32_t      *s      = n00b_str_concat(ix, dot);
        n00b_renderable_t *bullet = n00b_to_str_renderable(s, bullet_style);
        n00b_obj_t        *obj    = n00b_list_get(items, i, NULL);
        n00b_type_t       *t      = n00b_get_my_type(obj);
        n00b_renderable_t *li;

        switch (t->typeid) {
        case N00B_T_RENDERABLE:
            li = (n00b_renderable_t *)obj;
            break;
        case N00B_T_UTF8:
        case N00B_T_UTF32:
            li = n00b_new(n00b_type_renderable(),
                         n00b_kw("obj",
                                n00b_to_utf32((n00b_str_t *)obj),
                                "tag",
                                item_style));
            break;
        case N00B_T_GRID:
            li = n00b_new(n00b_type_renderable(),
                         n00b_kw("obj", obj, "tag", item_style));
            break;
        default:
            N00B_CRAISE("Invalid object type for list.");
        }
        n00b_grid_set_cell_contents(res, i, 0, bullet);
        n00b_grid_set_cell_contents(res, i, 1, li);
    }
    return res;
}

n00b_grid_t *
_n00b_unordered_list(n00b_list_t *items, ...)
{
    n00b_utf8_t     *bullet_style = n00b_new_utf8("bullet");
    n00b_utf8_t     *item_style   = n00b_new_utf8("li");
    n00b_codepoint_t bullet_cp    = 0x2022;

    n00b_karg_only_init(items);
    n00b_kw_ptr("bullet_style", bullet_style);
    n00b_kw_ptr("item_style", item_style);
    n00b_kw_codepoint("bullet", bullet_cp);

    items = n00b_list_copy(items);

    int64_t             n        = n00b_list_len(items);
    n00b_grid_t         *res      = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(n),
                                     "start_cols",
                                     n00b_ka(2),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("ul"))));
    n00b_utf32_t        *bull_str = n00b_utf32_repeat(bullet_cp, 1);
    n00b_renderable_t   *bullet   = n00b_to_str_renderable(bull_str,
                                                     bullet_style);
    n00b_render_style_t *bp       = n00b_lookup_cell_style(bullet_style);

    bp->dims.units += bp->left_pad + bp->right_pad;

    n00b_set_column_props(res, 0, bp);
    n00b_set_column_style(res, 1, item_style);

    for (int i = 0; i < n; i++) {
        n00b_obj_t        *obj = n00b_list_get(items, i, NULL);
        n00b_type_t       *t   = n00b_get_my_type(obj);
        n00b_renderable_t *li;

        switch (t->typeid) {
        case N00B_T_RENDERABLE:
            li = (n00b_renderable_t *)obj;
            break;
        case N00B_T_UTF8:
        case N00B_T_UTF32:
            li = n00b_new(n00b_type_renderable(),
                         n00b_kw("obj",
                                n00b_to_utf32((n00b_str_t *)obj),
                                "tag",
                                item_style));
            break;
        case N00B_T_GRID:
            li = n00b_new(n00b_type_renderable(),
                         n00b_kw("obj", obj, "tag", item_style));
            break;
        default:
            N00B_CRAISE("Invalid object type for list.");
        }

        n00b_grid_set_cell_contents(res, i, 0, bullet);
        n00b_grid_set_cell_contents(res, i, 1, li);
    }

    return res;
}

n00b_grid_t *
n00b_grid_flow(uint64_t items, ...)
{
    va_list contents;

    n00b_grid_t *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(items),
                                     "start_cols",
                                     n00b_ka(1),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("flow"))));

    va_start(contents, items);
    for (uint64_t i = 0; i < items; i++) {
        n00b_grid_set_cell_contents(res,
                                   i,
                                   0,
                                   (n00b_obj_t)va_arg(contents, n00b_obj_t));
    }
    va_end(contents);

    return res;
}

n00b_grid_t *
n00b_grid_flow_from_list(n00b_list_t *items)
{
    n00b_grid_t *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(n00b_list_len(items)),
                                     "start_cols",
                                     n00b_ka(1),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("flow"))));
    int         l   = n00b_list_len(items);

    for (int i = 0; i < l; i++) {
        void *item = n00b_list_get(items, i, NULL);
        n00b_grid_set_cell_contents(res, i, 0, item);
    }

    return res;
}

n00b_grid_t *
n00b_callout(n00b_str_t *s)
{
    n00b_renderable_t *r   = n00b_to_str_renderable(s, n00b_new_utf8("callout"));
    n00b_grid_t       *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(1),
                                     "start_cols",
                                     n00b_ka(1),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("callout_cell"))));
    n00b_grid_set_cell_contents(res, 0, 0, r);
    n00b_set_column_style(res, 0, n00b_new_utf8("callout_cell"));

    return res;
}

n00b_grid_t *
n00b_new_cell(n00b_str_t *s, n00b_utf8_t *style)
{
    n00b_renderable_t *r   = n00b_to_str_renderable(s, style);
    n00b_grid_t       *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(1),
                                     "start_cols",
                                     n00b_ka(1),
                                     "container_tag",
                                     n00b_ka(n00b_new_utf8("flow"))));
    n00b_grid_set_cell_contents(res, 0, 0, r);
    n00b_set_column_style(res, 0, n00b_new_utf8("flow"));

    return res;
}

n00b_grid_t *
n00b_grid_horizontal_flow(n00b_list_t *items,
                         uint64_t    max_columns,
                         uint64_t    total_width,
                         n00b_utf8_t *table_style,
                         n00b_utf8_t *cell_style)
{
    uint64_t list_len   = n00b_list_len(items);
    uint64_t start_cols = n00b_min(list_len, max_columns);
    uint64_t start_rows = (list_len + start_cols - 1) / start_cols;

    if (table_style == NULL) {
        table_style = n00b_new_utf8("flow");
    }

    if (cell_style == NULL) {
        cell_style = n00b_new_utf8("td");
    }

    n00b_grid_t *res = n00b_new(n00b_type_grid(),
                              n00b_kw("start_rows",
                                     n00b_ka(start_rows),
                                     "start_cols",
                                     n00b_ka(start_cols),
                                     "container_tag",
                                     n00b_ka(table_style),
                                     "td_tag",
                                     n00b_ka(cell_style)));

    for (uint64_t i = 0; i < list_len; i++) {
        int row = i / start_cols;
        int col = i % start_cols;

        n00b_grid_set_cell_contents(res,
                                   row,
                                   col,
                                   n00b_list_get(items, i, NULL));
    }

    return res;
}
static n00b_renderable_t *
n00b_renderable_copy(n00b_renderable_t *renderable)
{
    return renderable;
}

static n00b_dict_t *
copy_props(n00b_dict_t *old)
{
    uint64_t             n;
    n00b_dict_t          *res  = n00b_dict(n00b_type_int(), n00b_type_ref());
    hatrack_dict_item_t *view = hatrack_dict_items_sort(old, &n);

    for (uint64_t i = 0; i < n; i++) {
        hatrack_dict_item_t item = view[i];

        hatrack_dict_add(res, item.key, item.value);
    }

    return res;
}

static n00b_grid_t *
n00b_grid_copy(n00b_grid_t *orig)
{
    n00b_grid_t *result  = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_rows",
                                        n00b_ka(orig->num_rows),
                                        "start_cols",
                                        n00b_ka(orig->num_cols),
                                        "spare_rows",
                                        n00b_ka(orig->spare_rows),
                                        "header_rows",
                                        n00b_ka(orig->header_rows),
                                        "header_cols",
                                        n00b_ka(orig->header_cols),
                                        "stripe",
                                        n00b_ka(orig->stripe)));
    result->width       = orig->width;
    result->height      = orig->height;
    result->td_tag_name = orig->td_tag_name;
    result->th_tag_name = orig->th_tag_name;
    result->col_props   = copy_props(orig->col_props);
    result->row_props   = copy_props(orig->row_props);

    size_t num_cells = (orig->num_rows + orig->spare_rows) * orig->num_cols;
    result->cells    = n00b_gc_array_alloc(n00b_renderable_t *, num_cells);
    num_cells        = orig->num_rows * orig->num_cols;

    for (unsigned int i = 0; i < num_cells; i++) {
        n00b_renderable_t *r = orig->cells[i];

        if (r) {
            result->cells[i] = n00b_renderable_copy(r);
        }
    }

    result->self = n00b_renderable_copy(orig->self);

    return result;
}

// For instantiating w/o varargs.
n00b_grid_t *
n00b_grid(int32_t     start_rows,
         int32_t     start_cols,
         n00b_utf8_t *table_tag,
         n00b_utf8_t *th_tag,
         n00b_utf8_t *td_tag,
         int         header_rows,
         int         header_cols,
         int         s)
{
    return n00b_new(n00b_type_grid(),
                   n00b_kw("start_rows",
                          n00b_ka(start_rows),
                          "start_cols",
                          n00b_ka(start_cols),
                          "container_tag",
                          n00b_ka(table_tag),
                          "th_tag",
                          n00b_ka(th_tag),
                          "td_tag",
                          n00b_ka(td_tag),
                          "header_rows",
                          n00b_ka(header_rows),
                          "header_cols",
                          n00b_ka(header_cols),
                          "stripe",
                          n00b_ka(s)));
}

typedef struct {
    n00b_utf8_t      *tag;
    n00b_codepoint_t *padstr;
    n00b_grid_t      *grid;
    void            *callback;
    void            *thunk;
    n00b_set_t       *to_collapse;
    n00b_list_t      *state_stack;
    n00b_utf8_t      *nl;
    n00b_codepoint_t  pad;
    n00b_codepoint_t  tchar;
    n00b_codepoint_t  lchar;
    n00b_codepoint_t  hchar;
    n00b_codepoint_t  vchar;
    int              vpad;
    int              ipad;
    int              no_nl;
    n00b_style_t      style;
    int              depth; // Previous depth.
    int64_t          pad_ix;
    int64_t          done_at_this_depth;
    int64_t          total_at_this_depth;
    bool             root;
    bool             cycle;
} tree_fmt_t;

typedef struct {
    n00b_utf8_t     *tag;
    n00b_grid_t     *grid;
    void           *callback;
    void           *thunk;
    n00b_utf8_t     *nl;
    n00b_set_t      *to_collapse;
    n00b_list_t     *depth_positions;
    n00b_list_t     *depth_totals;
    n00b_dict_t     *cycle_nodes;
    n00b_codepoint_t pad;
    n00b_codepoint_t tchar;
    n00b_codepoint_t lchar;
    n00b_codepoint_t hchar;
    n00b_codepoint_t vchar;
    int             vpad;
    int             ipad;
    int             no_nl;
    n00b_style_t     style;
    bool            cycle;
    bool            at_start;
    bool            show_cycles;
} tree_fmt_new_t;

typedef n00b_utf8_t *(*thunk_cb)(void *, void *);
typedef n00b_utf8_t *(*thunkless_cb)(void *);

static n00b_str_t *
build_grid_tree_pad(tree_fmt_new_t *ctx)
{
    int depth = n00b_list_len(ctx->depth_positions);

    if (depth == 1) {
        return n00b_new_utf8("");
    }

    int              num_cps = (depth - 1) * (ctx->vpad + 1) + ctx->ipad;
    n00b_codepoint_t *arr     = alloca(num_cps * sizeof(n00b_codepoint_t));
    n00b_codepoint_t *p       = arr;
    int64_t          pos;
    int64_t          tot;

    // Because we treat the root specially, start this at 1, not 0.
    for (int i = 1; i < depth - 1; i++) {
        pos = (int64_t)n00b_list_get(ctx->depth_positions, i, NULL);
        tot = (int64_t)n00b_list_get(ctx->depth_totals, i, NULL);

        if (pos == tot) {
            *p++ = ctx->pad;
        }
        else {
            *p++ = ctx->vchar;
        }
        for (int j = 0; j < ctx->vpad; j++) {
            *p++ = ctx->pad;
        }
    }

    pos = (int64_t)n00b_list_get(ctx->depth_positions, depth - 1, NULL);
    tot = (int64_t)n00b_list_get(ctx->depth_totals, depth - 1, NULL);

    if (pos == tot) {
        *p++ = ctx->lchar;
    }
    else {
        *p++ = ctx->tchar;
    }

    for (int i = ctx->ipad; i < ctx->vpad; i++) {
        *p++ = ctx->hchar;
    }

    for (int i = 0; i < ctx->ipad; i++) {
        *p++ = ctx->pad;
    }

    n00b_utf32_t *pad = n00b_new(n00b_type_utf32(),
                               n00b_kw("length",
                                      n00b_ka((int64_t)(p - arr)),
                                      "codepoints",
                                      n00b_ka(arr)));
    n00b_str_set_style(pad, ctx->style);

    return pad;
}

static inline void
reset_builder_ctx(tree_fmt_new_t *ctx)
{
    ctx->depth_positions = n00b_list(n00b_type_int());
    ctx->depth_totals    = n00b_list(n00b_type_int());
    ctx->cycle           = false;
    ctx->at_start        = true;
}

static bool
new_tree_builder(n00b_tree_node_t *node, int depth, tree_fmt_new_t *ctx)
{
    int64_t    my_position;
    int64_t    total_in_group;
    n00b_str_t *repr;
    bool       result   = true;
    void      *contents = n00b_tree_get_contents(node);

    if (!ctx->depth_positions) {
        reset_builder_ctx(ctx);
    }

    if (ctx->at_start) {
        my_position    = 1;
        total_in_group = 1;
        ctx->at_start  = false;
    }
    else {
        my_position    = (int64_t)n00b_list_pop(ctx->depth_positions) + 1;
        total_in_group = (int64_t)n00b_list_pop(ctx->depth_totals);
    }

    n00b_list_append(ctx->depth_positions, (void *)my_position);
    n00b_list_append(ctx->depth_totals, (void *)total_in_group);

    if (ctx->to_collapse && n00b_set_contains(ctx->to_collapse, node)) {
        result = false;
    }

    if (ctx->thunk) {
        repr = (*(thunk_cb)ctx->callback)(contents, ctx->thunk);
    }

    else {
        repr = (*(thunkless_cb)ctx->callback)(contents);
    }

    if (!repr) {
        // Note that we can still descend, we'll just hide the node.
        goto on_exit;
    }

    if (ctx->cycle) {
        // repr = n00b_str_concat(repr, n00b_cstr_format("[h6] (CYCLES)[/] "));
        repr = n00b_cstr_format("{} [h6](CYCLE #{})[/] ",
                               repr,
                               hatrack_dict_get(ctx->cycle_nodes, node, NULL));
    }

    if (ctx->no_nl) {
        int64_t ix = n00b_str_find(repr, ctx->nl);

        if (ix != -1) {
            repr = n00b_str_slice(repr, 0, ix);
            repr = n00b_str_concat(repr, n00b_utf32_repeat(0x2026, 1));
        }
    }
    else {
        repr = n00b_utf32_repeat(0x2026, 1);
    }

    repr = n00b_str_concat(build_grid_tree_pad(ctx), repr);

    n00b_style_gaps(repr, ctx->style);
    n00b_renderable_t *item = n00b_to_str_renderable(repr, ctx->tag);
    n00b_grid_add_row(ctx->grid, item);

on_exit:

    if (my_position == total_in_group) {
        if (ctx->cycle || !node->num_kids) {
            // If we call no children and are last at our level,
            // we have to clean up the stack, removing all levels that are
            // 'done'.

            int n = n00b_list_len(ctx->depth_positions);

            while (n--) {
                my_position    = (int64_t)n00b_list_pop(ctx->depth_positions);
                total_in_group = (int64_t)n00b_list_pop(ctx->depth_totals);
                if (my_position != total_in_group) {
                    n00b_list_append(ctx->depth_positions, (void *)my_position);
                    n00b_list_append(ctx->depth_totals, (void *)total_in_group);
                    break;
                }
            }
        }
    }

    if (node->num_kids && !ctx->cycle) {
        n00b_list_append(ctx->depth_positions, (void *)0ULL);
        n00b_list_append(ctx->depth_totals, (void *)(int64_t)node->num_kids);
    }

    return result;
}

static bool
grid_cycle_callback(n00b_tree_node_t *node, int depth, tree_fmt_new_t *ctx)
{
    if (ctx->show_cycles) {
        return false;
    }

    if (!ctx->cycle_nodes) {
        ctx->cycle_nodes = n00b_dict(n00b_type_ref(), n00b_type_int());
    }

    if (!hatrack_dict_get(ctx->cycle_nodes, node, NULL)) {
        hatrack_dict_put(ctx->cycle_nodes,
                         node,
                         (void *)hatrack_dict_len(ctx->cycle_nodes) + 1);
    }

    ctx->cycle = true;
    new_tree_builder(node, depth, ctx);
    ctx->cycle = false;

    return false;
}

static void
build_tree_output(n00b_tree_node_t *node, tree_fmt_t *info, bool last)
{
    if (node == NULL) {
        return;
    }

    n00b_str_t       *line = n00b_tree_get_contents(node);
    int              i;
    n00b_codepoint_t *prev_pad = info->padstr;
    int              last_len = info->pad_ix;

    if (line != NULL) {
        if (info->no_nl) {
            int64_t ix = n00b_str_find(line, info->nl);

            if (ix != -1) {
                line = n00b_str_slice(line, 0, ix);
                line = n00b_str_concat(line,
                                      n00b_utf32_repeat(0x2026, 1));
            }
        }
    }
    else {
        line = n00b_utf32_repeat(0x2026, 1);
    }

    if (!info->root) {
        info->pad_ix += info->vpad + info->ipad + 1;
        info->padstr = n00b_gc_array_value_alloc(n00b_codepoint_t,
                                                info->pad_ix);
        for (i = 0; i < last_len; i++) {
            if (prev_pad[i] == info->tchar || prev_pad[i] == info->vchar) {
                info->padstr[i] = info->vchar;
            }
            else {
                if (prev_pad[i] == info->lchar) {
                    info->padstr[i] = info->vchar;
                }
                else {
                    info->padstr[i] = ' ';
                }
            }
        }

        if (last) {
            info->padstr[i++] = info->lchar;
        }
        else {
            info->padstr[i++] = info->tchar;
        }

        for (int j = 0; j < info->vpad; j++) {
            info->padstr[i++] = info->hchar;
        }

        for (int j = 0; j < info->ipad; j++) {
            info->padstr[i++] = ' ';
        }

        n00b_utf32_t *pad = n00b_new(n00b_type_utf32(),
                                   n00b_kw("length",
                                          n00b_ka(i),
                                          "codepoints",
                                          n00b_ka(info->padstr)));
        n00b_str_set_style(pad, info->style);
        line = n00b_str_concat(pad, line);
    }
    else {
        info->root = false;
    }

    n00b_style_gaps(line, info->style);
    n00b_renderable_t *item = n00b_to_str_renderable(line, info->tag);

    n00b_grid_add_row(info->grid, item);

    int64_t num_kids = n00b_tree_get_number_children(node);

    if (last) {
        assert(info->padstr[last_len] == info->lchar);
        info->padstr[last_len] = 'x';
    }

    int              my_pad_ix = info->pad_ix;
    n00b_codepoint_t *my_pad    = info->padstr;

    for (i = 0; i < num_kids; i++) {
        if (i + 1 == num_kids) {
            build_tree_output(n00b_tree_get_child(node, i), info, true);
        }
        else {
            build_tree_output(n00b_tree_get_child(node, i), info, false);
            info->pad_ix = my_pad_ix;
            info->padstr = my_pad;
        }
    }
}

void
n00b_set_column_props(n00b_grid_t *grid, int col, n00b_render_style_t *s)
{
    if (grid->col_props == NULL) {
        grid->col_props = n00b_dict(n00b_type_int(), n00b_type_ref());
    }

    hatrack_dict_put(grid->col_props, (void *)(int64_t)col, s);
}

void
n00b_set_row_props(n00b_grid_t *grid, int row, n00b_render_style_t *s)
{
    if (grid->row_props == NULL) {
        grid->row_props = n00b_dict(n00b_type_int(), n00b_type_ref());
    }

    hatrack_dict_put(grid->row_props, (void *)(int64_t)row, s);
}

void
n00b_set_column_style(n00b_grid_t *grid, int col, n00b_utf8_t *tag)
{
    n00b_render_style_t *style = n00b_lookup_cell_style(tag);

    if (!style) {
        N00B_CRAISE("Style not found.");
    }

    n00b_set_column_props(grid, col, style);
}

void
n00b_set_row_style(n00b_grid_t *grid, int row, n00b_utf8_t *tag)
{
    n00b_render_style_t *style = n00b_lookup_cell_style(tag);

    if (!style) {
        N00B_CRAISE("Style not found.");
    }

    n00b_set_row_props(grid, row, style);
}

n00b_grid_t *
_n00b_grid_tree_new(n00b_tree_node_t *tree, ...)
{
    n00b_codepoint_t pad      = ' ';
    n00b_codepoint_t tchar    = 0x251c;
    n00b_codepoint_t lchar    = 0x2514;
    n00b_codepoint_t hchar    = 0x2500;
    n00b_codepoint_t vchar    = 0x2502;
    int32_t         vpad     = 2;
    int32_t         ipad     = 1;
    bool            no_nl    = true;
    n00b_utf8_t     *tag      = n00b_new_utf8("tree_item");
    void           *callback = NULL;
    void           *thunk    = NULL;

    n00b_karg_only_init(tree);
    n00b_kw_codepoint("pad", pad);
    n00b_kw_codepoint("t_char", tchar);
    n00b_kw_codepoint("l_char", lchar);
    n00b_kw_codepoint("h_char", hchar);
    n00b_kw_codepoint("v_char", vchar);
    n00b_kw_int32("vpad", vpad);
    n00b_kw_int32("ipad", ipad);
    n00b_kw_bool("truncate_at_newline", no_nl);
    n00b_kw_ptr("style_tag", tag);
    n00b_kw_ptr("callback", callback);
    n00b_kw_ptr("user_data", thunk);

    if (callback == NULL) {
        N00B_CRAISE(
            "Must provide a callback for providing a "
            "representation.");
    }

    if (vpad < 1) {
        vpad = 1;
    }
    if (ipad < 0) {
        ipad = 1;
    }

    n00b_grid_t *result = n00b_new(n00b_type_grid(),
                                 n00b_kw("container_tag",
                                        n00b_ka(n00b_new_utf8("flow")),
                                        "td_tag",
                                        n00b_ka(tag)));

    tree_fmt_new_t fmt_info = {
        .pad      = pad,
        .tchar    = tchar,
        .lchar    = lchar,
        .hchar    = hchar,
        .vchar    = vchar,
        .vpad     = vpad,
        .ipad     = ipad,
        .no_nl    = no_nl,
        .style    = n00b_str_style(n00b_lookup_cell_style(tag)),
        .tag      = tag,
        .grid     = result,
        .nl       = n00b_utf8_repeat('\n', 1),
        .callback = callback,
        .thunk    = thunk,
    };

    n00b_tree_walk_with_cycles(tree,
                              (n00b_walker_fn)new_tree_builder,
                              (n00b_walker_fn)grid_cycle_callback,
                              &fmt_info);

    fmt_info.show_cycles = true;

    if (fmt_info.cycle_nodes != NULL) {
        uint64_t          n;
        n00b_tree_node_t **nodes = (void *)hatrack_dict_keys_sort(
            fmt_info.cycle_nodes,
            &n);
        fmt_info.cycle_nodes = NULL;

        for (uint64_t i = 0; i < n; i++) {
            reset_builder_ctx(&fmt_info);

            n00b_str_t *s = n00b_cstr_format("[h1]Rooted from Cycle {}: ", i + 1);
            n00b_style_gaps(s, fmt_info.style);

            n00b_renderable_t *item = n00b_to_str_renderable(s, fmt_info.tag);
            n00b_grid_add_row(fmt_info.grid, item);
            n00b_tree_walk_with_cycles(nodes[i],
                                      (n00b_walker_fn)new_tree_builder,
                                      NULL,
                                      &fmt_info);
        }
    }

    return result;
}

// This currently expects a tree[utf8] or tree[utf32].  Eventually
// maybe would make it handle anything via it's repr.  However, it
// should also be restructured to be a single renderable item itself,
// so that it can be responsive when we want to add items, once we get
// more GUI-oriented.
//
// This is the quick-and-dirty implementation to replace the trees
// I currently have in NIM for n00b debugging, etc.

n00b_grid_t *
_n00b_grid_tree(n00b_tree_node_t *tree, ...)
{
    n00b_codepoint_t pad       = ' ';
    n00b_codepoint_t tchar     = 0x251c;
    n00b_codepoint_t lchar     = 0x2514;
    n00b_codepoint_t hchar     = 0x2500;
    n00b_codepoint_t vchar     = 0x2502;
    int32_t         vpad      = 2;
    int32_t         ipad      = 1;
    bool            no_nl     = true;
    n00b_utf8_t     *tag       = n00b_new_utf8("tree_item");
    void           *converter = NULL;

    n00b_karg_only_init(tree);
    n00b_kw_codepoint("pad", pad);
    n00b_kw_codepoint("t_char", tchar);
    n00b_kw_codepoint("l_char", lchar);
    n00b_kw_codepoint("h_char", hchar);
    n00b_kw_codepoint("v_char", vchar);
    n00b_kw_int32("vpad", vpad);
    n00b_kw_int32("ipad", ipad);
    n00b_kw_bool("truncate_at_newline", no_nl);
    n00b_kw_ptr("style_tag", tag);
    n00b_kw_ptr("converter", converter);

    if (converter != NULL) {
        tree = n00b_tree_str_transform(tree,
                                      (n00b_str_t * (*)(void *)) converter);
    }

    if (vpad < 1) {
        vpad = 1;
    }
    if (ipad < 0) {
        ipad = 1;
    }

    n00b_grid_t *result = n00b_new(n00b_type_grid(),
                                 n00b_kw("container_tag",
                                        n00b_ka(n00b_new_utf8("flow")),
                                        "td_tag",
                                        n00b_ka(tag)));

    tree_fmt_t fmt_info = {
        .pad    = pad,
        .tchar  = tchar,
        .lchar  = lchar,
        .hchar  = hchar,
        .vchar  = vchar,
        .vpad   = vpad,
        .ipad   = ipad,
        .no_nl  = no_nl,
        .style  = n00b_str_style(n00b_lookup_cell_style(tag)),
        .tag    = tag,
        .pad_ix = 0,
        .grid   = result,
        .nl     = n00b_utf8_repeat('\n', 1),
        .root   = true,
    };

    build_tree_output(tree, &fmt_info, false);

    return result;
}

void
n00b_grid_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_grid_t *grid = (n00b_grid_t *)alloc;
    n00b_mark_raw_to_addr(bitfield, alloc, &grid->th_tag_name);
}

void
n00b_renderable_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_renderable_t *r = (n00b_renderable_t *)alloc;
    n00b_mark_raw_to_addr(bitfield, alloc, &r->raw_item);
}

static n00b_grid_t *
n00b_to_grid_lit(n00b_type_t *objtype, n00b_list_t *items, n00b_utf8_t *litmod)
{
    if (!strcmp(litmod->data, "table")) {
        int nrows = n00b_list_len(items);
        int ncols = 0;

        if (!nrows) {
            return n00b_new(n00b_type_grid(),
                           n00b_kw("start_rows",
                                  n00b_ka(0),
                                  "start_cols",
                                  n00b_ka(0)));
        }

        n00b_type_t *t = n00b_get_my_type(n00b_list_get(items, 0, NULL));
        if (n00b_types_are_compat(t, n00b_type_utf8(), NULL)) {
            N00B_CRAISE("Not implemented yet.");
        }
        if (!n00b_types_are_compat(t,
                                  n00b_type_list(n00b_type_utf8()),
                                  NULL)) {
            N00B_CRAISE("Currently only strings are supported in tables.");
        }
        for (int i = 0; i < nrows; i++) {
            n00b_list_t *l   = n00b_list_get(items, i, NULL);
            int         len = n00b_list_len(l);

            if (len > ncols) {
                ncols = len;
            }
        }

        n00b_grid_t *result = n00b_new(n00b_type_grid(),
                                     n00b_kw("start_rows",
                                            n00b_ka(nrows),
                                            "start_cols",
                                            n00b_ka(ncols),
                                            "header_rows",
                                            n00b_ka(1),
                                            "stripe",
                                            n00b_ka(true)));

        for (int i = 0; i < nrows; i++) {
            n00b_list_t *l = n00b_list_get(items, i, NULL);
            n00b_grid_add_row(result, l);
        }

        return result;
    }

    N00B_CRAISE("Not implemented yet.");
}

const n00b_vtable_t n00b_grid_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]   = (n00b_vtable_entry)grid_init,
        [N00B_BI_TO_STR]        = (n00b_vtable_entry)n00b_grid_to_str,
        [N00B_BI_GC_MAP]        = (n00b_vtable_entry)n00b_grid_set_gc_bits,
        [N00B_BI_CONTAINER_LIT] = (n00b_vtable_entry)n00b_to_grid_lit,
        [N00B_BI_COPY]          = (n00b_vtable_entry)n00b_grid_copy,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `n00b_stream_t` on my mac).
        [N00B_BI_FINALIZER]     = NULL,
    },
};

const n00b_vtable_t n00b_renderable_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)renderable_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)n00b_renderable_set_gc_bits,
        [N00B_BI_COPY]        = (n00b_vtable_entry)n00b_renderable_copy,
        [N00B_BI_FINALIZER]   = NULL,
    },
};
