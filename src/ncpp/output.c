#include "ncpp.h"

static char *
repr_type(ttype_t ty)
{
    switch (ty) {
    case TT_WS:
        return "ws";
    case TT_ID:
        return "id";
    case TT_NUM:
        return "number";
    case TT_COMMENT:
        return "comment";
    case TT_STR:
        return "string";
    case TT_CHR:
        return "char lit";
    case TT_PUNCT:
        return "punct";
    case TT_PREPROC:
        return "preproc";
    case TT_ERR:
        return "error";
    default:
        return "?";
    }
}

#define MAX_HASH_LINE_LEN 256

static inline void
write_line_directive(FILE *f, char *fname, int lineno)
{
    char buf[MAX_HASH_LINE_LEN];
    int  n = snprintf(buf,
                     MAX_HASH_LINE_LEN,
                     "#line %d \"%s\"\n",
                     lineno,
                     fname);

    write_to_file(f, buf, n);
}

static char *
repr_ws(lex_t *state, tok_t *t)
{
    int   count = 0;
    char *p     = state->input->data + t->offset;

    for (int i = 0; i < t->len; i++) {
        if (*p++ == '\n') {
            count++;
        }
    }

    if (!count) {
        return strdup("");
    }

    char buf[100];
    snprintf(buf, 100, "(%d newlines)", count);

    return strdup(buf);
}

static char *
repr_asis(lex_t *state, tok_t *t)
{
    char *p = state->input->data + t->offset;

    char *result = calloc(1, t->len + 1);
    memcpy(result, p, t->len);

    return result;
}

static char *
repr_possible_nl(lex_t *state, tok_t *t)
{
    int   count = 0;
    char *p     = state->input->data + t->offset;

    for (int i = 0; i < t->len; i++) {
        if (*p++ == '\n') {
            count++;
        }
    }

    if (count) {
        return strdup("<<multi-line value>>");
    }

    return repr_asis(state, t);
}

static char *
repr_tok(lex_t *state, tok_t *t)
{
    switch (t->type) {
    case TT_WS:
        return repr_ws(state, t);
    case TT_ID:
    case TT_NUM:
    case TT_STR:
    case TT_CHR:
    case TT_PUNCT:
        return repr_asis(state, t);
    default:
        return repr_possible_nl(state, t);
    }
}

void
print_tokens(lex_t *state)
{
    for (int i = 0; i < state->num_toks; i++) {
        tok_t *t = &state->toks[i];

        char *s1 = repr_type(t->type);
        char *s2 = repr_tok(state, t);
        printf("%04d: %s: %s (line %d)\n", i, s1, s2, t->line_no);
        free(s2); // s1 is always static.
    }
}

bool
output_new_file(lex_t *state, char *fname)
{
    // Start w/ false so if we copy out the gate, we add a line directive.
    bool  using_in_file = false;
    int   out_line_no   = 1;
    FILE *f;

    if (!strcmp(fname, "-")) {
        f = stdout;
    }
    else {
        f = fopen(fname, "w");
    }
    if (!f) {
        return false;
    }

    for (int i = 0; i < state->num_toks; i++) {
        tok_t *t = &state->toks[i];

        if (t->replacement) {
            if (using_in_file) {
                using_in_file = false;
                write_line_directive(f, state->out_file, ++out_line_no);
            }
            out_line_no += count_newlines(state, t);
            if (!write_to_file(f, t->replacement->data, t->replacement->len)) {
                return false;
            }

            continue;
        }

        if (t->skip_emit) {
            continue;
        }

        if (!using_in_file) {
            using_in_file = true;
            write_line_directive(f, state->in_file, t->line_no);
            out_line_no += 1;
        }

        out_line_no += count_newlines(state, t);

        if (!write_to_file(f, &state->input->data[t->offset], t->len)) {
            return false;
        }
    }

    return true;
}

bool
write_to_file(FILE *f, char *p, int len)
{
    while (len) {
        int l = fwrite(p, 1, len, f);

        if (!l) {
            return false;
        }

        p += l;
        len -= l;
    }

    return true;
}
