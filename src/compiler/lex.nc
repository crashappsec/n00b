#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    char *tt_name;
    bool  show_contents;
} internal_tt_info_t;

static const internal_tt_info_t tt_info[] = {
    {"error", false},
    {"space", false},
    {";", false},
    {"newline", false},
    {"comment", true},
    {"~", false},
    {"+", false},
    {"-", false},
    {"*", false},
    {"comment", true},
    {"/", false}, // 10
    {"%", false},
    {"<=", false},
    {"<", false},
    {">=", false},
    {">", false},
    {"!=", false},
    {"!", false},
    {":", false},
    {"=", false},
    {"==", false}, // 20
    {",", false},
    {".", false},
    {"{", false},
    {"}", false},
    {"[", false},
    {"]", false},
    {"(", false},
    {")", false},
    {"and", false},
    {"or", false}, // 30
    {"int", true},
    {"hex", true},
    {"float", true},
    {"string", true},
    {"char", true},
    {"unquoted", true},
    {"true", false},
    {"false", false},
    {"nil", false},
    {"if", false}, // 40
    {"elif", false},
    {"else", false},
    {"for", false},
    {"from", false},
    {"to", false},
    {"break", false},
    {"continue", false},
    {"return", false},
    {"enum", false},
    {"identifier", true}, // 50
    {"func", false},
    {"var", false},
    {"global", false},
    {"const", false},
    {"once", false},
    {"let", false},
    {"private", false},
    {"`", false},
    {"->", false},
    {"object", false}, // 60
    {"while", false},
    {"in", false},
    {"&", false},
    {"|", false},
    {"^", false},
    {"<<", false},
    {">>", false},
    {"typeof", false},
    {"switch", false},
    {"case", false},
    {"+=", false}, // 70
    {"-=", false},
    {"*=", false},
    {"/=", false},
    {"%=", false},
    {"&=", false},
    {"|=", false},
    {"^=", false},
    {"<<=", false},
    {">>=", false}, // 80
    {"lock", false},
#ifdef N00B_DEV
    {"print", false},
#endif
    {"eof", false},
};

void
n00b_token_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    *bitfield = 0x3f;
}

n00b_string_t *
n00b_token_type_to_string(n00b_token_kind_t tk)
{
    return n00b_cstring(tt_info[tk].tt_name);
}

typedef struct {
    n00b_module_t    *ctx;
    n00b_codepoint_t *start;
    n00b_codepoint_t *end;
    n00b_codepoint_t *pos;
    n00b_codepoint_t *line_start;
    n00b_token_t     *last_token;
    size_t            token_id;
    size_t            line_no;
    size_t            cur_tok_line_no;
    size_t            cur_tok_offset;
} lex_state_t;

// These helpers definitely require us to keep names consistent internally.
//
// They just remove clutter in calling stuff and emphasize the variability:
// - TOK adds a token to the output stream of the given kind;
// - LITERAL_TOK is the same, except the system looks to see if there is
// - a lit modifier at the end; if there is, it copies it into the token.
// - LEX_ERROR adds an error to the broader context object, and longjumps.
#define TOK(kind) output_token(state, kind)
#define LITERAL_TOK(kind, amt, syntax)   \
    output_token(state, kind);           \
    state->last_token->adjustment = amt; \
    capture_lit_text(state->last_token); \
    handle_lit_mod(state, syntax)
