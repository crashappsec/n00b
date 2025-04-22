// Intended tot replace the old grids. Will probably rename back to
// grid once working.
#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void setup_table_rendering(n00b_table_t *table, int width);
static void emit_cache(n00b_table_t *table, bool add_end);

#define pad_func(direction)                                        \
    static inline int get_##direction##_pad(n00b_box_props_t *box) \
    {                                                              \
        if (box->direction##_pad < 0) {                            \
            return N00B_DEFAULT_TABLE_PAD;                         \
        }                                                          \
        return box->direction##_pad;                               \
    }

pad_func(top);
pad_func(bottom);
pad_func(left);
pad_func(right);

int64_t
n00b_table_next_column_fit(n00b_table_t *table)
{
    // This is the default. Here, we set a 'preferred' width based on
    // the longest line length we see in the content.
    //
    // If there is no content with a specific width at the time we do
    // layout, then we automatically convert the column to a 'flex'
    // column with a factor of 1.

    n00b_new_layout_cell(table->column_specs);
    return n00b_tree_get_number_children(table->column_specs);
}

int64_t
n00b_table_next_column_flex(n00b_table_t *table, int64_t factor)
{
    n00b_new_layout_cell(table->column_specs, n00b_kw("flex", factor));

    return n00b_tree_get_number_children(table->column_specs);
}

int64_t
n00b_table_next_column_set_width_range(n00b_table_t *table,
                                       int64_t       min,
                                       int64_t       max)
{
    n00b_new_layout_cell(table->column_specs,
                         n00b_kw("min", min),
                         n00b_kw("max", max));

    return n00b_tree_get_number_children(table->column_specs);
}

int64_t
n00b_table_next_column_set_width_pct(n00b_table_t *table,
                                     double        min,
                                     double        max)
{
    n00b_new_layout_cell(table->column_specs,
                         n00b_kw("min_pct", min),
                         n00b_kw("max_pct", max));

    return n00b_tree_get_number_children(table->column_specs);
}

void
n00b_table_set_column_priority(n00b_table_t *table, int64_t col, int64_t pri)
{
    n00b_tree_node_t *n = n00b_tree_get_child(table->column_specs, col);

    if (!n) {
        N00B_CRAISE("Column's properties have not been declared yet.");
    }

    n00b_layout_t *layout = n00b_tree_get_contents(n);

    layout->priority = pri;
}

static void
n00b_table_init(n00b_table_t *table, va_list args)
{
    n00b_stream_t          *outstream        = NULL;
    bool                    eject_on_render  = true;
    n00b_string_t          *theme_name       = NULL; // Default is "table"
    n00b_decoration_style_t decoration_style = N00B_TABLE_DEFAULT;
    int64_t                 num_columns      = 0;
    n00b_list_t            *contents         = NULL;
    n00b_list_t            *column_widths    = NULL;
    n00b_string_t          *title            = NULL;
    n00b_string_t          *caption          = NULL;
    n00b_theme_t           *theme            = NULL;

    n00b_karg_va_init(args);
    n00b_kw_ptr("theme", theme_name);
    n00b_kw_ptr("outstream", outstream);
    n00b_kw_ptr("contents", contents);
    n00b_kw_ptr("column_widths", column_widths);
    n00b_kw_ptr("title", title);
    n00b_kw_ptr("caption", caption);
    n00b_kw_bool("nocache", eject_on_render);
    n00b_kw_int64("columns", num_columns);
    n00b_kw_int32("style", decoration_style);

    if (!theme_name) {
        theme_name = n00b_cstring("table");
    }

    theme = n00b_lookup_theme(theme_name);

    if (!theme) {
        theme = n00b_get_current_theme();
    }

    n00b_lock_init(&table->lock);
    table->theme            = theme;
    table->outstream        = outstream;
    table->stored_cells     = n00b_list(n00b_type_ref());
    table->current_row      = n00b_list(n00b_type_ref());
    table->eject_on_render  = eject_on_render;
    table->title            = title;
    table->caption          = caption;
    table->max_col          = num_columns;
    table->decoration_style = decoration_style;
    table->column_specs     = n00b_new_layout();
    table->render_cache     = n00b_list(n00b_type_string());

    if (contents) {
        n00b_table_add_contents(table, contents);
    }
}

static inline void
exception_cleanup(n00b_table_t *table)
{
    if (table->locked_row) {
        n00b_unlock_list(table->locked_row);
    }
    if (table->locked_list) {
        n00b_unlock_list(table->locked_list);
    }
}

static inline void
internal_table_end_row(n00b_table_t *table)
{
    n00b_list_append(table->stored_cells, table->current_row);
    table->current_row = n00b_list(n00b_type_ref());
    table->row_cursor += 1;
    table->col_cursor = 0;
}

static void
n00b_row_width_check(n00b_table_t *table)
{
    if (table->max_col) {
        if (table->col_cursor > table->max_col) {
            exception_cleanup(table);
            N00B_CRAISE("Cell added to row extends beyond the set # columns.");
        }
        if (table->col_cursor == table->max_col) {
            internal_table_end_row(table);
            if (table->outstream) {
                emit_cache(table, false);
            }
        }
    }
    else {
        if (table->col_cursor > table->current_num_cols) {
            table->current_num_cols = table->col_cursor;
        }
    }
}

static inline n00b_box_props_t *
n00b_get_cell_formatting(n00b_table_t *table)
{
    int index;

    switch (table->decoration_style) {
        // Vertically oriented formatting.
    case N00B_TABLE_UL:
    case N00B_TABLE_OL:
    case N00B_TABLE_VHEADER:
        index = table->col_cursor;
        break;
    default:
        index = table->row_cursor;
        break;
    }

    if (!index) {
        return n00b_header_box(table);
    }
    if (index % 2) {
        return n00b_cell_box(table);
    }
    return n00b_alt_cell_box(table);
}

