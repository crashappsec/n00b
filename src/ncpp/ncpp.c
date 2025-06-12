#if 0
void
test_func(int arg, ...) {
    keywords {
	int64_t test_kw        = 0;
	int64_t test2_kw(blah) = 100;
	char *idfk             = NULL;
    }



}
#endif

// I do a lot of preprocessor gunk to try to improve the usability of
// writing C for myself, but sometimes the results aren't good enough.
//
// Doing a full-baked proper pre-processor is way too much work, but I
// finally decided to do something that's a bit janky just for this
// project.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>

typedef struct {
    int64_t len;
    char    data[];
} buf_t;

typedef int8_t ttype_t;

#define TT_ERR     -1
#define TT_WS      0
#define TT_ID      1
#define TT_NUM     2
#define TT_COMMENT 3
#define TT_STR     4
#define TT_CHR     5
#define TT_PUNCT   6
#define TT_PREPROC 7
#define TT_UNKNOWN 8

typedef struct {
    buf_t   *replacement;
    ttype_t  type;
    uint16_t offset;
    uint16_t len;
    // Tokens we choose to rewrite get "skipped", but the "replacement"
    // text will all live in the first token of the rewrite.
    uint8_t  skip_emit : 1;
} tok_t;

typedef struct {
    int    num_toks;
    buf_t *input;
    tok_t *toks;
    int    offset;
    char  *cur;
    char  *end;
    bool   line_start;
} lex_t;

typedef struct {
    buf_t *input;
    tok_t *toks;
    int    ix;
    int    max;
    int    rewrite_start_ix;
} xform_t;

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

tok_t *
alloc_tokens(buf_t *b)
{
    int len = sizeof(tok_t) * b->len;
    int ps  = getpagesize();

    if (len % ps) {
        int pages = (len / ps) + 1;
        len       = pages * ps;
    }

    return mmap(NULL,
                len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                -1,
                0);
}

// This is by no means a full C lexer; it lexes just what we need, and
// assumes the subset of C we use.
//
// Specifically, we do not match identifiers that have either \u or \U
// in them.
//
// Here's what happens:
//
// 1. We do enough to make sure we can accurately identify the bounds
//    of strings, char literals, comments and preprocessor directive
//    lines, but no more.
//
// 2. We aggregate whitespace.
//
// 3. We do NOT fully lex numbers to the standard. Instead, we take
//    advantage of the fact that stylistically, we always properly set
//    off numbers with punctuation or spaces. Basically, we treat
//    numbers and identifiers the same way, and differentiate which
//    kind of token it is by the first character.
//
// 4. We lex known punctuation as individual characters, and do not
//    worry about grouping them into full C tokens (transforms can do
//    it if they like).
//
// 5. Everything else gets marked as "unknown", one character at a time.
//
// 6. We only explicitly handle the preprocessor "splice" within
//    preprocessor directives. In C, it's valid anywhere, but we don't
//    use it anywhere else in N00b, so I don't handle it.
//
// 7. We don't distinguish between keywords and identifiers.
//
// 8. String encoding prefixes are ignored, since we don't use them.

static inline void
tok_punct(lex_t *state)
{
    state->line_start              = false;
    state->toks[state->num_toks++] = (tok_t){
        .type   = TT_PUNCT,
        .offset = state->offset++,
        .len    = 1,
    };

    state->cur++;
}

static inline void
tok_unk(lex_t *state)
{
    state->line_start              = false;
    state->toks[state->num_toks++] = (tok_t){
        .type   = TT_PUNCT,
        .offset = state->offset++,
        .len    = 1,
    };

    state->cur++;
}

static inline void
tok_char(lex_t *state)
{
    state->line_start = false;
    tok_t *t          = &state->toks[state->num_toks++];

    *t = (tok_t){
        .type   = TT_CHR,
        .offset = state->offset,
    };

    char *p = state->cur + 1;

    while (p < state->end) {
        switch (*p) {
        case '\'':
            p++;
            t->len = p - state->cur;
            state->offset += p - state->cur;
            state->cur = p;
            return;
        case '\\':
            p++;
            p++;
            continue;
        case '\n':
            t->type = TT_ERR;
            t->len  = p - state->cur;
            state->offset += p - state->cur;
            state->cur = p;
            return;
        default:
            p++;
            continue;
        }
    }

    t->type = TT_ERR;
    t->len  = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
    return;
}

