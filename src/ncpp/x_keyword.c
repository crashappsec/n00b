#include "ncpp.h"

static inline char *
kw_param_name_elipsis(xform_t *ctx, int ix)
{
    tok_t *t;

    if (ix < 8) {
        return NULL;
    }

    // Current ix we know is properly a dot. Previous two tokens
    // must be dots. We confirm the current one first.

    t = &ctx->toks[ix--];
    if (ctx->input->data[t->offset] != '.') {
        return NULL;
    }
    t = &ctx->toks[ix--];
    if (t->type != TT_PUNCT || ctx->input->data[t->offset] != '.') {
        return NULL;
    }
    t = &ctx->toks[ix--];
    if (t->type != TT_PUNCT || ctx->input->data[t->offset] != '.') {
        return NULL;
    }
    t = &ctx->toks[ix--];
    if (t->type == TT_WS) {
        t = &ctx->toks[ix--];
    }
    if (t->type != TT_PUNCT || ctx->input->data[t->offset] != ',') {
        return NULL;
    }
    t = &ctx->toks[ix--];
    if (t->type == TT_WS) {
        t = &ctx->toks[ix];
    }
    if (t->type != TT_ID) {
        return NULL;
    }
    return extract(ctx->input, t);
}

static inline char *
kw_param_name_fixed(xform_t *ctx, int ix, bool *va)
{
    char *name;

    *va = false;

    if (ix < 8) {
        return NULL;
    }

    tok_t *id_tok = &ctx->toks[ix--];

    if (id_tok->type != TT_ID) {
        return NULL;
    }

    tok_t *star = &ctx->toks[ix--];

    if (star->type == TT_WS) {
        star = &ctx->toks[ix--];
    }
    if (star->type != TT_PUNCT || ctx->input->data[star->offset] != '*') {
        if (star->type == TT_ID) {
            name = extract(ctx->input, star);
            if (!strcmp(name, "va_list")) {
                *va = true;
                free(name);
                return extract(ctx->input, id_tok);
            }
        }
        return NULL;
    }

    tok_t *tname = &ctx->toks[ix--];
    if (tname->type == TT_WS) {
        tname = &ctx->toks[ix--];
    }

    name = extract(ctx->input, tname);

    if (!strcmp(name, "n00b_karg_info_t") || !strcmp(name, "void")) {
        free(name);

        return extract(ctx->input, id_tok);
    }

    free(name);
    return NULL;
}