static inline n00b_cell_t *
raw_empty_cell(n00b_table_t *table)
{
    n00b_cell_t *cell = n00b_gc_alloc_mapped(n00b_cell_t,
                                             N00B_GC_SCAN_ALL);
    cell->contents    = n00b_cached_empty_string();
    cell->formatting  = n00b_get_cell_formatting(table);

    return cell;
}
void
n00b_table_empty_cell(n00b_table_t *table)
{
    defer_on();
    n00b_table_acquire(table);
    n00b_cell_t *cell = raw_empty_cell(table);
    n00b_list_append(table->current_row, cell);
    table->col_cursor += 1;
    n00b_row_width_check(table);

    Return;

    defer_func_end();
}

bool
_n00b_table_add_cell(n00b_table_t *table, void *contents, ...)
{
    defer_on();

    if (!contents) {
        contents = n00b_cached_empty_string();
    }

    int64_t           span;
    va_list           args;
    n00b_box_props_t *custom_props = NULL;
    n00b_type_t      *t;

    va_start(args, contents);
    span = va_arg(args, int64_t);

    if (span > 0) {
        custom_props = va_arg(args, n00b_box_props_t *);
    }
    else {
        if (span == N00B_COL_SPAN_ALL) {
            if (table->max_col) {
                span = table->max_col - table->col_cursor;
            }
            else {
                span           = table->current_num_cols - table->col_cursor;
                table->max_col = table->current_num_cols;
            }
        }
        else {
            span = 1;
        }
    }

    t = n00b_get_my_type(contents);

    if (!t) {
        exception_cleanup(table);
        Return
            N00B_CRAISE("Content is not an object type."),
            false;
    }

    n00b_cell_t *cell = n00b_gc_alloc_mapped(n00b_cell_t,
                                             N00B_GC_SCAN_ALL);

    cell->contents = contents;
    cell->span     = span;

    // For now, we aren't going to merge these, we're just going to take
    // them part and parcel.
    if (custom_props) {
        cell->formatting = custom_props;
    }
    else {
        cell->formatting = n00b_get_cell_formatting(table);
    }

    n00b_table_acquire(table);
    n00b_list_append(table->current_row, cell);
    if (span > 0) {
        table->col_cursor += span;
    }
    if (span < 0) {
        table->col_cursor = 0;
    }
    n00b_row_width_check(table);

    Return true;
    defer_func_end();
}

void
n00b_table_end_row(n00b_table_t *table)
{
    defer_on();
    n00b_table_acquire(table);
    internal_table_end_row(table);
    if (table->outstream) {
        emit_cache(table, false);
    }

    Return;
    defer_func_end();
}

static inline void
internal_add_row(n00b_table_t *table, n00b_list_t *row)
{
    if (n00b_list_len(table->current_row) != 0) {
        internal_table_end_row(table);
    }

    n00b_lock_list_read(row);
    table->locked_row = row;
    int n             = n00b_list_len(row);

    for (int i = 0; i < n; i++) {
        n00b_table_add_cell(table, n00b_private_list_get(row, i, NULL));
    }

    internal_table_end_row(table);
    table->locked_row = NULL;
    n00b_unlock_list(row);
}

void
n00b_table_add_row(n00b_table_t *table, n00b_list_t *row)
{
    defer_on();
    n00b_table_acquire(table);
    internal_add_row(table, row);
    if (table->outstream) {
        emit_cache(table, false);
    }

    Return;
    defer_func_end();
}

void
n00b_table_end(n00b_table_t *table)
{
    defer_on();
    n00b_table_acquire(table);
    table->max_col = table->row_cursor;

    if (table->outstream) {
        emit_cache(table, true);
    }

    Return;
    defer_func_end();
}

bool
n00b_table_add_contents(n00b_table_t *table, n00b_list_t *l)
{
    defer_on();

    n00b_table_acquire(table);
    n00b_lock_list_read(l);
    table->locked_list = l;
    int n              = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_list_t *row = n00b_private_list_get(l, i, NULL);
        n00b_type_t *t   = n00b_get_my_type(row);

        if (!n00b_type_is_list(t)) {
            exception_cleanup(table);
            Return
                N00B_CRAISE("Row passed as table contents is not a list."),
                false;
        }
        internal_add_row(table, row);
    }

    table->locked_list = NULL;
    n00b_unlock_list(l);

    Return true;
    defer_func_end();
}

n00b_list_t *
n00b_table_render(n00b_table_t *table, int width, int ignored)
{
    defer_on();
    n00b_table_acquire(table);
    n00b_stream_t *saved_outstream = table->outstream;

    if (saved_outstream && table->eject_on_render) {
        Return N00B_CRAISE("Table is set to stream and release data as it outputs."), NULL;
    }

    // We'll manually add to table->render_cache when we call render()
    table->outstream = NULL;
    table->did_setup = false;

    setup_table_rendering(table, width);
    emit_cache(table, true);

    table->outstream = saved_outstream;

    n00b_list_t *result = table->render_cache;
    table->render_cache = n00b_list(n00b_type_string());

    Return result;
    defer_func_end();
}

n00b_string_t *
n00b_table_to_string(n00b_table_t *table)
{
    return n00b_string_join(n00b_table_render(table, 0, 0),
                            n00b_cached_empty_string());
}

static inline int
calculate_col_space_available(n00b_table_t     *table,
                              int               width,
                              n00b_box_props_t *table_props)
{
    int available = width;

    available -= get_left_pad(table_props);
    available -= get_right_pad(table_props);

    if (table_props->borders & N00B_BORDER_INFO_LEFT) {
        available -= 1;
    }
    if (table_props->borders & N00B_BORDER_INFO_RIGHT) {
        available -= 1;
    }

    return available;
}

