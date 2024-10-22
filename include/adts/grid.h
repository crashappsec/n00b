#pragma once
#include "n00b.h"

static inline n00b_renderable_t **
n00b_cell_address(n00b_grid_t *g, int row, int col)
{
    return &g->cells[g->num_cols * row + col];
}

static inline n00b_utf8_t *
n00b_get_th_tag(n00b_grid_t *g)
{
    if (g->th_tag_name != NULL) {
        return g->th_tag_name;
    }
    return n00b_new_utf8("th");
}

static inline n00b_utf8_t *
n00b_get_td_tag(n00b_grid_t *g)
{
    if (g->td_tag_name != NULL) {
        return g->td_tag_name;
    }
    return n00b_new_utf8("td");
}
void n00b_grid_set_all_contents(n00b_grid_t *, n00b_list_t *);

extern n00b_grid_t  *n00b_grid_flow(uint64_t items, ...);
extern n00b_grid_t  *n00b_grid_flow_from_list(n00b_list_t *);
extern n00b_grid_t  *n00b_callout(n00b_str_t *s);
extern n00b_utf32_t *n00b_grid_to_str(n00b_grid_t *);
extern n00b_grid_t  *_n00b_ordered_list(n00b_list_t *, ...);
extern n00b_grid_t  *_n00b_unordered_list(n00b_list_t *, ...);
extern n00b_grid_t  *_n00b_grid_tree(n00b_tree_node_t *, ...);
extern n00b_grid_t  *_n00b_grid_tree_new(n00b_tree_node_t *, ...);
extern n00b_list_t  *_n00b_grid_render(n00b_grid_t *, ...);
extern void         n00b_set_column_props(n00b_grid_t *,
                                         int,
                                         n00b_render_style_t *);
extern void         n00b_row_column_props(n00b_grid_t *,
                                         int,
                                         n00b_render_style_t *);
extern void         n00b_set_column_style(n00b_grid_t *, int, n00b_utf8_t *);
extern void         n00b_set_row_style(n00b_grid_t *, int, n00b_utf8_t *);

#define n00b_grid_render(g, ...)    _n00b_grid_render(g, N00B_VA(__VA_ARGS__))
#define n00b_ordered_list(l, ...)   _n00b_ordered_list(l, N00B_VA(__VA_ARGS__))
#define n00b_unordered_list(l, ...) _n00b_unordered_list(l, N00B_VA(__VA_ARGS__))
#define n00b_grid_tree(t, ...)      _n00b_grid_tree(t, N00B_VA(__VA_ARGS__))
#define n00b_grid_tree_new(t, ...)  _n00b_grid_tree_new(t, N00B_VA(__VA_ARGS__))

void
n00b_grid_add_col_span(n00b_grid_t       *grid,
                      n00b_renderable_t *contents,
                      int64_t           row,
                      int64_t           start_col,
                      int64_t           num_cols);

static inline n00b_renderable_t *
n00b_to_str_renderable(n00b_str_t *s, n00b_utf8_t *tag)
{
    return n00b_new(n00b_type_renderable(),
                   n00b_kw("obj", n00b_ka(s), "tag", n00b_ka(tag)));
}

static inline n00b_style_t
n00b_grid_blend_color(n00b_style_t style1, n00b_style_t style2)
{
    // We simply do a linear average of the colors.
    return ((style1 & ~N00B_STY_CLEAR_FG) + (style2 & ~N00B_STY_CLEAR_FG)) >> 1;
}

extern bool        n00b_install_renderable(n00b_grid_t *,
                                          n00b_renderable_t *,
                                          int,
                                          int,
                                          int,
                                          int);
extern void        n00b_apply_container_style(n00b_renderable_t *, n00b_utf8_t *);
extern void        n00b_grid_expand_columns(n00b_grid_t *, uint64_t);
extern void        n00b_grid_expand_rows(n00b_grid_t *, uint64_t);
extern void        n00b_grid_add_row(n00b_grid_t *, n00b_obj_t);
extern n00b_grid_t *n00b_grid(int,
                            int,
                            n00b_utf8_t *,
                            n00b_utf8_t *,
                            n00b_utf8_t *,
                            int,
                            int,
                            int);
extern n00b_grid_t *n00b_grid_horizontal_flow(n00b_list_t *,
                                            uint64_t,
                                            uint64_t,
                                            n00b_utf8_t *,
                                            n00b_utf8_t *);

extern n00b_grid_t *n00b_new_cell(n00b_str_t *, n00b_utf8_t *);

extern void n00b_grid_set_cell_contents(n00b_grid_t *, int, int, n00b_obj_t);

static inline void
n00b_grid_add_cell(n00b_grid_t *grid, n00b_obj_t container)
{
    n00b_grid_set_cell_contents(grid,
                               grid->row_cursor,
                               grid->col_cursor,
                               container);
}

static inline void
n00b_grid_add_rows(n00b_grid_t *grid, n00b_list_t *l)
{
    int n = n00b_list_len(l);

    for (int i = 0; i < n; i++) {
        n00b_list_t *row = n00b_list_get(l, i, NULL);

        if (row) {
            n00b_grid_add_row(grid, row);
        }
    }
}

static inline void
n00b_grid_stripe_rows(n00b_grid_t *grid)
{
    grid->stripe = 1;
}

static inline n00b_list_t *
n00b_new_table_row()
{
    return n00b_new(n00b_type_list(n00b_type_utf32()));
}

extern void _n00b_print(n00b_obj_t, ...);

static inline void
n00b_debug_callout(char *s)
{
    _n00b_print(n00b_callout(n00b_new_utf8(s)), 0);
}

extern void n00b_enforce_container_style(n00b_obj_t, n00b_utf8_t *, bool);