#define LEX_ERROR(code)          \
    fill_lex_error(state, code); \
    N00B_CRAISE("Exception:" #code "\n")

static const __uint128_t max_intval = (__uint128_t)0xffffffffffffffffULL;

static inline n00b_codepoint_t
next(lex_state_t *state)
{
    if (state->pos >= state->end) {
        return 0;
    }
    return *state->pos++;
}

static inline void
unput(lex_state_t *state)
{
    if (state->pos && state->pos <= state->end) {
        --state->pos;
    }
}

static inline void
advance(lex_state_t *state)
{
    state->pos++;
}

static inline n00b_codepoint_t
peek(lex_state_t *state)
{
    if (state->pos >= state->end) {
        return 0;
    }
    return *(state->pos);
}

static inline void
at_new_line(lex_state_t *state)
{
    state->line_no++;
    state->line_start = state->pos;
}

static inline void
output_token(lex_state_t *state, n00b_token_kind_t kind)
{
    n00b_token_t *tok = n00b_gc_alloc_mapped(n00b_token_t,
                                             n00b_token_set_gc_bits);
    tok->kind         = kind;
    tok->noscan       = N00B_NOSCAN;
    tok->module       = state->ctx;
    tok->start_ptr    = state->start;
    tok->end_ptr      = state->pos;
    tok->token_id     = ++state->token_id;
    tok->line_no      = state->cur_tok_line_no;
    tok->line_offset  = state->cur_tok_offset;
    state->last_token = tok;

    n00b_list_append(state->ctx->ct->tokens, tok);
}

static inline void
skip_optional_newline(lex_state_t *state)
{
    n00b_codepoint_t *start = state->pos;

    while (true) {
        switch (peek(state)) {
        case ' ':
        case '\t':
            advance(state);
            continue;
        case '\n':
            advance(state);
            at_new_line(state);
            // We only allow one newline after tokens.  So don't keep
            // running the same loop; we're done when this one finds
            // a non-space character.
            while (true) {
                switch (peek(state)) {
                case ' ':
                case '\t':
                    advance(state);
                    continue;
                default:
                    goto possible_ws_token;
                }
            }
            // Explicitly fall through here out of the nested switch
            // since we're done.
        default:
possible_ws_token:
            if (state->pos != start) {
                TOK(n00b_tt_space);
            }
            return;
        }
    }
}

static inline void
handle_lit_mod(lex_state_t *state, n00b_lit_syntax_t syntax)
{
    n00b_token_t *tok = state->last_token;

    tok->syntax = syntax;

    if (peek(state) != '\'') {
        return;
    }
    advance(state);

    n00b_codepoint_t *lm_start = state->pos;

    while (n00b_codepoint_is_n00b_id_continue(peek(state))) {
        advance(state);
    }

    size_t n = (size_t)(state->pos - lm_start);

    tok->literal_modifier = n00b_utf32(lm_start, n);
    state->start          = state->pos;
}

static inline void
capture_lit_text(n00b_token_t *tok)
{
    int64_t diff = tok->end_ptr - tok->start_ptr - 2 * tok->adjustment;
    tok->text    = n00b_utf32(tok->start_ptr + tok->adjustment, diff);
}

static inline void
fill_lex_error(lex_state_t *state, n00b_compile_error_t code)

{
    n00b_token_t *tok = n00b_gc_alloc_mapped(n00b_token_t, n00b_token_set_gc_bits);
    tok->kind         = n00b_tt_error;
    tok->noscan       = N00B_NOSCAN;
    tok->module       = state->ctx;
    tok->start_ptr    = state->start;
    tok->end_ptr      = state->pos;
    tok->line_no      = state->line_no;
    tok->line_offset  = state->start - state->line_start;

    n00b_compile_error *err = n00b_new_error(0);
    err->code               = code;
    err->loc                = n00b_token_get_location_str(tok);

    if (!state->ctx->ct->errors) {
        state->ctx->ct->errors = n00b_list(n00b_type_ref());
    }

    n00b_list_append(state->ctx->ct->errors, err);
}

#if 0
static inline void
scan_unquoted_literal(lex_state_t *state)
{
    // For now, this just scans to the end of the line, and returns a
    // token of type n00b_tt_unquoted_lit.  When it comes time to
    // re-implement the litmod stuff and we add literal parsers for
    // all the builtins, this can generate the proper token up-front.
    while (true) {
        switch (next(state)) {
        case '\n':
            at_new_line(state);
            // fallthrough.
        case 0:
            LITERAL_TOK(n00b_tt_unquoted_lit, 0);
            return;
        }
    }
}
#endif

static void
scan_int_or_float_literal(lex_state_t *state)
{
    // This one probably does make more sense to fully parse here.
    // There is an issue:
    //
    // We're using u32 as our internal repr for what we're parsing.
    // But the easiest way to deal w/ floats is to call strtod(),
    // which expects UTF8 (well, ASCII really). We don't want to
    // reconvert (or keep around) the whole remainder of the file, so
    // we just scan forward looking at absolutely every character than
    // can possibly be in a valid float (including E/e, but not NaN /
    // infinity; those will have to be handled as keywords).  We
    // convert that bit back to UTF-8.
    //
    // If did we see a starting character that indicates a float, we
    // know it might be a float, so we keep a record of where the
    // first such character is; then we call strtod(); if strtod()
    // tells us it found a valid parse where the ending point is
    // farther than the first float indicator, then we're
    // done; we just need to set the proper token end point.
    //
    // Otherwise, we re-parse as an int, and we can just do that
    // manually into a __uint128_t (getting the float parse precisely
    // right is not something I relish, even though it can be done
    // faster than w/ strtod).
    //
    // One final note: we already passed the first character before we
    // got here. But state->start does point to the beginning, so we
    // use that when we need to reconstruct the string.

    n00b_codepoint_t *start    = state->start;
    int               ix       = 1; // First index we need to check.
    int               float_ix = 0; // 0 means not a float.

    while (true) {
        switch (start[ix]) {
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
            ix++;
            continue;
        case '.':
            if (float_ix) {
                // Already had a dot or something like that.
                break;
            }
            float_ix = ix++;
            continue;
        case 'e':
        case 'E':
            if (!float_ix) {
                float_ix = ix;
            }
            ix++;
            continue;
        case '+':
        case '-':
            ix++;
            continue;
        default:
            break;
        }
        break;
    }

    n00b_string_t *u8 = n00b_utf32(start, ix);

    if (float_ix) {
        char  *endp  = NULL;
        double value = strtod((char *)u8->data, &endp);

        if (endp == (char *)u8->data || !endp) {
            // I don't think this one should ever happen here.
            LEX_ERROR(n00b_err_lex_invalid_float_lit);
        }

        if (errno == ERANGE) {
            if (value == HUGE_VAL) {
                LEX_ERROR(n00b_err_lex_float_oflow);
            }
            LEX_ERROR(n00b_err_lex_float_uflow);
        }

        int float_strlen = (int)(endp - u8->data);
        if (float_strlen > float_ix) {
            state->pos = state->start + float_strlen;
            LITERAL_TOK(n00b_tt_float_lit, 0, ST_Float);
            state->last_token->literal_value = ((n00b_box_t)value).v;
            return;
        }
    }

    // Either we saw no evidence of a float or the float parse
    // didn't get to any of that evidence, so voila, it's an int token.

    __int128_t val  = 0;
    int        i    = 0;
    size_t     slen = n00b_string_byte_len(u8);
    char      *p    = (char *)u8->data;

    for (; i < (int64_t)slen; i++) {
        char c = *p++;

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
            val *= 10;
            val += c - '0';
            if (val > (uint64_t)max_intval) {
                LEX_ERROR(n00b_err_lex_int_oflow);
            }
            continue;
        default:
            goto finished_int;
        }
    }
finished_int:;
    uint64_t n = (uint64_t)val;
    state->pos = state->start + i;
    LITERAL_TOK(n00b_tt_int_lit, 0, ST_Base10);
    state->last_token->literal_value = (void *)n;
    return;
}