static inline void
build_outer_pad(n00b_table_t *table)
{
    n00b_string_t *s = n00b_string_repeat(' ', table->total_width + 1);

    s->data[s->codepoints - 1] = '\n';
    table->outer_pad_cache     = s;
}

static inline void
build_hborder(n00b_table_t *table, n00b_box_props_t *props, n00b_border_t bi)
{
    // Horizontal border between cells.  This includes the outer
    // edges.
    uint8_t hbar_buf[4] = {
        0,
    };
    uint8_t cross_buf[4] = {
        0,
    };
    uint8_t l_t_buf[4] = {
        0,
    };
    uint8_t r_t_buf[4] = {
        0,
    };

    int hl = 0;
    int cl = 0;
    int ll = 0;
    int rl = 0;

    hl = utf8proc_encode_char(props->border_theme->horizontal_rule, hbar_buf);

    n00b_assert(hl > 0);

    if (bi & N00B_BORDER_INFO_VINTERIOR) {
        cl = utf8proc_encode_char(props->border_theme->cross, cross_buf);
        n00b_assert(cl > 0);
    }

    if (bi & N00B_BORDER_INFO_LEFT) {
        ll = utf8proc_encode_char(props->border_theme->left_t, l_t_buf);
        n00b_assert(ll > 0);
    }

    if (bi & N00B_BORDER_INFO_RIGHT) {
        rl = utf8proc_encode_char(props->border_theme->right_t, r_t_buf);
        n00b_assert(rl > 0);
    }

    // sum incrementally.
    // Dashes go over column text.
    int col_factor = cl ? table->max_col - 1 : 0;
    int vbar_cols  = table->column_width_used - col_factor;
    int sum        = hl * vbar_cols + 1;
    // Space for Interior border cross, one for each rendered column minus one.
    sum += cl * col_factor;
    // Size of left and right padding.
    sum += ll + rl;
    // Above we counted borders, but need to add assigned pad info, which
    // gets spaces above it.
    int lpad = get_left_pad(props) + table->add_left;
    int rpad = get_right_pad(props) + table->add_right;

    sum += lpad + rpad;

    // Allocate and build.
    n00b_string_t *s = n00b_new(n00b_type_string(), NULL, true, sum);
    char          *p = s->data;

    for (int i = 0; i < lpad; i++) {
        *p++ = ' ';
    }
    for (int i = 0; i < ll; i++) {
        *p++ = l_t_buf[i];
    }

    int num_rendered = 0;
    int n            = table->max_col;

    for (int i = 0; i < n; i++) {
        int64_t w = (int64_t)n00b_private_list_get(table->cur_widths, i, NULL);
        for (int j = 0; j < w; j++) {
            for (int k = 0; k < hl; k++) {
                *p++ = hbar_buf[k];
            }
        }
        if (++num_rendered != table->max_col) {
            for (int k = 0; k < cl; k++) {
                *p++ = cross_buf[k];
            }
        }
    }

    for (int i = 0; i < rl; i++) {
        *p++ = r_t_buf[i];
    }
    for (int i = 0; i < rpad; i++) {
        *p++ = ' ';
    }

    *p++          = '\n';
    s->codepoints = table->total_width + 1;

    n00b_assert(p - s->data == sum);

    table->hborder_cache = s;

    // We substract 1 extra for the newline, which we do not want to style;
    // if we style it, when we are rendering sub-tables we will get
    // an extra character of background in the pad that we won't want.
    n00b_internal_style_range(s,
                              props,
                              table->add_left,
                              s->codepoints - table->add_right - 1);
}

static inline void
build_vborder(n00b_table_t *table, n00b_box_props_t *props)
{
    uint8_t vbar_buf[4] = {
        0,
    };

    int l = utf8proc_encode_char(props->border_theme->vertical_rule, vbar_buf);
    n00b_assert(l > 0);

    n00b_string_t *s = n00b_new(n00b_type_string(), NULL, true, l);
    char          *p = s->data;

    for (int i = 0; i < l; i++) {
        *p++ = vbar_buf[i];
    }

    s->codepoints = 1;

    n00b_string_style_without_copying(s, props);
    table->vborder_cache = s;
}

static inline void
build_lpad(n00b_table_t *table, n00b_box_props_t *props, bool add_border)
{
    uint8_t vbar_buf[4] = {
        0,
    };
    int l = 0;

    if (add_border && props->border_theme
        && props->border_theme->vertical_rule) {
        l = utf8proc_encode_char(props->border_theme->vertical_rule, vbar_buf);
        n00b_assert(l > 0);
    }

    int lpad = get_left_pad(props) + table->add_left;
    int len  = lpad + l;

    if (!len) {
        return;
    }

    n00b_string_t *s = n00b_new(n00b_type_string(), NULL, true, len);
    char          *p = s->data;

    for (int i = 0; i < lpad; i++) {
        *p++ = ' ';
    }

    for (int i = 0; i < l; i++) {
        *p++ = vbar_buf[i];
    }

    s->codepoints = lpad + (l ? 1 : 0);

    n00b_internal_style_range(s, props, table->add_left, s->codepoints);
    table->left_cache = s;
}

