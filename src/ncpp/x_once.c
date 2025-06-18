#include "ncpp.h"

// This is prefixed before anything else if we return something. The
// substitution parameter is the base function name.

char *once_cached_result_template = "static void * __n00b_%s_cached_result;\n";

// The prefix below always appears (if the result template appears, it
// is written before this).
//
// This prefix declares the lock we need, and prototypes the
// user-provided implementation, which we rename by giving it a prefix
// of `__n00b_once_func_`.
//
// In this first template, the first %s gets the base name. Second is
// the full declared type, minus `once` and, if provided, `static`.
//
// The third is the base name again, and then the fourth one is the arguments
// in the prototype, including the parens on both sides.

char *once_template_prefix =
    "static n00b_futex_t __n00b_%s_once_lock = 0;\n"
    "static %s __n00b_once_func_%s%s;\n\n";

// There are two primary templates, one for when we are expecting the
// once function to return a value, and once where we are not.  In
// either case, we will completely proxy any arguments passed to the
// wrapper down to the user provided implementation.
char *once_void_wrapper =
    "%s\n%s%s \n{\n"
    "    uint32_t val = atomic_fetch_or(&__n00b_%s_once_lock, 1);\n"
    "    switch(val) {\n"
    "        case 0:\n"
    "            __n00b_once_func_%s(%s);\n"
    "            atomic_store(&__n00b_%s_once_lock, ~0);\n"
    "            n00b_futex_wake(&__n00b_%s_once_lock, true);\n"
    "            return;\n"
    "        case 1:\n"
    "            while (atomic_read(&__n00b_%s_once_lock) != (uint32_t)~0) {\n"
    "              n00b_futex_wait(&__n00b_%s_once_lock, 1, ~0);\n"
    "            }\n"
    "            return;\n"
    "        default:\n"
    "            return;\n"
    "   }\n"
    "}\n\n"
    "static %s\n__n00b_once_func_%s%s \n";

char *once_ret_wrapper =
    "%s\n%s%s \n{\n"
    "    uint32_t val = atomic_fetch_or(&__n00b_%s_once_lock, 1);\n"
    "    switch(val) {\n"
    "        case 0:\n"
    "            n00b_gc_register_root(&__n00b_%s_cached_result, 1);\n"
    "            __n00b_%s_cached_result = \n"
    "                              (void *)(int64_t) __n00b_once_func_%s(%s);\n"
    "            atomic_store(&__n00b_%s_once_lock, ~0);\n"
    "            n00b_futex_wake(&__n00b_%s_once_lock, true);\n"
    "            return (%s)__n00b_%s_cached_result;\n"
    "        case 1:\n"
    "            while (atomic_read(&__n00b_%s_once_lock) != (uint32_t)~0) {\n"
    "              n00b_futex_wait(&__n00b_%s_once_lock, 1, ~0);\n"
    "            }\n"
    "            return (%s)__n00b_%s_cached_result;\n"
    "        default:\n"
    "            return (%s)__n00b_%s_cached_result;\n"
    "   }\n"
    "}\n"
    "static %s\n__n00b_once_func_%s%s \n";

static inline char *
extract_formals(xform_t *ctx, int lparen_ix, int rparen_ix)
{
    return extract_range(ctx, lparen_ix, rparen_ix + 1);
}

static inline char *
extract_qualifiers(xform_t *ctx, int start_ix, int end_ix)
{
    // Here, we are expected to skip over 'once', 'static', 'extern',
    // 'inline' and '_Noreturn', since they are modifiers that are not
    // part of a type.
    //
    // So we make an array and join by space at the end. It will
    // make the generated code a bit funky when we have multiple
    // consecutive stars, but whatever.
    list_t *l = list_alloc(end_ix - start_ix);
    int     n = 0;

    for (int i = start_ix; i < end_ix; i++) {
        tok_t *t    = &ctx->toks[i];
        char  *text = extract(ctx->input, t);

        if (!strcmp(text, "once") || !strcmp(text, "static")
            || !strcmp(text, "extern") || !strcmp(text, "inline")
            || !strcmp(text, "_Noreturn")) {
            free(text);
            continue;
        }

        l->items[n++] = text;
    }

    l->nitems = n;

    char *result = join(l, " ");

    for (int i = 0; i < n; i++) {
        free(l->items[i]);
    }

    while (*result == ' ' || *result == '\n') {
        result++;
    }

    n       = strlen(result);
    char *p = result + n - 1;

    while (*p == ' ' || *p == '\n') {
        *p-- = 0;
    }

    return result;
}

