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

#include "ncpp.h"

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
