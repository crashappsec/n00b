#include <sys/mman.h>
#include "ncpp.h"
#define _GNU_SOURCE

buf_t *
concat(buf_t *b, char *start, int len)
{
    int total = len;

    if (b) {
        total += b->len;
    }
    buf_t *result = calloc(1, total + sizeof(buf_t));
    char  *p      = result->data;
    result->len   = total;

    if (b) {
        memcpy(p, b->data, b->len);
        p += b->len;
        free(b);
    }

    memcpy(p, start, len);

    return result;
}

buf_t *
concat_static(buf_t *b, char *s)
{
    return concat(b, s, strlen(s));
}

buf_t *
read_file(char *fname)
{
    FILE *f = fopen(fname, "r");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END)) {
        return NULL;
    }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET)) {
        return NULL;
    }

    buf_t *result = calloc(1, sz + sizeof(buf_t));
    result->len   = fread(result->data, 1, sz, f);

    fclose(f);

    return result;
}

tok_t *
alloc_tokens(buf_t *b)
{
    int len = sizeof(tok_t) * b->len;
    int ps  = getpagesize();

    if (len % ps) {
        int pages = (len / ps) + 1;
        len       = pages * ps;
    }

    return mmap(NULL, len, MMAP_PROTS, MMAP_FLAGS, -1, 0);
}

int
count_newlines(lex_t *state, tok_t *t)
{
    int   result = 0;
    char *p;
    char *end;

    if (t->replacement) {
        p   = t->replacement->data;
        end = p + t->replacement->len;
    }
    else {
        p   = &state->input->data[t->offset];
        end = p + t->len;
    }

    while (p < end) {
        if (*p++ == '\n') {
            result++;
        }
    }

    return result;
}

// This requires being handed the index of the two bounding
// parentheses of a formal declaration (and NOT a prototype).
// Its goal is to extract the *names* of each variable, so that
// we can use them to have a wrapper invoke the ACTUAL implementation.
//
// Currently, we use this for the 'once' function only, but I know I'm
// eventually going to want to use it to support automatic compile
// time 'detours', so that we can add code to look at inputs / outputs
// for debugging purposes.
//
// There will probably be other uses too.
//
// This does a quick scan front-to-back, counting commas to determine
// how big of a list to allocate. Then, it actually works by starting
// at the end and working backword, since, for each argument, the name
// is the last thing we see.
//
// Note that this function requires each param slot to have a name,
// and so will 100% fail on functions declared varargs. This is
// intentional, since we cannot statically proxy this, even with our
// preprocessor.
//
// If we see a preproc directive in here, we also bail.
//
// We also bail if there isn't an ID where we expect to see it.
//
// We otherwise ignore stuff we don't understand.

list_t *
get_wrapper_actuals(xform_t *state, int lparen_ix, int rparen_ix)
{
    int     arg_count = 0;
    bool    found_arg = false;
    list_t *result;
    int     i;
    tok_t  *t;

    for (i = lparen_ix + 1; i < rparen_ix; i++) {
        t = &state->toks[i];

        switch (t->type) {
        case TT_ID:
            if (!found_arg) {
                found_arg = true;
                arg_count++;
            }
            continue;
        case TT_PREPROC:
            fprintf(stderr,
                    "Cannot have a preprocessor directive inside a function "
                    "declaration for something being transformed in ncpp.\n");
            exit(-1);
        case TT_PUNCT:
            if (state->input->data[t->offset] == ',') {
                if (!found_arg) {
                    fprintf(stderr, "Could not find an argument before ','\n");
                    exit(-1);
                }
                found_arg = false;
                continue;
            }
        default:
            continue;
        }
    }

    result    = list_alloc(arg_count);
    found_arg = false;

    while (arg_count && i > lparen_ix) {
        t = &state->toks[--i];
        switch (t->type) {
        case TT_ID:
            if (!found_arg) {
                found_arg                  = true;
                result->items[--arg_count] = extract(state->input, t);
                // Special case `foo(void) {`
                if (result->nitems == 1 && !strcmp(result->items[0], "void")) {
                    free(result->items[0]);
                    result->items[0] = NULL;
                    result->nitems   = 0;
                }
            }
            continue;
        case TT_WS:
        case TT_COMMENT:
            continue;
        default:
            if (!found_arg) {
missing_name:
                fprintf(stderr, "Could not find a parameter name.\n");
                exit(-1);
            }

            if (state->input->data[t->offset] == ',') {
                found_arg = false;
            }
            continue;
        }
    }
    if (arg_count) {
        goto missing_name;
    }

    return result;
}

char *
join(list_t *str_list, char *joiner)
{
    if (!str_list->nitems) {
        return calloc(1, 1);
    }

    int jlen   = strlen(joiner);
    int reslen = 0;

    for (int i = 0; i < str_list->nitems; i++) {
        reslen += strlen(str_list->items[i]);
    }

    reslen += jlen * (str_list->nitems - 1);

    char *result = calloc(1, reslen + 1);
    char *p      = result;

    int i = 0;
    int l;
    goto post_joiner;

    for (; i < str_list->nitems; i++) {
        if (jlen) {
            memcpy(p, joiner, jlen);
            p += jlen;
        }
post_joiner:
        l = strlen(str_list->items[i]);
        if (l) {
            memcpy(p, str_list->items[i], l);
            p += l;
        }
    }

    return result;
}
