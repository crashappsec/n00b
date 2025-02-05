#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void n00b_gopt_comma(n00b_gopt_lex_state *);

static inline n00b_string_t *
n00b_gopt_normalize(n00b_gopt_lex_state *state, n00b_string_t *s)
{
    if (state->gctx->options & N00B_CASE_SENSITIVE) {
        return s;
    }

    return n00b_string_lower(s);
}

static int64_t
lookup_bi_id(n00b_gopt_lex_state *state, n00b_gopt_bi_ttype i)
{
    return (int64_t)n00b_list_get(state->gctx->terminal_info, i, NULL);
}

static void
n00b_gopt_tok_emit(n00b_gopt_lex_state *state,
                   int64_t              id,
                   int                  word,
                   int                  start,
                   int                  end,
                   n00b_string_t       *contents)
{
    if (id >= 0 && id < N00B_GOTT_LAST_BI) {
        id = lookup_bi_id(state, id);
    }

    n00b_token_info_t *tok = n00b_gc_alloc_mapped(n00b_token_info_t,
                                                  N00B_GC_SCAN_ALL);
    tok->value             = contents;
    tok->tid               = id,
    tok->index             = n00b_list_len(state->gctx->tokens);
    tok->line              = word;
    tok->column            = start;
    tok->endcol            = end;

    n00b_list_append(state->gctx->tokens, tok);
}

static inline void
n00b_gopt_tok_command_name(n00b_gopt_lex_state *state)
{
    if (!n00b_gopt_gflag_is_set(state, N00B_TOPLEVEL_IS_ARGV0)) {
        return;
    }

    if (!state->command_name) {
        n00b_gopt_tok_emit(state,
                           lookup_bi_id(state, N00B_GOTT_OTHER_COMMAND_NAME),
                           0,
                           0,
                           0,
                           n00b_cstring("??"));
        return;
    }

    n00b_string_t *s     = state->command_name;
    int64_t        tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                              s,
                                              NULL);

    if (!tokid) {
        tokid = N00B_GOTT_OTHER_COMMAND_NAME;
    }

    n00b_gopt_tok_emit(state, tokid, 0, 0, 0, s);
}

static void
n00b_gopt_tok_word_or_bool(n00b_gopt_lex_state *state)
{
    // At this point, we are either going to generate BOOL or WORD
    // for whatever is left. So slice it out, compare the normalized
    // version against our boolean values, and be done.

    n00b_string_t *raw_contents;
    n00b_string_t *s;
    n00b_string_t *truthstr;

    if (state->cur_word_position == 0) {
        raw_contents = state->raw_word;
    }
    else {
        raw_contents = n00b_string_slice(state->raw_word,
                                         state->cur_word_position,
                                         state->cur_wordlen);
    }

    if (n00b_len(raw_contents) == 0) {
        return; // Nothing to yield.
    }

    n00b_codepoint_t *p = (n00b_codepoint_t *)raw_contents->data;

    switch (*p) {
    case 't':
    case 'T':
        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_TF)) {
            break;
        }
        // Even if it isn't normalized, force it...
        s = n00b_string_lower(raw_contents);
        if (n00b_string_eq(s, n00b_cstring("t"))
            || n00b_string_eq(s, n00b_cstring("true"))) {
            n00b_gopt_tok_emit(state,
                               N00B_GOTT_BOOL_T,
                               state->word_id,
                               state->cur_word_position,
                               state->cur_wordlen,
                               n00b_cstring("true"));
            return;
        }
        break;
    case 'f':
    case 'F':
        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_TF)) {
            break;
        }
        // Even if it isn't normalized, force it...
        s = n00b_string_lower(raw_contents);
        if (n00b_string_eq(s, n00b_cstring("f"))
            || n00b_string_eq(s, n00b_cstring("false"))) {
            n00b_gopt_tok_emit(state,
                               N00B_GOTT_BOOL_F,
                               state->word_id,
                               state->cur_word_position,
                               state->cur_wordlen,
                               n00b_cstring("false"));
            return;
        }
        break;
    case 'y':
    case 'Y':
        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_YN)) {
            break;
        }

        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_TF)) {
            truthstr = n00b_cstring("yes");
        }
        else {
            truthstr = n00b_cstring("true");
        }
        // Even if it isn't normalized, force it...
        s = n00b_string_lower(raw_contents);
        if (n00b_string_eq(s, n00b_cstring("y"))
            || n00b_string_eq(s, n00b_cstring("yes"))) {
            n00b_gopt_tok_emit(state,
                               N00B_GOTT_BOOL_T,
                               state->word_id,
                               state->cur_word_position,
                               state->cur_wordlen,
                               truthstr);
            return;
        }
        break;
    case 'n':
    case 'N':
        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_YN)) {
            break;
        }

        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_BOOL_NO_TF)) {
            truthstr = n00b_cstring("no");
        }
        else {
            truthstr = n00b_cstring("false");
        }
        // Even if it isn't normalized, force it...
        s = n00b_string_lower(raw_contents);
        if (n00b_string_eq(s, n00b_cstring("n"))
            || n00b_string_eq(s, n00b_cstring("no"))) {
            n00b_gopt_tok_emit(state,
                               N00B_GOTT_BOOL_F,
                               state->word_id,
                               state->cur_word_position,
                               state->cur_wordlen,
                               truthstr);
            return;
        }
        break;
    }

    int64_t ix = n00b_string_find(raw_contents, n00b_cached_comma());
    int64_t tokid;

    if (ix == -1) {
        // If it's a command name, use the specific token
        // for the command name; otherwise, use the token
        // for generic word.

        tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                          raw_contents,
                                          NULL);

        if (!tokid) {
            tokid = N00B_GOTT_WORD;
        }

        n00b_gopt_tok_emit(state,
                           tokid,
                           state->word_id,
                           state->cur_word_position,
                           state->cur_wordlen,
                           raw_contents);
        return;
    }

    raw_contents = n00b_string_slice(raw_contents, 0, ix);

    tokid = (int64_t)hatrack_dict_get(state->gctx->sub_names,
                                      raw_contents,
                                      NULL);

    if (!tokid) {
        tokid = N00B_GOTT_WORD;
    }

    n00b_gopt_tok_emit(state,
                       tokid,
                       state->word_id,
                       state->cur_word_position,
                       state->cur_word_position + ix,
                       raw_contents);
    state->cur_word_position += ix;
    n00b_gopt_comma(state);
    return;
}