static inline void
scan_hex_literal(lex_state_t *state)
{
    while (true) {
        switch (peek(state)) {
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
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            advance(state);
            continue;
        default:
            LITERAL_TOK(n00b_tt_hex_lit, 0, ST_Hex);
            return;
        }
    }
}

// This only gets called if we already passed a leading 0. So we
// inspect the first char; if it's an 'x' or 'X', we go the hex
// route. Otherwise, we go the int route, which promotes to float
// depending on what it sees.

static inline void
scan_int_float_or_hex_literal(lex_state_t *state)
{
    switch (peek(state)) {
    case 'x':
    case 'X':
        advance(state);
        scan_hex_literal(state);
        return;
    default:
        scan_int_or_float_literal(state);
        return;
    }
}

static inline void
scan_tristring(lex_state_t *state)
{
    // Here, we already got 3 quotes. We now need to:
    // 1. Keep track of line numbers when we see newlines.
    // 2. Skip any backtick'd things.
    // 3. Count consecutive quotes.
    // 4. Error when we get to EOF.

    int quote_count = 0;

    while (true) {
        switch (next(state)) {
        case 0:
            LEX_ERROR(n00b_err_lex_eof_in_string_lit);
        case '\n':
            at_new_line(state);
            break;
        case '\\':
            advance(state);
            break;
        case '"':
            if (++quote_count == 3) {
                LITERAL_TOK(n00b_tt_string_lit, 3, ST_2Quote);
                return;
            }
            continue; // breaking would reset quote count.
        default:
            break;
        }
        quote_count = 0;
    }
}