// This appends its declaration (which it returns), and then
// creates a new buffer for the check within the loop, which is
// passed through the second parameter.
static buf_t *
next_kw_param(buf_t *decl_buf, buf_t **loop_buf, xform_t *ctx)
{
    // This implements kw_list below, and translates it into both a
    // variable declaration, and a test / assign.
    if (ctx->ix >= ctx->max) {
        return NULL;
    }

    tok_t *start = cur_tok(ctx);
    tok_t *cur;
    tok_t *last_id;
    bool   id_found  = false;
    bool   got_star  = false;
    bool   got_paren = false;
    bool   id_done   = false;
    char  *kw_name   = NULL;
    buf_t *type_name; // Stashed for the cast.
    char  *var_name;
    char  *msg;

    // This loop is a state machine for everything up to the assignment
    // operator.
    while (true) {
        cur = advance(ctx, true);

        if (!cur) {
            fprintf(stderr, "End of file in keyword block.\n");
            return NULL;
        }

        if (cur->type == TT_ID) {
            id_found = true;
            last_id  = cur;
            if (got_star) {
                id_done = true;
            }
            continue;
        }

        if (cur->type != TT_PUNCT) {
            goto syntax_err;
        }

        switch (ctx->input->data[cur->offset]) {
        case '(':
            if (got_paren || !id_found || (got_star && !id_done)) {
                goto syntax_err;
            }
            cur = advance(ctx, true);
            if (cur->type != TT_ID) {
                goto syntax_err;
            }
            kw_name = extract(ctx->input, cur);
            cur     = advance(ctx, true);
            if (cur->type != TT_PUNCT || ctx->input->data[cur->offset] != ')') {
                goto syntax_err;
            }
            got_paren = true;
            id_done   = true;
            continue;
        case ';':
            fprintf(stderr,
                    "Keyword parameters must be assigned default values.\n");
            return NULL;
        case '=':
            if (ctx->ix >= ctx->max) {
                goto syntax_err;
            }
            if (id_done || (id_found && !got_star)) {
                break;
            }
            goto syntax_err;
        case '*':
            if (id_done || got_paren) {
                goto syntax_err;
            }
            got_star = true;
            continue;
        default:
syntax_err:
            msg = extract_line(ctx, start);
            fprintf(stderr,
                    "Invalid syntax for keyword block for keyword: %s.\n",
                    msg);
            free(msg);
            return NULL;
        }

        break;
    }
    int l     = last_id->offset - start->offset;
    var_name  = extract(ctx->input, last_id);
    type_name = concat(NULL, ctx->input->data + start->offset, l);

    if (!kw_name) {
        kw_name = var_name;
    }

    // Add a declaration for the "__found_kw" variable.

    decl_buf = concat_static(decl_buf, "    bool __found_");
    decl_buf = concat_static(decl_buf, kw_name);
    decl_buf = concat_static(decl_buf, " = false;\n");

    // Copy in both the type name and the variable name, without
    // freeing our stash of the type name (which we use to add
    // the appropriate casting on assignment).

    decl_buf = concat_static(decl_buf, "    ");
    l        = l + last_id->len;
    decl_buf = concat(decl_buf, ctx->input->data + start->offset, l);
    decl_buf = concat_static(decl_buf, " ");

    start = &ctx->toks[ctx->ix];
    skip_forward_to_punct(ctx, ';');
    cur = cur_tok(ctx);
    // now advance past the semicolon.

    if (!cur) {
        return NULL;
    }

    // Add one to capture the semicolon.
    l        = cur->offset - start->offset + 1;
    decl_buf = concat(decl_buf, ctx->input->data + start->offset, l);
    decl_buf = concat_static(decl_buf, "\n\n");

    // Cool, we're done with the declaration portion of the game. Time
    // to put together the kw check / assignment portion.

    buf_t *b = concat_static(NULL, "            if (!strcmp(__ka->kw, \"");
    b        = concat_static(b, kw_name);
    b        = concat_static(b, "\")) {\n\n                if(__found_");
    b        = concat_static(b, kw_name);
    b        = concat_static(b,
                      ") {\n                    goto __dupe_kw_found;\n"
                             "                }\n\n                __found_");
    b        = concat_static(b, kw_name);
    b        = concat_static(b, " = true;\n                ");
    b        = concat_static(b, var_name);
    b        = concat_static(b, " = (");
    b        = concat(b, type_name->data, type_name->len);

    free(type_name);

    if (!got_star) {
        b = concat_static(b,
                          ")(int64_t)__ka->value;\n                continue;\n"
                          "            }\n");
    }
    else {
        b = concat_static(b,
                          ")__ka->value;\n                continue;\n"
                          "            }\n");
    }

    *loop_buf = b;

    advance(ctx, true);

    return decl_buf;
}

// Okay, the grammar for this transform is:
//
// kw ::= 'keywords' WS '{' kw_list '}'
// kw_list ::= (ID | '*')* ID kw_name_if_different? '=' .* ';' kw_list?
// kw_name_if_different ::= '(' ID ')';
//
// The keyword name defaults to the variable name; the override
// is for odd name collisions.
//
// This transform also scans backward to the last right paren, and
// looks at the preceding (non-ws) token. If it's an ID, it looks to
// see if it's va_list. If not, it ensures it's either of type `void
// *` or it's of type `n00b_karg_info_t *`.
//
// Otherwise, it scans back to the comma, and grabs the previous
// variable name to set up the va_arg properly.