static inline void
build_rpad(n00b_table_t *table, n00b_box_props_t *props, bool add_border)
{
    // Since we don't know if we're a sub-box or full screen
    // width, go ahead and add a newline after every line making
    // up the row.
    uint8_t vbar_buf[4] = {
        0,
    };

    int l = 0;

    if (add_border && props->border_theme
        && props->border_theme->vertical_rule) {
        l = utf8proc_encode_char(props->border_theme->vertical_rule,
                                 vbar_buf);
        n00b_assert(l > 0);
    }

    int            rpad = get_right_pad(props) + table->add_right;
    int            len  = rpad + l + 1; // 1 for the newline.
    n00b_string_t *s    = n00b_new(n00b_type_string(), NULL, true, len);
    char          *p    = s->data;

    for (int i = 0; i < l; i++) {
        *p++ = vbar_buf[i];
    }

    for (int i = 0; i < rpad; i++) {
        *p++ = ' ';
    }

    *p = '\n';

    s->codepoints = rpad + (l ? 2 : 1);

    n00b_internal_style_range(s, props, 0, s->codepoints - table->add_right - 1);
    table->right_cache = s;
}

static inline void
setup_cache_items(n00b_table_t *table, n00b_box_props_t *props)
{
    n00b_border_t binfo = props->borders;

    if (binfo == N00B_BORDER_INFO_UNSPECIFIED) {
        binfo = N00B_BORDER_INFO_NONE;
    }

    // At some point, we will cache the vertical borders and pad
    // between cells, and blank lines per-column, under the assumption
    // that default formatting is used.
    //
    // However, when we do that, we still will have to detect when a
    // cell overrides, because at that point we need to have the pad
    // match the cell (but not the border).
    //
    // For now, we just cache the exterior pads that cannot be
    // overriden, to avoid complicating things.
    build_lpad(table,
               props,
               binfo & (N00B_BORDER_INFO_LEFT != N00B_BORDER_INFO_NONE));
    build_rpad(table,
               props,
               binfo & (N00B_BORDER_INFO_RIGHT != N00B_BORDER_INFO_NONE));

    // If there's an internal horizontal border, cache it here.
    if (binfo & N00B_BORDER_INFO_HINTERIOR) {
        build_hborder(table, props, binfo);
    }

    if (binfo & N00B_BORDER_INFO_VINTERIOR) {
        build_vborder(table, props);
    }

    if (get_top_pad(props) || get_bottom_pad(props)) {
        build_outer_pad(table);
    }

    // Note: The first and last row borders only get used once per
    // render, so we don't currently cache those.
}

static inline n00b_list_t *
extract_column(n00b_table_t *t, int col)
{
    int          n      = n00b_list_len(t->stored_cells);
    n00b_list_t *result = n00b_list(n00b_type_ref());

    for (int i = 0; i < n; i++) {
        n00b_list_t *row  = n00b_private_list_get(t->stored_cells, i, NULL);
        n00b_cell_t *cell = n00b_private_list_get(row, col, NULL);

        if (cell) {
            n00b_private_list_append(result, cell);
        }
    }

    return result;
}

static inline int
longest_word_len(n00b_string_t *s)
{
    n00b_list_t *l   = n00b_string_split_words(s,
                                             n00b_kw("punctuation",
                                                     n00b_ka(false)));
    int          max = 0;
    int          n   = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_string_t *w = n00b_list_get(l, i, NULL);
        if (w->codepoints > max) {
            max = w->codepoints;
        }
    }

    return max;
}

static inline int
longest_line_len(n00b_string_t *s)
{
    int            longest = 0;
    int            start   = 0;
    int            next    = 0;
    n00b_string_t *nl      = n00b_cached_newline();

    while (start < s->codepoints) {
        next = n00b_string_find(s, nl, n00b_kw("start", n00b_ka(start)));
        if (next < 0) {
            next = s->codepoints;
        }
        int diff = next - start;
        if (diff > longest) {
            longest = diff;
        }
        start = next + 1;
    }

    return longest;
}

static inline void
set_column_preferences(n00b_table_t *table, int64_t width)
{
    for (int i = 0; i < table->max_col; i++) {
        n00b_list_t *col_contents;
        int64_t      longest_line = 0;
        int64_t      longest_word = 0;
        int          pad;

        n00b_tree_node_t *t      = n00b_tree_get_child(table->column_specs, i);
        n00b_layout_t    *layout = n00b_tree_get_contents(t);

        if (layout->flex_multiple > 0) {
            continue;
        }

        col_contents = extract_column(table, i);

        for (int j = 0; j < n00b_list_len(col_contents); j++) {
            n00b_cell_t *cell = n00b_private_list_get(col_contents, j, NULL);

            if (!n00b_type_is_string(n00b_get_my_type(cell->contents))) {
                continue;
            }

            n00b_string_t *s      = cell->contents;
            int            max_wd = longest_word_len(s);
            int            llen   = longest_line_len(s);

            pad = get_left_pad(cell->formatting);
            pad += get_right_pad(cell->formatting);

            int span = cell->span;
            if (span < 0) {
                span = table->max_col - i;
            }
            if (span > 1) {
                max_wd /= span;
                llen /= span;
            }

            max_wd += pad;
            llen += pad;

            if (max_wd > longest_word) {
                longest_word = max_wd;
            }

            if (llen > longest_line) {
                longest_line = llen;
            }
        }

        // If our column's contents are never strings, we don't try to
        // be smart, we just turn the column into a 'flex' column. If
        // it doesn't produce good results, then set the columns
        // manually!

        if (!longest_line) {
            layout->flex_multiple = 1;
            continue;
        }

        // If we have data in the table instead of english words (for
        // instance, hex data), then the word boundry isn't actually
        // an awesome lower bound; we probably want something higher.
        // Generally speaking, the number of common words in english
        // of length 16 or higher is negligable, so we use that as a
        // cut-off.
        if (width < longest_word && width > 15) {
            longest_word = width / table->max_col;
        }
        else {
            // We're going to end up using the 'longest_word' to try to
            // prevent bad wrapping. We add '2' to it here, to account for
            // possible important punctuation that doesn't wrap, like an
            // empahsis dash.
            longest_word += 2;
        }

        layout->min.value.i = longest_word;

        // If the longest line *could* fit it in the full width, we'll
        // take that width if we can get it!
        //
        // Otherwise, we aim for our proportional column size, given
        // the number of columns (as long as that's not smaller than
        // our biggest word).

        if (longest_line < width) {
            // If we actually fit, don't bother giving us extra space,
            // we might be taking it from columns that don't need it.
            layout->pref.value.i = longest_line;
            layout->max.value.i  = layout->pref.value.i;
        }
        else {
            layout->pref.value.i = n00b_max(longest_word,
                                            width / table->max_col);
        }
    }
}