static inline void
tok_string(lex_t *state)
{
    state->line_start = false;
    tok_t *t          = &state->toks[state->num_toks++];

    *t = (tok_t){
        .type   = TT_STR,
        .offset = state->offset,
    };

    char *p = state->cur + 1;

    while (p < state->end) {
        switch (*p) {
        case '"':
            p++;
            t->len = p - state->cur;
            state->offset += p - state->cur;
            state->cur = p;
            return;
        case '\\':
            p++;
            p++;
            continue;
        case '\n':
            t->type = TT_ERR;
            t->len  = p - state->cur;
            state->offset += p - state->cur;
            state->cur = p;
            return;
        default:
            p++;
            continue;
        }
    }

    t->type = TT_ERR;
    t->len  = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
    return;
}

static inline bool
is_id_char(char c)
{
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '_':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
        return true;
    default:
        return false;
    }
}

static inline void
tok_id_or_num(lex_t *state, bool num)
{
    state->line_start = false;
    tok_t *t          = &state->toks[state->num_toks++];
    char  *p          = state->cur;

    *t = (tok_t){
        .type   = num ? TT_NUM : TT_ID,
        .offset = state->offset,
    };

    while (++p < state->end) {
        if (!is_id_char(*p)) {
            break;
        }
    }

    t->len = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
}

static inline void
tok_ws(lex_t *state)
{
    tok_t *t = &state->toks[state->num_toks++];
    char  *p = state->cur;

    *t = (tok_t){
        .type   = TT_WS,
        .offset = state->offset,
    };

    while (++p < state->end) {
        switch (*p) {
        case ' ':
        case '\t':
            continue;
        case '\n':
            state->line_start = true;
            continue;
        default:
            break;
        }
        break;
    }

    t->len = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
}

static inline void
tok_preproc(lex_t *state)
{
    tok_t *t = &state->toks[state->num_toks++];
    char  *p = state->cur;

    *t = (tok_t){
        .type   = TT_PREPROC,
        .offset = state->offset,
    };

    while (++p < state->end) {
        switch (*p) {
        case '\\':
            ++p;
            continue;
        case '\n':
            ++p; // Count this in the preproc statement.
            break;
        default:
            continue;
        }
        break;
    }

    t->len = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
}

static inline void
line_comment(lex_t *state)
{
    tok_t *t = &state->toks[state->num_toks++];
    char  *p = state->cur;

    *t = (tok_t){
        .type   = TT_COMMENT,
        .offset = state->offset,
    };

    while (++p < state->end) {
        if (*p == '\n') {
            p++;
            break;
        }
    }
    t->len = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
}

static inline void
match_comment(lex_t *state)
{
    tok_t *t = &state->toks[state->num_toks++];
    char  *p = state->cur;

    *t = (tok_t){
        .type   = TT_COMMENT,
        .offset = state->offset,
    };

    while (++p < state->end - 1) {
        if (*p == '*' && p[1] == '/') {
            p++;
            p++;
            t->len = p - state->cur;
            state->offset += p - state->cur;
            state->cur = p;
            return;
        }
    }

    t->len = p - state->cur;
    state->offset += p - state->cur;
    state->cur = p;
    t->type    = TT_ERR;
}

static inline void
tok_comment_or_punct(lex_t *state)
{
    if (state->cur + 1 == state->end) {
        tok_punct(state);
        return;
    }
    switch (state->cur[1]) {
    case '/':
        line_comment(state);
        return;
    case '*':
        match_comment(state);
        return;
    default:
        tok_punct(state);
        return;
    }
}

char *
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

char *
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

char *
repr_asis(lex_t *state, tok_t *t)
{
    char *p = state->input->data + t->offset;

    char *result = calloc(1, t->len + 1);
    memcpy(result, p, t->len);

    return result;
}

char *
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

char *
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
        printf("%04d: %s: %s\n", i, s1, s2);
        free(s2); // s1 is always static.
    }
}

void
lex(lex_t *state)
{
    state->toks       = alloc_tokens(state->input);
    state->num_toks   = 0;
    state->cur        = state->input->data;
    state->end        = state->cur + state->input->len;
    //    state->line_no    = 1;
    state->line_start = true;

    while (state->cur < state->end) {
        switch (*state->cur) {
        case ' ':
        case '\t':
        case '\n':
            tok_ws(state);
            continue;
        case '#':
            if (state->line_start) {
                tok_preproc(state);
            }
            else {
                tok_punct(state);
            }
            continue;
        case '/':
            tok_comment_or_punct(state);
            continue;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            tok_id_or_num(state, true);
            continue;
        case '_':
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
            tok_id_or_num(state, false);
            continue;
        case '"':
            tok_string(state);
            continue;
        case '\'':
            tok_char(state);
            continue;
        case '~':
        case '!':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '{':
        case '[':
        case '}':
        case ']':
        case '|':
        case ':':
        case ';':
        case '<':
        case ',':
        case '>':
        case '.':
        case '?':
            tok_punct(state);
            continue;
        default:
            tok_unk(state);
        }
    }
}

