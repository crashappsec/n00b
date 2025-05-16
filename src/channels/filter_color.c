#include "n00b.h"

typedef struct {
    n00b_theme_t  *theme;
    n00b_string_t *s;
    int32_t        width;
    int32_t        hang;
    bool           truncate;
} colorterm_ctx;

static void *
color_setup(colorterm_ctx *ctx, void *width)
{
    ctx->width = n00b_calculate_render_width((int64_t)width);
    ctx->theme = n00b_get_current_theme();

    return NULL;
}

static n00b_list_t *
n00b_filter_add_color(colorterm_ctx *ctx, void *msg)
{
    n00b_type_t   *t            = n00b_get_my_type(msg);
    n00b_list_t   *l            = n00b_list(n00b_type_ref());
    bool           partial_line = false;
    n00b_string_t *s            = NULL;
    n00b_buf_t    *b;
    n00b_list_t   *lines;
    int            n;
    int            width;

    width = n00b_calculate_render_width(ctx->width);

    if (n00b_type_is_buffer(t)) {
        n00b_list_append(l, msg);
        return l;
    }

    // Temporary compatability.
    if (!n00b_type_is_string(t)) {
        msg = n00b_to_string(msg);
    }

    s = (n00b_string_t *)msg;

    if (n00b_string_find(s, n00b_cached_escape()) != -1) {
        n00b_list_append(l, s);
        return l;
    }

    if (!s->codepoints) {
        return NULL;
    }

    if (ctx->s) {
        assert(n00b_get_my_type(s)->base_index == N00B_T_STRING);
        s      = n00b_string_concat(ctx->s, s);
        ctx->s = NULL;
    }

    if (!n00b_string_ends_with(s, n00b_cached_newline())) {
        partial_line = true;
    }

    if (!ctx->truncate) {
        lines = n00b_string_wrap(s, width, ctx->hang);
    }
    else {
        lines = n00b_string_split(s, n00b_cached_newline());
    }

    n = n00b_list_len(lines);

    if (partial_line) {
        n      = n - 1;
        ctx->s = n00b_private_list_get(lines, n, NULL);
    }

    for (int i = 0; i < n; i++) {
        n00b_string_t *line = n00b_private_list_get(lines, i, NULL);
        int32_t        end  = line->codepoints;

        // Don't pass on empty strings or we'll get unexpected newlines.
        if (!line->codepoints) {
            continue;
        }
        if (end > width && ctx->truncate) {
            line = n00b_string_truncate(line, width);
        }

        if (end < width) {
            line = n00b_string_align_left(line, width);
        }

        b = n00b_apply_ansi_with_theme(line, ctx->theme);

        n00b_private_list_append(l, b);
    }

    return l;
}

static n00b_filter_impl color_filter = {
    .cookie_size = sizeof(colorterm_ctx),
    .setup_fn    = (void *)color_setup,
    .read_fn     = NULL,
    .write_fn    = (void *)n00b_filter_add_color,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_apply_color(int width)
{
    if (!color_filter.name) {
        color_filter.name = n00b_cstring("apply_color");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &color_filter;
    result->policy             = N00B_FILTER_WRITE;
    result->param              = (void *)(int64_t)width;

    return result;
}