static void
scan_string_literal(lex_state_t *state)
{
    // This function only finds the end of the string and keeps track
    // of line numbers; it does not otherwise attempt to handle any
    // parsing of the string itself.
    //
    // That could either be done after we've seen if there's a lit mod,
    // or wait until the parser or ir generator need the data;
    //
    // My choice is to do it as late as possible, because we could
    // then allow people to register litmods and then use them in the
    // same source file (or a dependent source file) if done properly.

    // Here, we know we already passed a single quote. We must first
    // determine if we're looking at a tristring.
    if (peek(state) == '"') {
        advance(state);
        if (peek(state) != '"') {
            // empty string.
            goto finish_single_quote;
        }
        advance(state);
        scan_tristring(state);
        return;
    }

    while (true) {
        n00b_codepoint_t c = next(state);

        switch (c) {
        case 0:
            LEX_ERROR(n00b_err_lex_eof_in_string_lit);
        case '\n':
        case '\r':
            LEX_ERROR(n00b_err_lex_nl_in_string_lit);
        case '\\':
            // Skip absolutely anything that comes next,
            // including a newline.
            advance(state);
            continue;
        case '"':
finish_single_quote:
            LITERAL_TOK(n00b_tt_string_lit, 1, ST_2Quote);
            return;
        default:
            continue;
        }
    }
}

// Char literals can be:
// 1. a single character
// 2. \x, \X, \u, \U .. They're all the same. We scan till ' or some
//    error condition (which includes another \).
//    We don't check the value at this point; default char type will
//    error if it's outside the range of valid unicode. We don't even
//    check for it being valid hex; we just scan it.
//    w/ \u and \U I'll probably accept an optional + after the U since
//    officially that's what the unicode consortium does.
// 3. \ followed by any single character.
// -1. If we get a newline or null, it's an error.
// Also, if we get anything after it other than a ', it's an error.
//
// Note specifically that we do NOT turn this into a real char literal
// here. We wait till needed, so we can apply literal modifiers.
static void
scan_char_literal(lex_state_t *state)
{
    switch (next(state)) {
    case 0:
        LEX_ERROR(n00b_err_lex_eof_in_char_lit);
    case '\r':
    case '\n':
        LEX_ERROR(n00b_err_lex_nl_in_char_lit);
    case '\'':
        return;
    case '\\':
        switch (next(state)) {
        case 'x':
        case 'X':
        case 'u':
        case 'U':
            while (true) {
                switch (next(state)) {
                case 0:
                    LEX_ERROR(n00b_err_lex_eof_in_char_lit);
                case '\r':
                case '\n':
                    LEX_ERROR(n00b_err_lex_nl_in_char_lit);
                case '\\':
                    LEX_ERROR(n00b_err_lex_esc_in_esc);
                case '\'':
                    goto finish_up;
                }
            }
        default:
            break;
        }
    default:
        break;
    }
    if (next(state) != '\'') {
        LEX_ERROR(n00b_err_lex_extra_in_char_lit);
    }

finish_up:
    LITERAL_TOK(n00b_tt_char_lit, 1, ST_1Quote);
    return;
}

static n00b_dict_t *kws = NULL;

static inline void
add_keyword(char *keyword, n00b_token_kind_t kind)
{
    hatrack_dict_add(kws, n00b_cstring(keyword), (void *)(int64_t)kind);
}

