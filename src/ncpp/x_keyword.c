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
                           "[|em|][|#|][|/|]\",\n         "
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

// The below transform is for passing keywords TO a function. We keep
// state through the whole scan, tracking matched nesting of ID '('
// ... ')'. It throws away match state if it sees a preprocessor
// token, or if the nesting level goes below 0.
//
// Otherwise, when the nesting level is > 0, and we see either a '('
// or ',' followed by an ID and then a colon, the ID is converted into
// a keyword, and everything up to the next comma (or ')') is cast to
// a 64-bit int and treated as the RHS.
//
// There must not be any arguments after the last keyword parameter.
//
// This doesn't consider what function is being called or anything
// like that. It just converts spans of keywords matching the above
// pattern into a single call that acquires the keyword object.
//
// Also, this does not try to pick out function declarations
// whatsoever. If you try to use keywords in a declaration, it'll
// translate to broken C and give you some gnarly errors I'm sure.

static char *arg_fn_name = " n00b_kargs_obj(";
static char *cast        = ", (int64_t)(";
// The first paren ends the cast of the last kw_arg to an int64,
// Then there's a sentinal to tell n00b_kargs_obj when keywords
// are done.
// The second paren ends the n00b_kargs_obj call.
// The third paren ends the original call.
static char *ka_end      = "), NULL))";
// Here, the added paren ends the cast of the previous kw_arg.
static char *pkc         = "), ";

buf_t *comma_buf     = NULL;
buf_t *tri_paren     = NULL;
buf_t *post_ka_comma = NULL;

static void
open_paren_tracking(xform_t *ctx, bool id)
{
    kw_use_ctx_t *record = calloc(1, sizeof(kw_use_ctx_t));
    record->next         = ctx->kw_stack;
    record->id           = id;
    ctx->kw_stack        = record;
}

static void
reset_tracking(xform_t *ctx)
{
    kw_use_ctx_t *cur = ctx->kw_stack;

    while (cur) {
        kw_use_ctx_t *next = cur->next;
        free(cur);
        cur = next;
    }
}

static void
close_paren_tracking(xform_t *ctx, tok_t *cur)
{
    if (ctx->kw_stack) {
        kw_use_ctx_t *record = ctx->kw_stack;

        if (record->started_kobj) {
            cur->replacement = tri_paren;
        }

        ctx->kw_stack = record->next;
        free(record);
    }
}

static void
possible_keyword(xform_t *ctx, tok_t *cur)
{
    if (!ctx->kw_stack || !ctx->kw_stack->id) {
        return;
    }

    tok_t *la1 = lookahead(ctx, 1);
    tok_t *la2 = lookahead(ctx, 2);

    if (!la1 || la1->type != TT_ID) {
        goto no_match;
    }
    if (!la2 || la2->type != TT_PUNCT || ctx->input->data[la2->offset] != ':') {
        goto no_match;
    }

    if (!ctx->kw_stack->started_kobj) {
        // We're replacing the left paren here, or a comma.
        cur->replacement      = calloc(1,
                                  sizeof(buf_t) + strlen(arg_fn_name) + 3);
        char *p               = cur->replacement->data;
        // Finish the int64_t cast.
        *p++                  = ctx->input->data[cur->offset];
        cur->replacement->len = strlen(arg_fn_name) + 1;

        memcpy(p, arg_fn_name, strlen(arg_fn_name));

        if (!comma_buf) {
            comma_buf          = calloc(1, sizeof(buf_t) + strlen(cast) + 1);
            tri_paren          = calloc(1, sizeof(buf_t) + strlen(ka_end) + 1);
            post_ka_comma      = calloc(1, sizeof(buf_t) + strlen(pkc) + 1);
            comma_buf->len     = strlen(cast);
            tri_paren->len     = strlen(ka_end);
            post_ka_comma->len = strlen(pkc);
            memcpy(comma_buf->data, cast, strlen(cast));
            memcpy(tri_paren->data, ka_end, strlen(ka_end));
            memcpy(post_ka_comma->data, pkc, strlen(pkc));
        }
    }
    else {
        cur->replacement = post_ka_comma;
    }

    if (ctx->kw_stack->kw_count++) {
        if (ctx->kw_stack->kw_count > N00B_MAX_KEYWORD_SIZE) {
            fprintf(stderr,
                    "%s:%d: ERROR: Exceeded max allowed keyword args (%d)\n",
                    ctx->in_file,
                    cur->line_no,
                    N00B_MAX_KEYWORD_SIZE);
            exit(-1);
        }
    }

    ctx->kw_stack->started_kobj = true;
    la1->replacement            = calloc(1, sizeof(buf_t) + 3 + la1->len);
    la1->replacement->len       = la1->len + 2;
    la2->replacement            = comma_buf;

    char *p = la1->replacement->data;
    *p++    = '"';
    memcpy(p, ctx->input->data + la1->offset, la1->len);
    p += la1->len;
    *p = '"';

    return;

no_match:
    if (!ctx->kw_stack->started_kobj) {
        return;
    }

    fprintf(stderr,
            "%s:%d: ERROR: keywords must appear last in any function call.",
            ctx->in_file,
            la1->line_no);
    exit(-1);
}

void
kw_tracking(xform_t *ctx, tok_t *cur)
{
    if (cur->type == TT_PUNCT) {
        char c = ctx->input->data[cur->offset];

        if (c == '(') {
            tok_t *prev     = lookbehind(ctx, 1);
            int    id_prior = prev->type == TT_ID;
            open_paren_tracking(ctx, id_prior);

            if (id_prior) {
                possible_keyword(ctx, cur);
                return;
            }
        }

        if (c == ')') {
            close_paren_tracking(ctx, cur);
        }
        else {
            if (c == ',') {
                possible_keyword(ctx, cur);
            }
        }
        return;
    }
    if (cur->type == TT_PREPROC) {
        if (ctx->kw_stack && ctx->kw_stack->started_kobj) {
            fprintf(stderr,
                    "%s:%d: ERROR: Cannot put preprocessor token "
                    "inside function invocation that uses keyword arguments",
                    ctx->in_file,
                    cur->line_no);
            exit(-1);
        }
        reset_tracking(ctx);
        return;
    }
}
