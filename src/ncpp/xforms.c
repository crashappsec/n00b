#include "ncpp.h"

static inline bool
try_id_xforms(xform_t *ctx)
{
    tok_t *t = &ctx->toks[ctx->ix];

    if (id_check("keywords", ctx->input, t->offset, t->len)) {
        ctx->rewrite_start_ix = ctx->ix;
        return keyword_xform(ctx, t);
    }

    if (id_check("once", ctx->input, t->offset, t->len)) {
        // The xform sets the rewrite start; it scans backwards so that
        // `once` may appear in any reasonable order.
        once_xform(ctx, t);
        return true;
    }

    ctx->ix++;
    return true;
}

bool
apply_transforms(lex_t *state)
{
    xform_t ctx = {
        .input   = state->input,
        .toks    = state->toks,
        .ix      = 0,
        .max     = state->num_toks,
        .in_file = state->in_file,
    };

    while (ctx.ix < ctx.max) {
        switch (ctx.toks[ctx.ix].type) {
        case TT_ID:
            if (!try_id_xforms(&ctx)) {
                return false;
            }
            continue;
        default:
            kw_tracking(&ctx, &ctx.toks[ctx.ix]);
            ctx.ix++;
        }
    }

    return true;
}

void
finish_rewrite(xform_t *ctx, bool include_comments)
{
    for (int i = ctx->rewrite_start_ix; i < ctx->ix; i++) {
        if (!include_comments || ctx->toks[i].type != TT_COMMENT) {
            ctx->toks[i].skip_emit = true;
        }
    }
}

char *
extract_line(xform_t *ctx, tok_t *t)
{
    char *start = ctx->input->data + t->offset;
    char *end   = ctx->input->data + ctx->input->len;
    char *p     = start;

    while (++p < end) {
        if (*p == '\n') {
            break;
        }
    }

    char *result = calloc(1, p - start + 1);
    memcpy(result, start, p - start);

    return result;
}

char *
extract_range(xform_t *ctx, int start_ix, int end_ix)
{
    int    start_offset;
    int    end_offset;
    tok_t *st = &ctx->toks[start_ix];

    start_offset = st->offset;

    if (end_ix >= ctx->max) {
        end_offset = ctx->input->len;
    }
    else {
        st         = &ctx->toks[end_ix];
        end_offset = st->offset;
    }

    int   len    = end_offset - start_offset;
    char *result = calloc(1, len + 1);
    memcpy(result, &ctx->input->data[start_offset], len);

    return result;
}

tok_t *
skip_forward_to_punct(xform_t *ctx, char punc)
{
    tok_t *t = advance(ctx, true);
    while (t) {
        if (t->type == TT_PUNCT) {
            if (ctx->input->data[t->offset] == punc) {
                return t;
            }
        }
        t = advance(ctx, true);
    }
    return NULL;
}