static inline void
layout_table(n00b_table_t *table, int64_t width)
{
    n00b_assert(table->max_col);

    while (table->max_col > n00b_tree_get_number_children(table->column_specs)) {
        n00b_table_next_column_fit(table);
    }

    // First, subtract out width we cannot give to columns.
    n00b_layout_t *layout = n00b_tree_get_contents(table->column_specs);

    if (table->outer_borders & N00B_BORDER_INFO_VINTERIOR) {
        // We have interior vertical borders, so tell the layout manager
        // to add a character of pad between columns.
        layout->child_pad = 1;
    }
    else {
        layout->child_pad = 0;
    }

    // Based on what text is present (if any) the first time we render
    // any part of the table, we choose 'preferred' widths, which are
    // starting widths for the layout algorithm-- the length of the
    // longest FIRST line we see in any cell in the column (padding
    // included).
    //
    // If a column only contains cells that holds sub-tables or other
    // objects, (no strings), then we make it a 'flex' column with a
    // flex value of 1. In that case, we will set the starting value
    // to 1/n of the available width, where n is the total number of
    // columns.

    set_column_preferences(table, width);

    table->column_actuals = n00b_layout_calculate(table->column_specs, width);
    table->cur_widths     = n00b_list(n00b_type_int());

    int l = n00b_tree_get_number_children(table->column_actuals);

    table->column_width_used = 0;

    for (int i = 0; i < l; i++) {
        n00b_tree_node_t     *n = n00b_tree_get_child(table->column_actuals, i);
        n00b_layout_result_t *r = n00b_tree_get_contents(n);

        table->column_width_used += r->size;
        n00b_private_list_append(table->cur_widths, (void *)r->size);
    }

    table->column_width_used += layout->child_pad * (table->max_col - 1);
    int additional_pad = width - table->column_width_used;

    table->add_left  = 0;
    table->add_right = 0;

    if (!additional_pad) {
        return;
    }

    n00b_box_props_t *box = n00b_outer_box(table);
    if (box->alignment & N00B_ALIGN_LEFT) {
        table->add_right = additional_pad;
        return;
    }

    if (box->alignment & N00B_ALIGN_RIGHT) {
        table->add_left = additional_pad;
    }

    table->add_left  = additional_pad / 2;
    table->add_right = table->add_left;

    if (additional_pad & 1) {
        table->add_right++;
    }
}

static void
setup_table_rendering(n00b_table_t *table, int width)
{
    width = n00b_calculate_render_width(width);

    if (!table->max_col) {
        table->max_col = table->current_num_cols;
    }

    table->total_width = width;

    n00b_box_props_t *table_props = n00b_outer_box(table);
    table->outer_borders          = table_props->borders;

    int64_t available = calculate_col_space_available(table,
                                                      width,
                                                      table_props);

    layout_table(table, available);
    setup_cache_items(table, table_props);

    table->did_setup = true;
}

static void
core_emit(n00b_table_t *table, n00b_string_t *s)
{
    if (table->outstream) {
        n00b_write(table->outstream, s);
    }
    else {
        n00b_private_list_append(table->render_cache, s);
    }
}

static inline n00b_list_t *
align_lines_horizontal(n00b_list_t      *lines,
                       int               w,
                       int               lpad,
                       int               rpad,
                       n00b_box_props_t *fmt)
{
    // The passed in width is the width before any padding is added.
    int n  = n00b_list_len(lines);
    int tw = w + lpad + rpad;

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_private_list_get(lines, i, NULL);
        n00b_string_t *tmp;
        n00b_string_t *new = s;

        if (fmt->alignment & N00B_ALIGN_CENTER) {
            new = n00b_string_align_center(new, w);
        }
        else {
            if (fmt->alignment & N00B_ALIGN_RIGHT) {
                new = n00b_string_align_right(s, w);
            }
            else {
                new = n00b_string_align_left(s, w);
            }
        }

        if (lpad > 0) {
            tmp = n00b_string_repeat(' ', lpad);
            new = n00b_string_concat(tmp, new);
        }
        if (rpad > 0) {
            tmp = n00b_string_repeat(' ', rpad);
            new = n00b_string_concat(new, tmp);
        }

        if (n00b_string_render_len(new) > tw) {
            new = n00b_string_truncate(new, tw);
        }

        if (new != s) {
            n00b_private_list_set(lines, i, new);
        }

        n00b_string_style_without_copying(new, fmt);
    }

    return lines;
}