static inline void
init_keywords()
{
    if (kws != NULL) {
        return;
    }

    kws = n00b_new(n00b_type_dict(n00b_type_string(), n00b_type_i64()));

    add_keyword("True", n00b_tt_true);
    add_keyword("true", n00b_tt_true);
    add_keyword("False", n00b_tt_false);
    add_keyword("false", n00b_tt_false);
    add_keyword("nil", n00b_tt_nil);
    add_keyword("in", n00b_tt_in);
    add_keyword("var", n00b_tt_var);
    add_keyword("let", n00b_tt_let);
    add_keyword("const", n00b_tt_const);
    add_keyword("global", n00b_tt_global);
    add_keyword("private", n00b_tt_private);
    add_keyword("once", n00b_tt_once);
    add_keyword("is", n00b_tt_cmp);
    add_keyword("and", n00b_tt_and);
    add_keyword("or", n00b_tt_or);
    add_keyword("not", n00b_tt_not);
    add_keyword("if", n00b_tt_if);
    add_keyword("elif", n00b_tt_elif);
    add_keyword("else", n00b_tt_else);
    add_keyword("case", n00b_tt_case);
    add_keyword("for", n00b_tt_for);
    add_keyword("while", n00b_tt_while);
    add_keyword("from", n00b_tt_from);
    add_keyword("to", n00b_tt_to);
    add_keyword("break", n00b_tt_break);
    add_keyword("continue", n00b_tt_continue);
    add_keyword("return", n00b_tt_return);
    add_keyword("enum", n00b_tt_enum);
    add_keyword("func", n00b_tt_func);
    add_keyword("object", n00b_tt_object);
    add_keyword("typeof", n00b_tt_typeof);
    add_keyword("switch", n00b_tt_switch);
    add_keyword("infinity", n00b_tt_float_lit);
    add_keyword("NaN", n00b_tt_float_lit);
#ifdef N00B_DEV
    add_keyword("print", n00b_tt_print);
#endif

    n00b_gc_register_root(&kws, 1);
}

static void
scan_id_or_keyword(lex_state_t *state)
{
    init_keywords();

    // The pointer should be over an id_start
    while (true) {
        n00b_codepoint_t c = next(state);
        if (!n00b_codepoint_is_n00b_id_continue(c)) {
            if (c) {
                unput(state);
            }
            break;
        }
    }

    bool    found  = false;
    int64_t length = (int64_t)(state->pos - state->start);

    if (length == 0) {
        return;
    }

    n00b_token_kind_t r = (n00b_token_kind_t)(int64_t)hatrack_dict_get(
        kws,
        n00b_utf32(state->start, length),
        &found);

    if (!found) {
        TOK(n00b_tt_identifier);
        return;
    }

    switch (r) {
    case n00b_tt_true:
    case n00b_tt_false:
        LITERAL_TOK(r, 0, ST_Bool);
        return;
    case n00b_tt_float_lit: {
        n00b_string_t *s     = n00b_utf32(state->start,
                                      (state->pos - state->start));
        double         value = strtod((char *)s->data, NULL);

        LITERAL_TOK(r, 0, ST_Float);
        state->last_token->literal_value = ((n00b_box_t)value).v;
        return;
    }
    default:
        TOK(r);
        return;
    }
}