static inline void
n00b_gopt_tok_possible_number(n00b_gopt_lex_state *state)
{
    n00b_codepoint_t *start = (n00b_codepoint_t *)state->raw_word->data;
    n00b_codepoint_t *p     = start + state->cur_word_position;
    n00b_codepoint_t *end   = p + state->cur_wordlen;
    bool              dot   = false;

    while (p < end) {
        switch (*p++) {
        case '.':
            if (dot) {
                n00b_gopt_tok_word_or_bool(state);
                return;
            }
            dot = true;
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
            continue;
        case ',':;
            int ix = (p - start) + state->cur_word_position;

            n00b_string_t *s = n00b_string_slice(state->raw_word,
                                                 state->cur_word_position,
                                                 ix);

            n00b_gopt_tok_emit(state,
                               dot ? N00B_GOTT_FLOAT : N00B_GOTT_INT,
                               state->word_id,
                               state->cur_word_position,
                               end - start,
                               s);

            state->cur_word_position = (end - start);
            n00b_gopt_comma(state);
            return;
        default:
            n00b_gopt_tok_word_or_bool(state);
            return;
        }
    }

    n00b_string_t *s = n00b_string_slice(state->raw_word,
                                         state->cur_word_position,
                                         state->cur_wordlen);
    n00b_gopt_tok_emit(state,
                       dot ? N00B_GOTT_FLOAT : N00B_GOTT_INT,
                       state->word_id,
                       state->cur_word_position,
                       state->cur_wordlen,
                       s); // Have emit take the slice.
}

static inline void
n00b_gopt_tok_num_bool_or_word(n00b_gopt_lex_state *state)
{
    n00b_codepoint_t *p = (n00b_codepoint_t *)state->raw_word->data;
    switch (p[state->cur_word_position]) {
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
    case '.':
        n00b_gopt_tok_possible_number(state);
        break;
    default:
        n00b_gopt_tok_word_or_bool(state);
    }
}

static void
n00b_gopt_comma(n00b_gopt_lex_state *state)
{
    n00b_gopt_tok_emit(state,
                       N00B_GOTT_COMMA,
                       state->word_id,
                       state->cur_word_position,
                       state->cur_word_position + 1,
                       n00b_cached_comma());
    state->cur_word_position++;

    if (state->cur_word_position != state->cur_wordlen) {
        n00b_gopt_tok_num_bool_or_word(state);
    }
}