static inline void
render_cell(n00b_cell_t *cell)
{
    n00b_box_props_t *fmt       = cell->formatting;
    int               width     = cell->wcache;
    int               lpad      = get_left_pad(fmt);
    int               rpad      = get_right_pad(fmt);
    int               available = width - lpad - rpad;

    // If the width is eaten up by pad, either go down to
    // no-more-than-one col of pad on either side, or if that still
    // isn't good enough, abandon all the pad.
    if (available < N00B_MIN_WIDTH_BEFORE_PADDING) {
        lpad      = n00b_min(lpad, 1);
        rpad      = n00b_min(rpad, 1);
        available = width - lpad - rpad;

        if (available < N00B_MIN_WIDTH_BEFORE_PADDING) {
            available = width;
            lpad      = 0;
            rpad      = 0;
        }
    }
    n00b_list_t *lines;

    if (n00b_is_renderable(cell->contents)) {
        lines            = n00b_render(cell->contents, available, 0);
        n00b_string_t *s = n00b_string_join(lines, n00b_cached_empty_string());
        lines            = n00b_string_split(s, n00b_cached_newline());
    }
    else {
        n00b_string_t *s = n00b_to_string(cell->contents);

        if (fmt->wrap) {
            int hang = 0;
            if (fmt->hang > 0) {
                hang = fmt->hang;
            }
            lines = n00b_string_wrap(s, available, hang);
        }
        else {
            lines = n00b_string_split_and_crop(s,
                                               n00b_cached_newline(),
                                               available);
        }
    }

    cell->ccache = align_lines_horizontal(lines, available, lpad, rpad, fmt);

    cell->top_pad    = get_top_pad(fmt);
    cell->bottom_pad = get_bottom_pad(fmt);
}

static inline int
align_cells_vertically(n00b_list_t *row)
{
    // Align all cells in a row to the proper padded height, making sure
    // all cells are the same size.

    int          n          = n00b_list_len(row);
    int          max_height = 0;
    n00b_cell_t *cell;

    for (int i = 0; i < n; i++) {
        cell  = n00b_private_list_get(row, i, NULL);
        int l = n00b_list_len(cell->ccache);
        l += cell->top_pad;
        l += cell->bottom_pad;

        if (l > max_height) {
            max_height = l;
        }
    }

    for (int i = 0; i < n; i++) {
        cell  = n00b_private_list_get(row, i, NULL);
        int l = n00b_list_len(cell->ccache);
        if (l >= max_height) {
            continue;
        }

        int ntop           = cell->top_pad;
        int nbottom        = cell->bottom_pad;
        int total_pad      = ntop + nbottom;
        int additional_pad = max_height - l - total_pad;
        if (additional_pad) {
            if (cell->formatting->alignment & N00B_ALIGN_TOP) {
                ntop += additional_pad;
            }
            else {
                if (cell->formatting->alignment & N00B_ALIGN_BOTTOM) {
                    nbottom += additional_pad;
                }
                else {
                    ntop += additional_pad / 2;
                    nbottom += ntop + additional_pad % 2;
                }
            }
        }

        n00b_string_t *pad_line = n00b_string_repeat(' ', cell->wcache);
        n00b_string_style_without_copying(pad_line, cell->formatting);

        n00b_assert(pad_line->codepoints == cell->wcache);

        for (int j = 0; j < nbottom; j++) {
            n00b_private_list_append(cell->ccache, pad_line);
        }

        if (ntop) {
            int          new_len = n00b_list_len(cell->ccache) + ntop;
            n00b_list_t *tmp     = n00b_new(n00b_type_list(n00b_type_string()),
                                        n00b_kw("length", n00b_ka(new_len)));

            for (int i = 0; i < ntop; i++) {
                n00b_private_list_set(tmp, i, pad_line);
            }

            for (int i = ntop; i < new_len; i++) {
                n00b_string_t *s = n00b_private_list_get(cell->ccache,
                                                         i - ntop,
                                                         NULL);
                n00b_private_list_set(tmp, i, s);
            }

            cell->ccache = tmp;
        }
    }

    return max_height;
}

static inline int
get_cell_width(n00b_table_t *table, int start_col, int span)
{
    // Returns the width of a single cell, given its span. It takes
    // into account whether there's a border between rows (the child
    // pad).
    //
    // However, it does not account for padding *within* a cell. That
    // happens when we're rendering.

    n00b_tree_node_t     *node;
    n00b_layout_t        *winfo  = n00b_tree_get_contents(table->column_specs);
    n00b_layout_result_t *acts   = n00b_tree_get_contents(table->column_actuals);
    int                   cpad   = winfo->child_pad;
    int                   result = cpad * (span - 1);

    for (int i = 0; i < span; i++) {
        node = n00b_tree_get_child(table->column_actuals, i + start_col);
        acts = n00b_tree_get_contents(node);
        result += acts->size;
    }

    return result;
}

static inline void
emit_cell_row(n00b_table_t *table,
              n00b_list_t  *cells,
              int           row_ix,
              bool          add_border,
              bool          outer_override)
{
    // 1. Set column widths inside the cell objects.
    // 2. Apply column spans when doing so.
    // 3. If there aren't enough cells in a row, the last cell eats
    //    up the rest of the space.
    // 4. Render.

    n00b_cell_t   *cell;
    n00b_string_t *s;
    int            n      = n00b_list_len(cells);
    int            col_ix = 0;

    for (int i = 0; i < n - 1; i++) {
        int start = col_ix;

        cell = n00b_private_list_get(cells, i, NULL);
        col_ix += cell->span < 1 ? 1 : cell->span;
        cell->wcache = get_cell_width(table, start, cell->span);
    }

    cell = n00b_private_list_get(cells, n - 1, NULL);

    if (!cell) {
        cell = raw_empty_cell(table);
    }

    cell->span   = table->max_col - col_ix;
    cell->wcache = get_cell_width(table, col_ix, cell->span);

    if (add_border && table->hborder_cache) {
        core_emit(table, table->hborder_cache);
    }

    n = n00b_list_len(cells);

    // First, render each cell. This will apply horizontal padding
    // and alignment to the given width.
    for (int i = 0; i < n; i++) {
        n00b_cell_t *cell = n00b_private_list_get(cells, i, NULL);
        render_cell(cell);
    }

    // Now, apply vertical padding and alignment to the whole row.
    int height = align_cells_vertically(cells);

    for (int i = 0; i < height; i++) {
        if (table->left_cache) {
            if (!outer_override) {
                core_emit(table, table->left_cache);
            }
            else {
                s = n00b_string_repeat(' ', table->left_cache->codepoints);
                // TODO: this needs to pick up the background styling of
                // either the text or caption in a better way. Currently
                // it gets done by string concat, which I will take away.
                core_emit(table, s);
            }
        }

        for (int j = 0; j < n; j++) {
            n00b_cell_t   *cell       = n00b_private_list_get(cells, j, NULL);
            n00b_list_t   *cell_lines = cell->ccache;
            n00b_string_t *line       = n00b_private_list_get(cell_lines,
                                                        i,
                                                        NULL);

            core_emit(table, line);

            if (table->vborder_cache && j + 1 != n) {
                core_emit(table, table->vborder_cache);
            }
        }

        if (table->right_cache && !outer_override) {
            core_emit(table, table->right_cache);
        }
        else {
            s = n00b_string_repeat(' ', table->right_cache->codepoints);

            s->data[table->right_cache->codepoints - 1] = '\n';
            core_emit(table, s);
        }
    }
}

