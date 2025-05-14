#include "n00b.h"

static n00b_list_t *
ansi_parse(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    n00b_list_t *result = n00b_list(n00b_type_list(n00b_type_ref()));

    n00b_ansi_parse(ctx, b);

    n00b_list_t      *nodes = ctx->results;
    n00b_ansi_node_t *last  = n00b_list_pop(nodes);

    if (!last) {
        return result;
    }

    n00b_private_list_append(result, nodes);

    ctx->results = n00b_list(n00b_type_ref());

    if (last->kind == N00B_ANSI_PARTIAL) {
        n00b_private_list_append(ctx->results, last);
    }
    else {
        n00b_private_list_append(nodes, last);
    }

    return result;
}

static n00b_list_t *
ansi_flush(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    n00b_list_t *result;

    if (b) {
        result = ansi_parse(ctx, b);
    }
    else {
        result = n00b_list(n00b_type_ref());
    }

    void *item = n00b_private_list_pop(ctx->results);

    if (item) {
        n00b_private_list_append(result, item);
    }

    return result;
}

static n00b_list_t *
ansi_strip(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    n00b_list_t   *result = n00b_list(n00b_type_string());
    n00b_list_t   *nodes  = ansi_parse(ctx, b);
    n00b_string_t *s      = n00b_ansi_nodes_to_string(nodes, false);

    if (s) {
        n00b_private_list_append(result, s);
    }

    return result;
}

static n00b_list_t *
ansi_strip_flush(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    n00b_list_t *result = NULL;

    if (b) {
        result = ansi_strip(ctx, b);
    }

    // This will nuke any partial on flush.
    n00b_list_pop(ctx->results);

    return result;
}

static void *
ansi_setup(n00b_ansi_ctx *ctx, void *unused)
{
    ctx->results = n00b_list(n00b_type_ref());

    return NULL;
}

static n00b_filter_impl parse_ansi = {
    .setup_fn    = (void *)ansi_setup,
    .cookie_size = sizeof(n00b_ansi_ctx),
    .flush_fn    = (void *)ansi_flush,
    .read_fn     = (void *)ansi_parse,
    .write_fn    = (void *)ansi_parse,
    .name        = NULL,
};

static n00b_filter_impl strip_ansi = {
    .setup_fn    = (void *)ansi_setup,
    .cookie_size = sizeof(n00b_ansi_ctx),
    .flush_fn    = (void *)ansi_strip_flush,
    .read_fn     = (void *)ansi_strip,
    .write_fn    = (void *)ansi_strip,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_parse_ansi(bool on_read)
{
    if (!parse_ansi.name) {
        parse_ansi.name = n00b_cstring("parse_ansi");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &parse_ansi;
    result->policy             = on_read ? N00B_FILTER_READ : N00B_FILTER_WRITE;

    return result;
}

n00b_filter_spec_t *
n00b_filter_strip_ansi(bool on_read)
{
    if (!strip_ansi.name) {
        strip_ansi.name = n00b_cstring("strip_ansi");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc(n00b_filter_spec_t);
    result->impl               = &strip_ansi;
    result->policy             = on_read ? N00B_FILTER_READ : N00B_FILTER_WRITE;

    return result;
}