void
n00b_peek_and_disallow_assign(n00b_gopt_lex_state *state)
{
    // Here, we know the start of the next word cannot be an
    // assignment operator, so we set a flag to ensure that, if the
    // next word does start with one, it's treated as a letter in a
    // word, and does not generate a token.

    if (state->word_id + 1 == state->num_words) {
        return;
    }

    n00b_string_t *s = n00b_list_get(state->words, state->word_id + 1, NULL);
    if (n00b_string_codepoint_len(s) == 0) {
        return;
    }

    switch (s->data[0]) {
    case '=':
        state->force_word = true;
        return;
    case ':':
        if (n00b_gopt_gflag_is_set(state, N00B_ALLOW_COLON_SEPARATOR)) {
            state->force_word = true;
        }
        return;
    default:
        return;
    }
}

static inline void
n00b_gopt_tok_argument_separator(n00b_gopt_lex_state *state, n00b_codepoint_t cp)
{
    if (!state->cur_word_position) {
        if (n00b_gopt_gflag_is_set(state, N00B_GOPT_NO_EQ_SPACING)) {
            // TODO: add a warning here.
            n00b_gopt_tok_word_or_bool(state);
            return;
        }
    }

    n00b_gopt_tok_emit(state,
                       N00B_GOTT_ASSIGN,
                       state->word_id,
                       state->cur_word_position,
                       state->cur_word_position + 1,
                       n00b_string_from_codepoint(cp));

    state->cur_word_position++;

    if (state->cur_word_position != state->cur_wordlen) {
        n00b_gopt_tok_num_bool_or_word(state);
        return;
    }
    if (state->word_id + 1 == state->num_words) {
        return;
    }

    n00b_peek_and_disallow_assign(state);
}

static void
n00b_emit_proper_flag(n00b_gopt_lex_state *state, int end_ix)
{
    n00b_string_t *s = n00b_string_slice(state->word,
                                         state->cur_word_position,
                                         end_ix);
    int64_t        tok_id;

    n00b_goption_t *info = hatrack_dict_get(state->gctx->all_options,
                                            s,
                                            NULL);
    if (!info) {
        tok_id = N00B_GOTT_UNKNOWN_OPT;
    }
    else {
        tok_id = info->token_id;
    }

    s = n00b_string_slice(state->raw_word,
                          state->cur_word_position,
                          end_ix);
    n00b_gopt_tok_emit(state,
                       tok_id,
                       state->word_id,
                       state->cur_word_position,
                       end_ix,
                       s);
}

static inline void
n00b_gopt_tok_gnu_short_opts(n00b_gopt_lex_state *state)
{
    // Generate a token for every flag we see from the beginning.
    // If we find a letter that isn't a flag, we generate an
    // UNKNOWN_OPT token, but keep going.
    //
    // As soon as we find an argument that can take an option at all,
    // we treat the rest of the word as an argument.

    n00b_codepoint_t *start   = (n00b_codepoint_t *)state->raw_word->data;
    n00b_codepoint_t *p       = start + state->cur_word_position;
    n00b_codepoint_t *end     = start + state->cur_wordlen;
    bool              got_arg = false;
    n00b_goption_t   *info;
    n00b_string_t    *flag;
    int64_t           tok_id;

    while (p < end) {
        flag = n00b_string_from_codepoint(*p);
        info = hatrack_dict_get(state->gctx->all_options, flag, NULL);

        if (info == NULL) {
            tok_id = N00B_GOTT_UNKNOWN_OPT;
        }
        else {
            tok_id = info->token_id;
            switch (info->type) {
            case N00B_GOAT_BOOL_T_ALWAYS:
            case N00B_GOAT_BOOL_F_ALWAYS:
            case N00B_GOAT_CHOICE_T_ALIAS:
            case N00B_GOAT_CHOICE_F_ALIAS:
                break;
            default:
                got_arg = true;
                break;
            }
        }

        n00b_gopt_tok_emit(state,
                           tok_id,
                           state->word_id,
                           state->cur_word_position,
                           state->cur_word_position + 1,
                           flag);
        p++;
        state->cur_word_position++;

        if (got_arg && state->cur_word_position != state->cur_wordlen) {
            n00b_gopt_tok_num_bool_or_word(state);
            return;
        }
    }
}

static inline void
n00b_gopt_tok_longform_opt(n00b_gopt_lex_state *state)
{
    n00b_codepoint_t *start = (n00b_codepoint_t *)state->raw_word->data;
    n00b_codepoint_t *p     = start + state->cur_word_position;
    n00b_codepoint_t *end   = start + state->cur_wordlen;

    while (p < end) {
        switch (*p) {
        case ':':
            if (n00b_gopt_gflag_is_set(state, N00B_ALLOW_COLON_SEPARATOR)) {
                goto got_attached_arg;
            }
            break;
        case '=':
            goto got_attached_arg;
        default:
            break;
        }
        p++;
    }

    n00b_emit_proper_flag(state, state->cur_wordlen);
    if (n00b_gopt_gflag_is_set(state, N00B_GOPT_NO_EQ_SPACING)) {
        n00b_peek_and_disallow_assign(state);
    }

    return;

got_attached_arg:
    n00b_emit_proper_flag(state, p - start);
    state->cur_word_position = p - start;
    n00b_gopt_tok_argument_separator(state, *p);
}