static inline void
emit_cached_rows(n00b_table_t *table)
{
    int n = n00b_list_len(table->stored_cells);

    for (; table->next_row_to_output < n; table->next_row_to_output++) {
        n00b_list_t *row = n00b_private_list_get(table->stored_cells,
                                                 table->next_row_to_output,
                                                 NULL);

        emit_cell_row(table,
                      row,
                      table->next_row_to_output,
                      table->next_row_to_output != 0,
                      false);
    }
}
static inline bool
emit_top_border(n00b_table_t *table, n00b_box_props_t *props)
{
    n00b_border_t borders = props->borders;

    if (!(borders & N00B_BORDER_INFO_TOP)) {
        return false;
    }

    if (!props->border_theme) {
        return false;
    }
    // This should look like the hborder we cached, except that we
    // use different drawing characters where appropriate.

    uint8_t hbar_buf[4] = {
        0,
    };
    uint8_t t_buf[4] = {
        0,
    };
    uint8_t l_buf[4] = {
        0,
    };
    uint8_t r_buf[4] = {
        0,
    };

    int hl = 0;
    int tl = 0;
    int ll = 0;
    int rl = 0;

    hl = utf8proc_encode_char(props->border_theme->horizontal_rule, hbar_buf);
    n00b_assert(hl > 0);

    if (borders & N00B_BORDER_INFO_VINTERIOR) {
        tl = utf8proc_encode_char(props->border_theme->top_t, t_buf);
        n00b_assert(tl > 0);
    }

    if (borders & N00B_BORDER_INFO_LEFT) {
        ll = utf8proc_encode_char(props->border_theme->upper_left, l_buf);
        n00b_assert(ll > 0);
    }

    if (borders & N00B_BORDER_INFO_RIGHT) {
        rl = utf8proc_encode_char(props->border_theme->upper_right, r_buf);
        n00b_assert(rl > 0);
    }

    int col_factor = tl ? table->max_col - 1 : 0;
    int vbar_cols  = table->column_width_used - col_factor;
    int sum        = hl * vbar_cols + 1; // +1 for newline

    sum += tl * col_factor;
    sum += ll + rl;

    int lpad = get_left_pad(props) + table->add_left;
    int rpad = get_right_pad(props) + table->add_right;

    sum = sum + lpad + rpad;

    n00b_string_t *s = n00b_new(n00b_type_string(), NULL, true, sum);
    char          *p = s->data;

    for (int i = 0; i < lpad; i++) {
        *p++ = ' ';
    }

    for (int i = 0; i < ll; i++) {
        *p++ = l_buf[i];
    }

    int num_rendered = 0;
    int n            = table->max_col;

    for (int i = 0; i < n; i++) {
        int64_t w = (int64_t)n00b_private_list_get(table->cur_widths, i, NULL);

        for (int j = 0; j < w; j++) {
            for (int k = 0; k < hl; k++) {
                *p++ = hbar_buf[k];
            }
        }

        if (++num_rendered != table->max_col) {
            for (int k = 0; k < tl; k++) {
                *p++ = t_buf[k];
            }
        }
    }

    for (int i = 0; i < rl; i++) {
        *p++ = r_buf[i];
    }

    for (int i = 0; i < rpad; i++) {
        *p++ = ' ';
    }

    *p++ = '\n';

    s->codepoints = table->total_width + 1;
    n00b_assert(p - s->data == sum);

    n00b_internal_style_range(s,
                              props,
                              table->add_left,
                              s->codepoints - table->add_right - 1);

    core_emit(table, s);

    return true;
}