// Currently, we ignore `once` in a prototype; allow it to show up
// for descriptiveness, but we currently do not do the work to
// check for whether the actual matching implementation uses the
// keyword.
//
// Similarly, when we're at the implementation site, we don't worry
// about trying to match to the prototype.
void
once_xform(xform_t *ctx, tok_t *cur)
{
    int    id_ix     = -1;
    bool   void_decl = false;
    int    start_ix  = ctx->ix;
    int    probe     = start_ix;
    int    rparen_ix = -1;
    int    lparen_ix;
    tok_t *t;
    char  *s;

    // First, we need info about the return type of the function. The
    // thing about C is, the return type might be either before or
    // after modifiers like `static`, `_Atomic` or `const`.
    //
    // The easy way out is to just look for whether a function returns
    // `void` or not; if it does, we do *not* need to cache any value.
    //
    // This will work fine, unless you want to have functions that
    // return values of more than 64 bits. We do *not* do that in N00b,
    // so I'm not going to try any harder.
    //
    // So, what we do here, is look between `once` and the '(', and
    // then backward until non-star punctuation (generally will be '}'
    // or ';'), or until we hit the start-of-file.
    //
    // We want to find the whole chain anyway, so we can copy the
    // modifiers for the second declaration we need to do.
    //
    // In terms of the semantics of the modifiers, we also add a
    // 'static' declarator to the implementation that we hide; we want
    // to avoid duplicating that, but we handle that issue when
    // copying.
    //
    // This isn't to be used with the gcc __attribute__ syntax either.

    while (probe--) {
        t = &ctx->toks[probe];

        switch (t->type) {
        case TT_PREPROC:
        case TT_CHR:
        case TT_STR:
            start_ix = probe + 1;
            goto finished_backscan;
        case TT_PUNCT:
            if (ctx->input->data[t->offset] != '*') {
                start_ix = probe + 1;
                goto finished_backscan;
            }
            continue;
        case TT_ID:
            s = extract(ctx->input, t);
            if (!strcmp(s, "void")) {
                void_decl = true;
            }
            free(s);
            continue;
        default:
            continue;
        }
    }

    start_ix = 0;

finished_backscan:
    ctx->rewrite_start_ix = start_ix;

    // This next section processes up until the '('
    while (++ctx->ix < ctx->max) {
        t = &ctx->toks[ctx->ix];

        switch (t->type) {
        case TT_ID:
            s = extract(ctx->input, t);
            if (!strcmp(s, "void")) {
                void_decl = true;
            }
            else {
                // This might not *actually* be the ID. It might be
                // 'struct' or 'static' or some other modifier. But we
                // keep updating it, until we get to a '('; the ID
                // will always (necessarily) be the last one we see.
                id_ix = ctx->ix;
            }
            free(s);
            continue;
        case TT_PUNCT:
            switch (ctx->input->data[t->offset]) {
            case '(':
                goto found_paren;
            case '*':
                continue;
            default:
                return; // Syntax error; pass through.
            }
        case TT_PREPROC:
            return;
        default:
            continue;
        }
    }

    return; // Syntax error; pass through.

found_paren:
    if (id_ix == -1) {
        return;
    }
    lparen_ix = ctx->ix;
    // Now, we look for the end of the prototype, which will be either
    // the first ';' or the first '{' that we see.
    //
    // If we see a ';', we actually will ignore the prototype (letting
    // it generate). We don't need to change the external API, except
    // to erase the 'once' keyword, which was already done above.

    while (++ctx->ix < ctx->max) {
        t = &ctx->toks[ctx->ix];

        switch (ctx->input->data[t->offset]) {
        case ')':
            if (rparen_ix != -1) {
                fprintf(stderr, "Could not parse the function declaration.\n");
                exit(-1);
            }
            rparen_ix = ctx->ix;
            continue;
        case '{':
            goto found_block_start;
        case ';':
            return; // Prototype doesn't get transformed.
        default:
            continue;
        }
    }
    return; // Syntax didn't match; pass through token.

found_block_start:
    // At this point, we're using the modifier, hve got our token
    // ranges, and will apply our transformation. The user's body is
    // going to go into a function with the same root name and
    // signature, but we will add the prefix `__n00b_once_func` to the
    // name.
    //
    // We will write a function w/ the desired name to implement the
    // test and lock that we need. There are two templates for that
    // function, one for things that return a value, and one for
    // things that do not.
    //
    // We never actually output the 'once' token, if we're applying
    // this xform. When we didn't match the syntax above, we just pass
    // through, assuming `once` was used as a variable, etc.
    cur->skip_emit = true;
    t              = &ctx->toks[id_ix];

    char   *base_name   = extract(ctx->input, t);
    list_t *actual_list = get_wrapper_actuals(ctx, lparen_ix, rparen_ix);
    char   *actual_str  = join(actual_list, ", ");
    char   *formal_str  = extract_formals(ctx, lparen_ix, rparen_ix);
    char   *quals       = extract_qualifiers(ctx, start_ix, id_ix);
    int     base_len    = strlen(base_name);
    int     qual_len    = strlen(quals);
    int     formal_len  = strlen(formal_str);
    int     actual_len  = strlen(actual_str);
    int     crt_len     = 0;
    int     prefix_len;
    int     body_len;
    int     output_len;

    prefix_len = strlen(once_template_prefix) - 8; // -8 for 4 %s fields.
    prefix_len = prefix_len + 2 * base_len + qual_len + formal_len;

    if (!void_decl) {
        // First, the cached_result_template.
        crt_len  = strlen(once_cached_result_template) - 2 + base_len;
        // Now, the once_ret_wrapper
        body_len = strlen(once_ret_wrapper) - (21 * 2);
        body_len += qual_len * 5;
        body_len += base_len * 13;
        body_len += formal_len * 2;
        body_len += actual_len;
    }
    else {
        body_len = strlen(once_void_wrapper) - (13 * 2);
        body_len += qual_len * 2;
        body_len += base_len * 8;
        body_len += formal_len * 2;
        body_len += actual_len * 1;
    }

    // +1 for the null byte.
    output_len = prefix_len + crt_len + body_len + 1;

    buf_t *outbuf    = calloc(1, sizeof(buf_t) + output_len);
    char  *p         = outbuf->data;
    int    remaining = output_len - 1;
    int    n;

    outbuf->len      = output_len - 2;
    cur->replacement = outbuf;

    if (crt_len) {
        n = snprintf(p, remaining, once_cached_result_template, base_name);
        remaining -= n;
        p += n;
    }

    n = snprintf(p,
                 remaining,
                 once_template_prefix,
                 base_name,
                 quals,
                 base_name,
                 formal_str);
    remaining -= n;
    p += n;

    if (void_decl) {
        n = snprintf(p,
                     remaining,
                     once_void_wrapper,
                     quals,
                     base_name,
                     formal_str,
                     base_name,
                     base_name,
                     actual_str,
                     base_name,
                     base_name,
                     base_name,
                     base_name,
                     quals,
                     base_name,
                     formal_str);
    }
    else {
        n = snprintf(p,
                     remaining,
                     once_ret_wrapper,
                     quals,
                     base_name,
                     formal_str,
                     base_name,
                     base_name,
                     base_name,
                     base_name,
                     actual_str,
                     base_name,
                     base_name,
                     quals,
                     base_name,
                     base_name,
                     base_name,
                     quals,
                     base_name,
                     quals,
                     base_name,
                     quals,
                     base_name,
                     formal_str);
    }

    finish_rewrite(ctx, false);
}