static inline void
n00b_gopt_tok_unix_flag(n00b_gopt_lex_state *state)
{
    if (!n00b_gopt_gflag_is_set(state, N00B_GOPT_SINGLE_DASH_ERR)) {
        if (n00b_string_eq(state->word, n00b_cached_minus())) {
            n00b_gopt_tok_word_or_bool(state);
            return;
        }
    }
    if (!n00b_gopt_gflag_is_set(state, N00B_GOPT_NO_DOUBLE_DASH)) {
        if (n00b_string_eq(state->word, n00b_cstring("--"))) {
            state->all_words = true;
        }
    }

    state->cur_word_position++;

    if (n00b_string_starts_with(state->word, n00b_cstring("--"))) {
        state->cur_word_position++;
        n00b_gopt_tok_longform_opt(state);
        return;
    }

    if (n00b_gopt_gflag_is_set(state, N00B_GOPT_RESPECT_DASH_LEN)) {
        n00b_gopt_tok_gnu_short_opts(state);
        return;
    }

    n00b_gopt_tok_longform_opt(state);
}

static inline void
n00b_gopt_tok_windows_flag(n00b_gopt_lex_state *state)
{
    state->cur_word_position++;
    n00b_gopt_tok_longform_opt(state);
}

static inline void
n00b_gopt_tok_default_state(n00b_gopt_lex_state *state)
{
    state->cur_word_position = 0;

    if (!state->cur_wordlen) {
        return;
    }

    n00b_codepoint_t cp = ((n00b_codepoint_t *)state->raw_word->data)[0];
    switch (cp) {
    case ':':
        if (!n00b_gopt_gflag_is_set(state, N00B_ALLOW_COLON_SEPARATOR)) {
            n00b_gopt_tok_word_or_bool(state);
            return;
        }
        // fallthrough
    case '=':
        // This will process the argument if there's anything after
        // the mark.  Note that the flag check to see how to parse
        // this happens inside this func.
        n00b_gopt_tok_argument_separator(state, cp);
        return;
    case '-':
        // Again, the arg check happens in the function.
        n00b_gopt_tok_unix_flag(state);
        return;
    case '/':
        if (n00b_gopt_gflag_is_set(state, N00B_ALLOW_WINDOWS_SYNTAX)) {
            n00b_gopt_tok_windows_flag(state);
            return;
        }
        n00b_gopt_tok_word_or_bool(state);
        return;
    case ',':
        n00b_gopt_comma(state);
        return;
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
    case '.':
        n00b_gopt_tok_possible_number(state);
        return;
    default:
        n00b_gopt_tok_word_or_bool(state);
        return;
    }
}

static inline void
n00b_gopt_tok_main_loop(n00b_gopt_lex_state *state)
{
    int n = n00b_len(state->words);

    for (int i = 0; i < n; i++) {
        state->word_id     = i;
        state->raw_word    = n00b_list_get(state->words, i, NULL);
        state->word        = n00b_gopt_normalize(state, state->raw_word);
        state->cur_wordlen = n00b_string_codepoint_len(state->raw_word);

        if (state->force_word) {
            n00b_gopt_tok_word_or_bool(state);
            state->force_word = false;
            continue;
        }
        if (state->all_words) {
            continue;
        }
        n00b_gopt_tok_default_state(state);
    }

    n00b_gopt_tok_emit(state, N00B_TOK_EOF, 0, 0, 0, NULL);
}

// Words must be a list of strings. We turn them into a list of tokens
// that the Earley parser expects to see. We could pass all this to
// the Earley parser ourselves, but it's easier for me to not bother.
void
n00b_gopt_tokenize(n00b_gopt_ctx *ctx,
                   n00b_string_t *command_name,
                   n00b_list_t   *words)
{
    n00b_gopt_lex_state *state = n00b_gc_alloc_mapped(n00b_gopt_lex_state,
                                                      N00B_GC_SCAN_ALL);

    state->gctx         = ctx;
    state->command_name = command_name;
    state->words        = words;
    state->num_words    = n00b_list_len(words);
    ctx->tokens         = n00b_list(n00b_type_ref());

    n00b_gopt_tok_command_name(state);
    n00b_gopt_tok_main_loop(state);
}