bool
output_new_file(lex_t *state, char *fname)
{
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
            if (!write_to_file(f, t->replacement->data, t->replacement->len)) {
                return false;
            }
            continue;
        }

        if (t->skip_emit) {
            continue;
        }

        if (!write_to_file(f, &state->input->data[t->offset], t->len)) {
            return false;
        }
    }

    return true;
}

bool
id_check(char *to_match, buf_t *input, int offset, int len)
{
    if (strlen(to_match) != (size_t)len) {
        return false;
    }

    return !(strncmp(to_match, input->data + offset, len));
}

char *
extract(buf_t *input, tok_t *tok)
{
    char *p      = input->data + tok->offset;
    char *result = calloc(1, tok->len + 1);

    memcpy(result, p, tok->len);

    return result;
}

int
scan_back(xform_t *ctx, int tok_ix, ttype_t type, char *match)
{
    while (tok_ix--) {
        tok_t *t = &ctx->toks[tok_ix];

        if (t->type != type) {
            continue;
        }

        char *part = extract(ctx->input, t);
        if (!strcmp(part, match)) {
            free(part);
            return tok_ix;
        }

        free(part);
    }

    return -1;
}

tok_t *
advance(xform_t *ctx, bool skip_ws)
{
    tok_t *t;

    while (true) {
        if (ctx->ix >= ctx->max) {
            return NULL;
        }

        t = &ctx->toks[++ctx->ix];

        if (t->type == TT_COMMENT) {
            continue;
        }
        if (skip_ws && t->type == TT_WS) {
            continue;
        }

        return t;
    }
}

tok_t *
cur_tok(xform_t *ctx)
{
    if (ctx->ix >= ctx->max) {
        return NULL;
    }
    return &ctx->toks[ctx->ix];
}

tok_t *
lookahead(xform_t *ctx, int num)
{
    int    saved_ix = ctx->ix;
    tok_t *t;

    for (int i = 0; i < num; i++) {
        t = advance(ctx, true);
    }

    ctx->ix = saved_ix;

    return t;
}

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

static inline tok_t *
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
            if (id_found || got_star || got_paren) {
                goto syntax_err;
            }
            got_star = true;
            continue;
        default:
syntax_err:
            fprintf(stderr, "Invalid syntax for keyword block.\n");
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

static inline void
finish_rewrite(xform_t *ctx, bool include_comments)
{
    for (int i = ctx->rewrite_start_ix; i < ctx->ix; i++) {
        if (!include_comments || ctx->toks[i].type != TT_COMMENT) {
            ctx->toks[i].skip_emit = true;
        }
    }
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

static inline bool
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

static inline bool
try_id_xforms(xform_t *ctx)
{
    tok_t *t = &ctx->toks[ctx->ix];

    if (id_check("keywords", ctx->input, t->offset, t->len)) {
        ctx->rewrite_start_ix = ctx->ix;
        return keyword_xform(ctx, t);
    }

    ctx->ix++;
    return true;
}

bool
apply_transforms(lex_t *state)
{
    xform_t ctx = {
        .input = state->input,
        .toks  = state->toks,
        .ix    = 0,
        .max   = state->num_toks,
    };

    while (ctx.ix < ctx.max) {
        switch (ctx.toks[ctx.ix].type) {
        case TT_ID:
            if (!try_id_xforms(&ctx)) {
                return false;
            }
            continue;
        default:
            ctx.ix++;
        }
    }

    return true;
}

int
main(int argc, char *argv[], char *envp[])
{
    int  base  = 1;
    bool debug = false;

    if (argc > 1) {
        if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--debug")) {
            debug = true;
            base++;
        }
    }

    if (argc != base + 2) {
        fprintf(stderr, "Usage: %s infile outfile\n", argv[0]);
        return -1;
    }

    lex_t state = (lex_t){
        .num_toks   = 0,
        .input      = read_file(argv[base]),
        .toks       = NULL,
        .cur        = NULL,
        .end        = NULL,
        .line_start = true,
    };

    if (!state.input) {
        fprintf(stderr,
                "%s: Could not read input file: %s\n",
                argv[0],
                argv[base]);
        return -2;
    }

    if (!strcmp(argv[base], argv[base + 1])) {
        fprintf(stderr,
                "%s: Output file cannot be the same as the input file.",
                argv[0]);
    }

    lex(&state);

    if (debug) {
        print_tokens(&state);
    }

    if (!apply_transforms(&state)) {
        fprintf(stderr, "%s: ncpp translation failed.\n", argv[base]);
        return -3;
    }

    if (!output_new_file(&state, argv[base + 1])) {
        fprintf(stderr,
                "%s: Could not write to file: %s\n",
                argv[0],
                argv[base + 1]);
        return -4;
    }
}