static void
lex(lex_state_t *state)
{
    while (true) {
        n00b_codepoint_t c;
        n00b_codepoint_t tmp;

        // When we need to escape from nested loops after
        // recognizing a token, it's sometimes easier to short
        // circuit here w/ a goto than to break out of all those
        // loops just to 'continue'.
lex_next_token:
        state->start           = state->pos;
        state->cur_tok_line_no = state->line_no;
        state->cur_tok_offset  = state->start - state->line_start;
        c                      = next(state);

        switch (c) {
        case 0:
            TOK(n00b_tt_eof);
            return;
        case ' ':
        case '\t':
            while (true) {
                switch (peek(state)) {
                case ' ':
                case '\t':
                    advance(state);
                    continue;
                default:
                    goto lex_next_token;
                }
            }
            TOK(n00b_tt_space);
            continue;
        case '\r':
            tmp = next(state);
            if (tmp != '\n') {
                LEX_ERROR(n00b_err_lex_stray_cr);
            }
            // Fallthrough if no exception got raised.
            // fallthrough
        case '\n':
            TOK(n00b_tt_newline);
            at_new_line(state);
            continue;
        case '#':
            // Line comments go to EOF or new line, and we include the
            // newline in the token.
            // Double-slash comments work in n00b too; if we see that,
            // the lexer jumps back up here once it advances past the
            // second slash.
line_comment:
            while (true) {
                switch (next(state)) {
                case '\n':
                    at_new_line(state);
                    TOK(n00b_tt_line_comment);
                    goto lex_next_token;
                case 0: // EOF
                    TOK(n00b_tt_eof);
                    return;
                default:
                    continue;
                }
            }
        case '~':
            TOK(n00b_tt_lock_attr);
            continue;
        case '`':
            TOK(n00b_tt_backtick);
            continue;
        case '+':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_plus_eq);
            }
            else {
                TOK(n00b_tt_plus);
            }
            skip_optional_newline(state);
            continue;
        case '-':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(n00b_tt_minus_eq);
                break;
            case '>':
                advance(state);
                TOK(n00b_tt_arrow);
                break;
            default:
                TOK(n00b_tt_minus);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '*':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_mul_eq);
            }
            else {
                TOK(n00b_tt_mul);
            }
            skip_optional_newline(state);
            continue;
        case '/':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(n00b_tt_div_eq);
                skip_optional_newline(state);
                break;
            case '/':
                advance(state);
                goto line_comment;
            case '*':
                advance(state);
                while (true) {
                    switch (next(state)) {
                    case '\n':
                        at_new_line(state);
                        continue;
                    case '*':
                        if (peek(state) == '/') {
                            advance(state);
                            TOK(n00b_tt_long_comment);
                            goto lex_next_token;
                        }
                        continue;
                    case 0:
                        LEX_ERROR(n00b_err_lex_eof_in_comment);
                    default:
                        continue;
                    }
                }
            default:
                TOK(n00b_tt_div);
                skip_optional_newline(state);
                break;
            }
            continue;
        case '%':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_mod_eq);
            }
            else {
                TOK(n00b_tt_mod);
            }
            skip_optional_newline(state);
            continue;
        case '<':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(n00b_tt_lte);
                break;
            case '<':
                advance(state);
                if (peek(state) == '=') {
                    advance(state);
                    TOK(n00b_tt_shl_eq);
                }
                else {
                    TOK(n00b_tt_shl);
                }
                break;
            default:
                TOK(n00b_tt_lt);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '>':
            switch (peek(state)) {
            case '=':
                advance(state);
                TOK(n00b_tt_gte);
                break;
            case '>':
                advance(state);
                if (peek(state) == '=') {
                    advance(state);
                    TOK(n00b_tt_shr_eq);
                }
                else {
                    TOK(n00b_tt_shr);
                }
                break;
            default:
                TOK(n00b_tt_gt);
                break;
            }
            skip_optional_newline(state);
            continue;
        case '!':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_neq);
            }
            else {
                TOK(n00b_tt_not);
            }
            skip_optional_newline(state);
            continue;
        case ';':
            TOK(n00b_tt_semi);
            continue;
        case ':':
            TOK(n00b_tt_colon);
            continue;
        case '=':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_cmp);
            }
            else {
                TOK(n00b_tt_assign);
            }
            skip_optional_newline(state);
            continue;
        case ',':
            TOK(n00b_tt_comma);
            skip_optional_newline(state);
            continue;
        case '.':
            TOK(n00b_tt_period);
            skip_optional_newline(state);
            continue;
        case '{':
            TOK(n00b_tt_lbrace);
            skip_optional_newline(state);
            continue;
        case '}':
            LITERAL_TOK(n00b_tt_rbrace, 0, ST_Dict);
            continue;
        case '[':
            TOK(n00b_tt_lbracket);
            skip_optional_newline(state);
            continue;
        case ']':
            LITERAL_TOK(n00b_tt_rbracket, 0, ST_List);
            continue;
        case '(':
            TOK(n00b_tt_lparen);
            skip_optional_newline(state);
            continue;
        case ')':
            LITERAL_TOK(n00b_tt_rparen, 0, ST_Tuple);
            continue;
        case '&':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_bit_and_eq);
            }
            else {
                TOK(n00b_tt_bit_and);
            }
            skip_optional_newline(state);
            continue;
        case '|':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_bit_or_eq);
            }
            else {
                TOK(n00b_tt_bit_or);
            }
            skip_optional_newline(state);
            continue;
        case '^':
            if (peek(state) == '=') {
                advance(state);
                TOK(n00b_tt_bit_xor_eq);
            }
            else {
                TOK(n00b_tt_bit_xor);
            }
            skip_optional_newline(state);
            continue;
        case '0':
            scan_int_float_or_hex_literal(state);
            continue;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            scan_int_or_float_literal(state);
            continue;
        case '\'':
            scan_char_literal(state);
            continue;
        case '"':
            scan_string_literal(state);
            continue;
        default:
            if (!n00b_codepoint_is_n00b_id_start(c)) {
                LEX_ERROR(n00b_err_lex_invalid_char);
            }
            scan_id_or_keyword(state);
            continue;
        }
    }
}