static inline void
emit_bottom_border(n00b_table_t *table, n00b_box_props_t *props)
{
    n00b_border_t borders = props->borders;

    if (!(borders & N00B_BORDER_INFO_BOTTOM)) {
        return;
    }

    if (!props->border_theme) {
        return;
    }

    // This should look like the hborder we cached, except that we
    // use different drawing characters where appropriate.

    uint8_t hbar_buf[4] = {
        0,
    };
    uint8_t t_buf[4] = {
        0,
    };
    uint8_t l_buf[4] = {
        0,
    };
    uint8_t r_buf[4] = {
        0,
    };

    int hl = 0;
    int tl = 0;
    int ll = 0;
    int rl = 0;

    hl = utf8proc_encode_char(props->border_theme->horizontal_rule, hbar_buf);
    n00b_assert(hl > 0);

    if (borders & N00B_BORDER_INFO_VINTERIOR) {
        tl = utf8proc_encode_char(props->border_theme->bottom_t, t_buf);
        n00b_assert(tl > 0);
    }

    if (borders & N00B_BORDER_INFO_LEFT) {
        ll = utf8proc_encode_char(props->border_theme->lower_left, l_buf);
        n00b_assert(ll > 0);
    }

    if (borders & N00B_BORDER_INFO_RIGHT) {
        rl = utf8proc_encode_char(props->border_theme->lower_right, r_buf);
        n00b_assert(rl > 0);
    }

    int col_factor = tl ? table->max_col - 1 : 0;
    int vbar_cols  = table->column_width_used - col_factor;
    int sum        = hl * vbar_cols + 1; // + 1 for newline;
    sum += tl * col_factor;
    sum += ll + rl;

    int lpad = get_left_pad(props) + table->add_left;
    int rpad = get_right_pad(props) + table->add_right;

    sum = sum + lpad + rpad;

    n00b_string_t *s = n00b_new(n00b_type_string(), NULL, true, sum);
    char          *p = s->data;

    for (int i = 0; i < lpad; i++) {
        *p++ = ' ';
    }
    for (int i = 0; i < ll; i++) {
        *p++ = l_buf[i];
    }

    int num_rendered = 0;
    int n            = table->max_col;

    for (int i = 0; i < n; i++) {
        int64_t w = (int64_t)n00b_private_list_get(table->cur_widths, i, NULL);
        for (int j = 0; j < w; j++) {
            for (int k = 0; k < hl; k++) {
                *p++ = hbar_buf[k];
            }
        }
        if (++num_rendered != table->max_col) {
            for (int k = 0; k < tl; k++) {
                *p++ = t_buf[k];
            }
        }
    }

    for (int i = 0; i < rl; i++) {
        *p++ = r_buf[i];
    }
    for (int i = 0; i < rpad; i++) {
        *p++ = ' ';
    }

    *p++ = '\n';

    n00b_assert(p - s->data == sum);

    s->codepoints = table->total_width + 1;

    n00b_internal_style_range(s,
                              props,
                              table->add_left,
                              s->codepoints - table->add_right - 1);

    core_emit(table, s);
}

static inline void
emit_title(n00b_table_t *table)
{
    // We're mostly going to treat this like a table row w/ one cell,
    // that uses the title properties from the theme. The width of the
    // 'column' ends up being the render width of the table (taking
    // into account any specified padding).
    if (!table->title) {
        return;
    }

    n00b_box_props_t *props = n00b_title_box(table);
    n00b_cell_t      *cell  = n00b_gc_alloc_mapped(n00b_cell_t,
                                             N00B_GC_SCAN_ALL);
    n00b_list_t      *row   = n00b_list(n00b_type_ref());

    cell->contents   = table->title;
    cell->formatting = props;
    cell->wcache     = table->column_width_used;

    n00b_private_list_append(row, cell);

    // If there's a top border spec'd in the title's box props, then
    // we go ahead and emit it for now (eventually we should use the
    // border props in the title box props as an override (it is 100%
    // reasonable to remove the border if the table itself has the top
    // border set; that border is the same as our bottom border).
    bool side_borders = emit_top_border(table, props);
    emit_cell_row(table, row, -1, false, !side_borders);
}

static inline void
emit_caption(n00b_table_t *table)
{
    // Should completely parallel emit_title().
    if (!table->caption) {
        return;
    }

    n00b_box_props_t *props = n00b_caption_box(table);
    n00b_cell_t      *cell  = n00b_gc_alloc_mapped(n00b_cell_t,
                                             N00B_GC_SCAN_ALL);
    n00b_list_t      *row   = n00b_list(n00b_type_ref());

    cell->contents   = table->caption;
    cell->formatting = props;
    cell->wcache     = table->column_width_used;

    n00b_private_list_append(row, cell);
    emit_cell_row(table, row, -1, false, true);
}

static inline void
emit_pad(n00b_table_t *table, bool top)
{
    if (!table->outer_pad_cache) {
        return;
    }

    int n;

    if (top) {
        n = get_top_pad(n00b_outer_box(table));
    }
    else {
        n = get_bottom_pad(n00b_outer_box(table));
    }

    if (!n) {
        return;
    }

    for (int i = 0; i < n; i++) {
        core_emit(table, table->outer_pad_cache);
    }
}

static inline void
emit_top_pad(n00b_table_t *table)
{
    emit_pad(table, true);
}

static inline void
emit_bottom_pad(n00b_table_t *table)
{
    emit_pad(table, false);
}

static void
emit_cache(n00b_table_t *table, bool add_end)
{
    // This gets called whenever we've processed enough data that we
    // *could* stream (so after we've accepted any new row).
    //
    // It also gets called when we're asking to print the whole table.
    //
    // What it needs to do is cycle through any data it does have,
    // and print it.
    //
    // Then, it should either eject the cell data, or it should set
    // the output cursor for the next streaming op.
    //
    // In all cases, the pieces will call core_emit() which will
    // either do the streaming write, or buffer for the big return at
    // the end (when not streaming).

    n00b_box_props_t *outer_props = n00b_outer_box(table);

    if (!table->did_setup) {
        setup_table_rendering(table, 0); // Use terminal width.
    }

    if (!table->next_row_to_output) {
        emit_top_pad(table);
        emit_title(table);
        emit_top_border(table, outer_props);
    }

    emit_cached_rows(table);

    if (add_end) {
        emit_bottom_border(table, outer_props);
        emit_caption(table);
        emit_bottom_pad(table);
    }
}

static void *
n00b_table_format(n00b_table_t *table, n00b_string_t *options)
{
    if (options && n00b_string_codepoint_len(options)) {
        return NULL;
    }

    return n00b_table_to_string(table);
}

const n00b_vtable_t n00b_table_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_table_init,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)n00b_table_to_string,
        [N00B_BI_FORMAT]      = (n00b_vtable_entry)n00b_table_format,
        [N00B_BI_RENDER]      = (n00b_vtable_entry)n00b_table_render,
    },
};