bool
keyword_xform(xform_t *ctx, tok_t *start)
{
    int    ix              = scan_back(ctx, ctx->ix - 1, TT_PUNCT, ")");
    bool   decl_va_list    = false;
    char  *last_param_name = NULL;
    bool   va              = false;
    buf_t *result          = NULL;
    buf_t *inner           = NULL;
    tok_t *t;

    do {
        ix--;
        if (ix < 0) {
            fprintf(stderr,
                    "Error: Couldn't find end of params for keywords\n");
            return false;
        }

        t = &ctx->toks[ix];
    } while (t->type == TT_WS); // Only should jump to top 0-1 times.

    if (t->type == TT_PUNCT) {
        decl_va_list    = true;
        last_param_name = kw_param_name_elipsis(ctx, ix);
    }
    else {
        last_param_name = kw_param_name_fixed(ctx, ix, &va);
    }

    if (!last_param_name) {
        fprintf(stderr,
                "Last parameter of function declaration must "
                "either be followed by an elipsis, or must be declared "
                "va_list, `n00b_karg_info_t *` (or `void *`)\n");
        return false;
    }

    result = concat_static(result, "\n    n00b_string_t *__err;\n    ");
    if (decl_va_list) {
        result          = concat_static(result,
                               "va_list __n00b_va;\n\n"
                                        "    va_start(__n00b_va, ");
        result          = concat_static(result, last_param_name);
        result          = concat_static(result, ");\n");
        va              = true;
        last_param_name = "__n00b_va";
    }

    if (va) {
        result          = concat_static(result,
                               "\n    n00b_karg_info_t *__n00b_ka_info = "
                                        "va_arg(");
        result          = concat_static(result, last_param_name);
        result          = concat_static(result, ", void *);\n");
        last_param_name = "__n00b_ka_info";
    }

    // Expecting a left brace.
    t = advance(ctx, true);
    if (!t || t->type != TT_PUNCT) {
        return false;
    }
    if (ctx->input->data[t->offset] != '{') {
        return false;
    }

    // Declarations will come before the loop.
    inner = concat_static(NULL, "\n    if (");
    inner = concat_static(inner, last_param_name);
    inner = concat_static(inner,
                          " != NULL) {\n"
                          "        for (int __i = 0; __i < ");
    inner = concat_static(inner, last_param_name);
    inner = concat_static(inner,
                          "->num_provided; __i++) {\n"
                          "            n00b_one_karg_t *__ka = &");
    inner = concat_static(inner, last_param_name);
    inner = concat_static(inner, "->args[__i];\n");

    // Skip past the {
    advance(ctx, true);

    while (true) {
        buf_t *check;

        result = next_kw_param(result, &check, ctx);

        if (!result) {
            return false;
        }

        inner = concat(inner, check->data, check->len);
        free(check);

        t = cur_tok(ctx);

        if (!t) {
            return false;
        }
        if (t->type == TT_PUNCT && ctx->input->data[t->offset] == '}') {
            break;
        }
    }

    // Since we're done w/ keywords, we can combine our two pieces.
    result = concat(result, inner->data, inner->len);
    result = concat_static(result,
                           "            __err  = "
                           "n00b_cformat(\"Invalid keyword param: "
                           "[=em=][=#=][=/=]\",\n         "
                           "              n00b_cstring(__ka->kw));\n"
                           "            N00B_RAISE(__err);\n"
                           "        __dupe_kw_found:\n"
                           "            __err = "
                           "n00b_cformat(\"Duplicate keyword provided: "
                           "[=em=][=#=][=/=]\",\n        "
                           "             n00b_cstring(__ka->kw));\n"
                           "            N00B_RAISE(__err);\n"
                           "        }\n    }\n");

    start->replacement = result;
    advance(ctx, false);
    finish_rewrite(ctx, false);

    return true;
}