bool
n00b_lex(n00b_module_t *ctx, n00b_stream_t *stream)
{
    n00b_assert(ctx->name);

    if (ctx->ct->status >= n00b_compile_status_tokenized) {
        return ctx->ct->fatal_errors;
    }

    n00b_obj_t raw;

    if (!ctx->source) {
        ctx->source = n00b_read_all(stream, 0);
    }

    raw = ctx->source;

    lex_state_t lex_info = {
        .token_id   = 0,
        .line_no    = 1,
        .line_start = 0,
        .ctx        = ctx,
    };

    if (raw == NULL) {
        return false;
    }

    if (n00b_type_is_buffer(n00b_get_my_type(raw))) {
        // A buffer object, which we assume is utf8.
        if (n00b_buffer_len((n00b_buf_t *)raw) == 0) {
            return false;
        }
        ctx->source = n00b_buf_to_utf8_string((n00b_buf_t *)raw);
    }

    n00b_string_t *utf32 = ctx->source;

    n00b_string_require_u32(utf32);

    int len             = n00b_string_codepoint_len(utf32);
    ctx->ct->tokens     = n00b_new(n00b_type_list(n00b_type_ref()));
    lex_info.start      = (n00b_codepoint_t *)utf32->u32_data;
    lex_info.pos        = (n00b_codepoint_t *)utf32->u32_data;
    lex_info.line_start = (n00b_codepoint_t *)utf32->u32_data;
    lex_info.end        = &((n00b_codepoint_t *)(utf32->u32_data))[len];

    bool error = false;

    N00B_TRY
    {
        lex(&lex_info);
    }
    N00B_EXCEPT
    {
        error = true;
    }
    N00B_TRY_END;

    ctx->ct->status = n00b_compile_status_tokenized;
    n00b_module_set_status(ctx, n00b_compile_status_tokenized);
    ctx->ct->fatal_errors = error;

    return !error;
}

n00b_string_t *
n00b_format_one_token(n00b_token_t *tok, n00b_string_t *prefix)
{
    int32_t        info_ix = (int)tok->kind;
    n00b_string_t *val     = n00b_utf32(tok->start_ptr,
                                    tok->end_ptr - tok->start_ptr);

    return n00b_cformat("«#»#«#» «#» («#»:«#») «#»",
                        prefix,
                        (int64_t)tok->token_id,
                        tt_info[info_ix].tt_name,
                        (int64_t)tok->line_no,
                        (int64_t)tok->line_offset,
                        val);
}

// Start out with any focus on color or other highlighting; just get
// them into a default table for now aimed at debugging, and we'll add
// a facility for styling later.
n00b_table_t *
n00b_format_tokens(n00b_module_t *ctx)
{
    n00b_table_t *tbl = n00b_table("columns", n00b_ka(5));
    int64_t       len = n00b_list_len(ctx->ct->tokens);

    n00b_table_add_cell(tbl, n00b_cstring("Seq #"));
    n00b_table_add_cell(tbl, n00b_cstring("Type"));
    n00b_table_add_cell(tbl, n00b_cstring("Line #"));
    n00b_table_add_cell(tbl, n00b_cstring("Column #"));
    n00b_table_add_cell(tbl, n00b_cstring("Value"));

    for (int64_t i = 0; i < len; i++) {
        n00b_token_t *tok     = n00b_list_get(ctx->ct->tokens, i, NULL);
        int           info_ix = (int)tok->kind;

        n00b_table_add_cell(tbl, n00b_string_from_int(i + 1));
        n00b_table_add_cell(tbl, n00b_cstring(tt_info[info_ix].tt_name));
        n00b_table_add_cell(tbl, n00b_string_from_int(tok->line_no));
        n00b_table_add_cell(tbl, n00b_string_from_int(tok->line_offset));

        if (tt_info[info_ix].show_contents) {
            int n = tok->end_ptr - tok->start_ptr;
            n00b_table_add_cell(tbl, n00b_utf32(tok->start_ptr, n));
        }
        else {
            n00b_table_add_cell(tbl, n00b_cached_empty_string());
        }
    }

    return tbl;
}

n00b_string_t *
n00b_token_raw_content(n00b_token_t *tok)
{
    if (!tok) {
        return NULL;
    }

    if (!tok->text) {
        capture_lit_text(tok);
    }

    if (!strcmp(tok->text->data, "result")) {
        return n00b_cstring("$result");
    }

    return tok->text;
}
