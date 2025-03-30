#include "n00b.h"

typedef struct {
    int32_t        width;
    int32_t        hang;
    n00b_string_t *s;
    n00b_theme_t  *theme;
    bool           truncate;
} colorterm_ctx;

static void *
n00b_flush_colorterm(n00b_stream_t *party, colorterm_ctx *ctx, void *ignore)
{
    n00b_string_t *result = ctx->s;
    ctx->s                = NULL;

    return result;
}

static void *
n00b_filter_colorterm(n00b_stream_t *party, colorterm_ctx *ctx, void *msg)
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
        // Then it's a synchronous write or other passthrough
    }

    s = (n00b_string_t *)msg;

    if (!s->codepoints) {
        return NULL;
    }

    if (ctx->s) {
        assert(n00b_get_my_type(s)->base_index == N00B_T_STRING);
        s      = n00b_string_concat(ctx->s, s);
        ctx->s = NULL;
    }

    if (!n00b_string_ends_with(s, n00b_cached_newline())
        && !n00b_string_ends_with(s, n00b_cached_cr())) {
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

void
n00b_colorterm_enable(n00b_stream_t *party,
                      size_t         width,
                      size_t         hang,
                      bool           truncate,
                      n00b_theme_t  *theme)
{
    n00b_stream_filter_t *f = n00b_new_filter((void *)n00b_filter_colorterm,
                                              (void *)n00b_flush_colorterm,
                                              n00b_cstring("colorterm"),
                                              sizeof(colorterm_ctx));

    colorterm_ctx *ctx = f->state;
    ctx->width         = width;
    ctx->hang          = hang;
    ctx->truncate      = truncate;
    ctx->theme         = theme;

    n00b_add_filter(party, f, false);
}
