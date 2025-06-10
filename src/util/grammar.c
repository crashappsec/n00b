#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This is the API for setting up grammars for our parser (parsing.c).
//
// Here is also where we handle setting up the parser object, since
// they go part and parcel together.
//
// You can reuse the same parser object to parse multiple items, as
// long as you properly reset between uses.
//
//
// Everything assumes single threaded access to data structures
// in this API.
//
// There's also a little bit of code in here for automatic
// tokenization for common scenarios. If that gets expanded
// significantly, it'll get broken out.

void
n00b_parser_reset(n00b_parser_t *parser)
{
    parser->states           = n00b_list(n00b_type_ref());
    parser->position         = 0;
    parser->start            = -1; // Use default start.
    parser->current_state    = n00b_new_earley_state(0);
    parser->next_state       = NULL;
    parser->user_context     = NULL;
    parser->token_cache      = NULL;
    parser->run              = false;
    parser->current_line     = 1;
    parser->current_column   = 0;
    parser->preloaded_tokens = false;
    // Leave the tokenizer if any, and ignore_escaped_newlines.

    n00b_list_append(parser->states, parser->current_state);
}

static void
n00b_parser_init(n00b_parser_t *parser, va_list args)
{
    parser->grammar = va_arg(args, n00b_grammar_t *);
    n00b_parser_reset(parser);
}

static void
n00b_grammar_init(n00b_grammar_t *grammar, va_list args)
{
    int32_t max_penalty         = N00B_DEFAULT_MAX_PARSE_PENALTY;
    bool    detect_errors       = true;
    bool    hide_error_rewrites = true;
    bool    hide_group_rewrites = true;

    n00b_karg_va_init(args);
    n00b_kw_int32("max_penalty", max_penalty);
    n00b_kw_bool("detect_errors", detect_errors);
    n00b_kw_bool("hide_error_rewrites", hide_error_rewrites);
    n00b_kw_bool("hide_group_rewrites", hide_group_rewrites);

    grammar->named_terms           = n00b_list(n00b_type_terminal());
    grammar->rules                 = n00b_list(n00b_type_ref());
    grammar->nt_list               = n00b_list(n00b_type_ruleset());
    grammar->nt_map                = n00b_dict(n00b_type_string(),
                                n00b_type_u64());
    grammar->terminal_map          = n00b_dict(n00b_type_string(),
                                      n00b_type_u64());
    grammar->error_rules           = detect_errors;
    grammar->max_penalty           = max_penalty;
    grammar->hide_penalty_rewrites = hide_error_rewrites;
    grammar->hide_groups           = hide_group_rewrites;
}

static void
n00b_terminal_init(n00b_terminal_t *terminal, va_list args)
{
    bool found;

    n00b_grammar_t *grammar = va_arg(args, n00b_grammar_t *);
    terminal->value         = va_arg(args, n00b_string_t *);

    // Special-case single character terminals to their codepoint.
    if (n00b_string_codepoint_len(terminal->value) == 1) {
        terminal->id = (int64_t)n00b_string_index(terminal->value, 0);
        return;
    }

    terminal->id = (int64_t)hatrack_dict_get(grammar->terminal_map,
                                             terminal->value,
                                             &found);

    if (!found) {
        int64_t n = n00b_list_len(grammar->named_terms);
        n00b_list_append(grammar->named_terms, terminal);

        // If there's a race condition, we may not get the slot we
        // think we're getting. And it's slightly possible we might
        // add the same terminal twice in parallel, in which case we
        // accept there might be a redundant item in the list.
        while (true) {
            n00b_terminal_t *t = n00b_list_get(grammar->named_terms, n, NULL);

            if (t == terminal || n00b_string_eq(t->value, terminal->value)) {
                n += N00B_START_TOK_ID;
                terminal->id = n;
                hatrack_dict_add(grammar->terminal_map,
                                 terminal->value,
                                 (void *)n);
                return;
            }
            n++;
        }
    }
}

static void
n00b_nonterm_init(n00b_nonterm_t *nonterm, va_list args)
{
    // With non-terminal addition, we have a similar race condition
    // with terminals. Here, however, we are going to stick rule state in
    // the objects, so we need to error if there are dupe rule names,
    // since we cannot currently bait-and-switch the object on the
    // call to n00b_new() (though that's a good feature to add).
    //
    // Note that a name of NULL indicates an anonymous node, which we
    // don't add to the dictionary, but we do add to the list. This is
    // used for groups.

    bool            found;
    n00b_string_t  *err;
    int64_t         n;
    n00b_grammar_t *grammar = va_arg(args, n00b_grammar_t *);
    nonterm->name           = va_arg(args, n00b_string_t *);
    nonterm->rules          = n00b_list(n00b_type_ref());

    if (nonterm->name == NULL) {
        // Anonymous / inline rule.
        n = n00b_list_len(grammar->nt_list);
        n00b_list_append(grammar->nt_list, nonterm);

        while (true) {
            n00b_nonterm_t *nt = n00b_list_get(grammar->nt_list, n, NULL);

            if (nt == nonterm) {
                nonterm->id = n;
                return;
            }

            n++;
        }
    }

    hatrack_dict_get(grammar->nt_map, nonterm->name, &found);

    if (found) {
bail:
        err = n00b_cformat("Duplicate ruleset name: [=em=][=#=][/]",
                           nonterm->name);
        N00B_RAISE(err);
    }

    n = n00b_list_len(grammar->nt_list);
    n00b_list_append(grammar->nt_list, nonterm);

    // If there's a race condition, we may not get the slot we
    // think we're getting. And it's slightly possible we might
    // add the same terminal twice in parallel, in which case we
    // accept there might be a redundant item in the list.
    while (true) {
        n00b_nonterm_t *nt = n00b_list_get(grammar->nt_list, n, NULL);
        if (!nt) {
            return;
        }

        if (nt == nonterm) {
            hatrack_dict_put(grammar->nt_map, nonterm->name, (void *)n);
            nonterm->id = n;

            return;
        }
        if (nt->name && n00b_string_eq(nt->name, nonterm->name)) {
            goto bail;
        }
    }
}

static bool is_nullable_rule(n00b_grammar_t *,
                             n00b_parse_rule_t *,
                             n00b_list_t *);

static bool
check_stack_for_nt(n00b_list_t *stack, int64_t ruleset)
{
    // Return true if 'ruleset' is in the stack, false otherwise.

    int64_t len = n00b_list_len(stack);
    for (int64_t i = 0; i < len; i++) {
        if (ruleset == (int64_t)n00b_list_get(stack, i, NULL)) {
            return true;
        }
    }

    return false;
}

static inline void
finalize_nullable(n00b_nonterm_t *nt, bool value)
{
    nt->nullable  = value;
    nt->finalized = true;

    if (nt->nullable && nt->group_nt) {
        N00B_CRAISE("Attempt to add a group where the group item can be empty.");
    }
}

static bool
is_nullable_nt(n00b_grammar_t *g, int64_t nt_id, n00b_list_t *stack)
{
    // do not allow ourselves to recurse. We simply claim
    // to be nullable in that case.
    if (stack && check_stack_for_nt(stack, nt_id)) {
        return true;
    }
    else {
        n00b_list_append(stack, (void *)nt_id);
    }

    n00b_nonterm_t *nt = n00b_get_nonterm(g, nt_id);

    if (nt->finalized) {
        return nt->nullable;
    }

    // A non-terminal is nullable if any of its individual rules are
    // nullable.
    n00b_parse_rule_t *cur_rule;
    bool               found_any_rule = false;
    int64_t            i              = 0;

    if (n00b_list_len(nt->rules) == 0) {
        finalize_nullable(nt, true);
        return true;
    }

    cur_rule = n00b_list_get(nt->rules, i++, NULL);

    while (cur_rule) {
        found_any_rule = true;
        if (is_nullable_rule(g, cur_rule, stack)) {
            finalize_nullable(nt, true);
            n00b_list_pop(stack);
            return true;
        }
        cur_rule = n00b_list_get(nt->rules, i++, NULL);
    }

    // If we got to the end of the list, either it was an empty production,
    // in which case this is nullable (and found_any_rule is true).
    //
    // Otherwise, no rules were nullable, so the rule set
    // (non-terminal) is not nullable.
    finalize_nullable(nt, !(found_any_rule));
    n00b_list_pop(stack);

    return nt->nullable;
}

// This is true if we can skip over the ENTIRE item.
// When we're looking at a rule like:
//
// A -> . B C
// We want to check to see if we can advance the dot past B, so we need
// to know that there is SOME rule that could cause us to skip B.
//
// So we need to check that B's rules are nullable, to see it we can advance
// A's rule one dot.

static bool
is_nullable_rule(n00b_grammar_t *g, n00b_parse_rule_t *rule, n00b_list_t *stack)
{
    int n = n00b_list_len(rule->contents);

    for (int i = 0; i < n; i++) {
        n00b_pitem_t *item = n00b_list_get(rule->contents, i, NULL);

        if (!n00b_is_nullable_pitem(g, item, stack)) {
            return false;
        }

        if (item->kind == N00B_P_NT) {
            if (check_stack_for_nt(stack, item->contents.nonterm)) {
                return false;
            }

            return is_nullable_nt(g, item->contents.nonterm, stack);
        }
    }

    return false;
}

bool
n00b_is_nullable_pitem(n00b_grammar_t *grammar,
                       n00b_pitem_t   *item,
                       n00b_list_t    *stack)
{
    // There's not a tremendous value in caching nullable here; it is
    // best cached in the non-terminal node. We recompute only when
    // adding rules.

    switch (item->kind) {
    case N00B_P_NULL:
        return true;
    case N00B_P_GROUP:
        // Groups MUST not be nullable, except by specifying 0 matches.
        // We assume here this constraint is checked elsewhere.
        return item->contents.group->min == 0;
    case N00B_P_NT:
        return is_nullable_nt(grammar, item->contents.nonterm, stack);
    default:
        return false;
    }
}

n00b_nonterm_t *
n00b_get_nonterm(n00b_grammar_t *grammar, int64_t id)
{
    if (id > n00b_list_len(grammar->nt_list) || id < 0) {
        return NULL;
    }

    n00b_nonterm_t *nt = n00b_list_get(grammar->nt_list, id, NULL);

    return nt;
}

n00b_nonterm_t *
n00b_pitem_get_ruleset(n00b_grammar_t *g, n00b_pitem_t *pi)
{
    return n00b_get_nonterm(g, pi->contents.nonterm);
}

// This creates an anonymous non-terminal, and adds the contained
// pitems as a rule. This allows us to easily model grouping rhs
// pieces in parentheses. For instance:
//
// x ::= dont_repeat (foo bar)+ also_no_repeats
//
// By creating an anonymous rule (say, ANON1) and rewriting the
// grammar as:
//
// x ::= dont_repeat ANON1+ also_no_repeats
// ANON1 ::= foo bar

n00b_pitem_t *
n00b_group_items(n00b_grammar_t *g, n00b_list_t *pitems, int min, int max)
{
    n00b_rule_group_t *group = n00b_gc_alloc_mapped(n00b_rule_group_t,
                                                    N00B_GC_SCAN_ALL);

    group->gid                = n00b_rand16();
    n00b_string_t  *tmp_name  = n00b_cformat("$$group_nt_[=#=]", group->gid);
    n00b_pitem_t   *tmp_nt_pi = n00b_pitem_nonterm_raw(g, tmp_name);
    n00b_nonterm_t *nt        = n00b_pitem_get_ruleset(g, tmp_nt_pi);

    nt->group_nt = true;

    n00b_ruleset_add_rule(g, nt, pitems, 0);

    group->contents = nt;
    group->min      = min;
    group->max      = max;

    n00b_pitem_t *result   = n00b_new_pitem(N00B_P_GROUP);
    result->contents.group = group;

    return result;
}

static const char *errstr =
    "The null string and truly empty productions are not allowed. "
    "However, you can use group match operators like '?' and '*' that "
    "may accept zero items for the same effect.";

static inline bool
pitems_eq(n00b_pitem_t *p1, n00b_pitem_t *p2)
{
    if (p1->kind != p2->kind) {
        return false;
    }

    switch (p1->kind) {
    case N00B_P_NULL:
    case N00B_P_ANY:
        return true;
    case N00B_P_TERMINAL:
        return p1->contents.terminal == p2->contents.terminal;
    case N00B_P_BI_CLASS:
        return p1->contents.class == p2->contents.class;
    case N00B_P_SET:
        if (n00b_len(p1->contents.items) != n00b_len(p2->contents.items)) {
            return false;
        }
        for (int i = 0; i < n00b_list_len(p1->contents.items); i++) {
            n00b_pitem_t *sub1 = n00b_list_get(p1->contents.items, i, NULL);
            n00b_pitem_t *sub2 = n00b_list_get(p2->contents.items, i, NULL);
            if (!pitems_eq(sub1, sub2)) {
                return false;
            }
        }
        return true;
    case N00B_P_NT:
        return p1->contents.nonterm == p2->contents.nonterm;
    case N00B_P_GROUP:
        return p1->contents.group == p2->contents.group;
    default:
        n00b_unreachable();
    }
}

static bool
rule_exists(n00b_nonterm_t *nt, n00b_parse_rule_t *new, n00b_parse_rule_t **oldp)
{
    int n = n00b_list_len(nt->rules);
    int l = n00b_list_len(new->contents);

    for (int i = 0; i < n; i++) {
        n00b_parse_rule_t *old = n00b_list_get(nt->rules, i, NULL);
        if (n00b_list_len(old->contents) != l) {
            continue;
        }
        for (int j = 0; j < l; j++) {
            n00b_pitem_t *pi_old = n00b_list_get(old->contents, j, NULL);
            n00b_pitem_t *pi_new = n00b_list_get(new->contents, j, NULL);
            if (!pitems_eq(pi_old, pi_new)) {
                goto next;
            }
        }

        if (oldp) {
            *oldp = old;
        }
        return true;

next:;
    }

    return false;
}

static n00b_parse_rule_t *
ruleset_add_rule_internal(n00b_grammar_t     *g,
                          n00b_nonterm_t     *ruleset,
                          n00b_list_t        *items,
                          int                 cost,
                          n00b_parse_rule_t  *penalty,
                          n00b_parse_rule_t **old,
                          n00b_string_t      *doc)
{
    if (g->finalized) {
        N00B_CRAISE("Cannot modify grammar after first parse w/o a reset.");
    }

    if (!n00b_list_len(items)) {
        N00B_CRAISE(errstr);
    }

    n00b_parse_rule_t *rule = n00b_gc_alloc_mapped(n00b_parse_rule_t,
                                                   N00B_GC_SCAN_ALL);
    rule->nt                = ruleset;
    rule->contents          = items;
    rule->cost              = cost;
    rule->penalty_rule      = penalty ? true : false;
    rule->link              = penalty;
    rule->doc               = doc;

    if (rule_exists(ruleset, rule, old)) {
        return NULL;
    }

    n00b_list_append(ruleset->rules, rule);
    n00b_list_append(g->rules, rule);

    return rule;
}

n00b_parse_rule_t *
_n00b_ruleset_add_rule(n00b_grammar_t *g,
                       n00b_nonterm_t *nt,
                       n00b_list_t    *items,
                       int             cost,
                       n00b_string_t  *doc)
{
    n00b_parse_rule_t *new;
    n00b_parse_rule_t *old = NULL;

    new = ruleset_add_rule_internal(g, nt, items, cost, NULL, &old, doc);

    if (new) {
        return new;
    }

    return old;
}

static inline void
create_one_error_rule_set(n00b_grammar_t *g, int rule_ix)

{
    // We're only going to handle single-token errors in these rules for now;
    // it could be useful to create all possible error rules in many cases,
    // especially with things like matching parens, but single rules with
    // dozens of tokens would pose a problem, and dealing with that would
    // over-complicate. Single-token detection is good enough.

    n00b_parse_rule_t *cur    = n00b_list_get(g->rules, rule_ix, NULL);
    int                n      = n00b_list_len(cur->contents);
    n00b_list_t       *l      = cur->contents;
    int                tok_ct = 0;
    n00b_pitem_t      *pi;

    if (cur->nt->no_error_rule) {
        return;
    }

    for (int i = 0; i < n; i++) {
        pi = n00b_list_get(l, i, NULL);

        if (n00b_is_non_terminal(pi)) {
            continue;
        }
        tok_ct++;

        n00b_string_t  *name   = n00b_cformat("$term-[=#0=]-[=#1=]-[=#2=]",
                                           cur->nt->name,
                                           rule_ix,
                                           tok_ct);
        n00b_pitem_t   *pi_err = n00b_pitem_nonterm_raw(g, name);
        n00b_nonterm_t *nt_err = n00b_pitem_get_ruleset(g, pi_err);
        n00b_list_t    *r      = n00b_list(n00b_type_ref());

        n00b_list_append(r, pi);
        n00b_parse_rule_t *ok = ruleset_add_rule_internal(g,
                                                          nt_err,
                                                          r,
                                                          0,
                                                          NULL,
                                                          NULL,
                                                          NULL);

        r = n00b_list(n00b_type_ref());
        n00b_list_append(r, n00b_new_pitem(N00B_P_NULL));
        ruleset_add_rule_internal(g, nt_err, r, 0, ok, NULL, NULL);

        r = n00b_list(n00b_type_ref());
        n00b_list_append(r, n00b_new_pitem(N00B_P_ANY));
        n00b_list_append(r, pi);
        ruleset_add_rule_internal(g, nt_err, r, 0, ok, NULL, NULL);

        n00b_list_set(l, i, pi_err);
    }
}

static int
cmp_rules_for_display_ordering(n00b_parse_rule_t **p1, n00b_parse_rule_t **p2)
{
    n00b_parse_rule_t *r1 = *p1;
    n00b_parse_rule_t *r2 = *p2;

    if (r1->nt && !r2->nt) {
        return -1;
    }

    if (!r1->nt && r2->nt) {
        return 1;
    }

    if (r1->nt) {
        int name_cmp = strcmp(r1->nt->name->data, r2->nt->name->data);

        if (name_cmp) {
            if (r1->nt->start_nt) {
                return -1;
            }
            if (r2->nt->start_nt) {
                return 1;
            }

            return name_cmp;
        }
    }

    if (r1->penalty_rule != r2->penalty_rule) {
        if (r1->penalty_rule) {
            return 1;
        }
        return -1;
    }

    int l1 = n00b_list_len(r1->contents);
    int l2 = n00b_list_len(r2->contents);

    if (l1 != l2) {
        return l2 - l1;
    }

    return r1->cost - r2->cost;
}

void
n00b_prep_first_parse(n00b_grammar_t *g)
{
    if (g->finalized) {
        return;
    }

    int n = n00b_list_len(g->nt_list);

    // Calculate nullability *before* we add in error rules.
    //
    // If we see 'nullable' set in a rule, we'll proactively advance
    // the cursor without expanding it or including the penalty /
    // cost, so we don't want to do this with token ellision errors.
    //
    // Those are forced to match explicitly.

    for (int i = 0; i < n; i++) {
        is_nullable_nt(g, i, n00b_list(n00b_type_u64()));
    }

    if (g->error_rules) {
        n = n00b_list_len(g->rules);

        for (int i = 0; i < n; i++) {
            create_one_error_rule_set(g, i);
        }
    }

    n00b_nonterm_t *start = n00b_get_nonterm(g, g->default_start);
    start->start_nt       = true;

    n00b_list_sort(g->rules, (n00b_sort_fn)cmp_rules_for_display_ordering);

    g->finalized = true;
}

int64_t
n00b_token_stream_codepoints(n00b_parser_t *parser, void **token_info)
{
    n00b_string_t    *str = (n00b_string_t *)parser->user_context;
    n00b_codepoint_t *p   = (n00b_codepoint_t *)str->data;

    if (parser->position >= str->codepoints) {
        return N00B_TOK_EOF;
    }

    return (int64_t)p[parser->position];
}

// In this variation, we have already done, say, a 'lex' pass, and
// have a list of string objects that we need to match against;
// we are passed strings only, and can pass user-defined info
// if needed.
int64_t
n00b_token_stream_strings(n00b_parser_t *parser, void **token_info)
{
    n00b_list_t   *list  = (n00b_list_t *)parser->user_context;
    n00b_string_t *value = n00b_list_get(list, parser->position, NULL);

    if (!value) {
        return N00B_TOK_EOF;
    }

    // If it's a one-character string, the token ID is the codepoint.
    // doesn't matter if we registered it or not.
    if (value->codepoints == 1) {
        return n00b_string_index(value, 0);
    }

    // Next, check to see if we registered the token, meaning it is an
    // explicitly named terminal symbol somewhere in the grammar.
    //
    // Since any registered tokens are non-zero, we can test for that to
    // determine if it's registered, instead of passing a bool.
    int64_t n = (int64_t)hatrack_dict_get(parser->grammar->terminal_map,
                                          value,
                                          NULL);

    parser->token_cache = value;

    if (n) {
        return (int64_t)n;
    }

    return N00B_TOK_OTHER;
}

static inline int
count_newlines(n00b_parser_t *parser, n00b_string_t *v, int *last_index)
{
    n00b_string_require_u32(v);
    n00b_codepoint_t *p      = v->u32_data;
    int               result = 0;

    for (int i = 0; i < v->codepoints; i++) {
        switch (p[i]) {
        case '\\':
            if (parser->ignore_escaped_newlines) {
                i++;
                continue;
            }
            else {
                continue;
            }
        case '\n':
            *last_index = i;
            result++;
        }
    }

    return result;
}

void
n00b_add_debug_highlight(n00b_parser_t *parser, int32_t eid, int32_t ix)
{
    if (!parser->debug_highlights) {
        parser->debug_highlights = n00b_dict(n00b_type_int(),
                                             n00b_type_set(n00b_type_int()));
    }

    int64_t key   = eid;
    int64_t value = ix;

    n00b_set_t *s = hatrack_dict_get(parser->debug_highlights,
                                     (void *)key,
                                     NULL);

    if (!s) {
        s = n00b_set(n00b_type_int());
        hatrack_dict_put(parser->debug_highlights, (void *)key, s);
    }

    hatrack_set_put(s, (void *)value);
}

void
n00b_parser_load_token(n00b_parser_t *parser)
{
    n00b_earley_state_t *state = parser->current_state;

    // Load the token for the *current* state; do NOT advance the
    // state. The current state object is expected to be initialized.
    //
    // If we were handed a list of n00b_token_info_t objects instead of
    // a callback, then we load info from a list store in token_cache,
    // reusing those token objects, until / unless we need to add an
    // EOF.

    if (parser->preloaded_tokens) {
        n00b_list_t *toks = (n00b_list_t *)parser->token_cache;
        bool         err;

        state->token = n00b_list_get(toks, parser->position, &err);

        if (err) {
            state->token = n00b_gc_alloc_mapped(n00b_token_info_t,
                                                N00B_GC_SCAN_ALL);

            state->token->tid = N00B_TOK_EOF;
        }

        state->token->index = state->id;

        return;
    }

    // In this branch, we don't have a pre-existing list of tokens. We
    // are either processing a single string as characters, or
    // processing a list of strings.
    //
    // We get the next token generically across those via an internal
    // callback.  Unlike the above case, the token_cache is used to
    // stash the literal value when needed.

    n00b_token_info_t *tok = n00b_gc_alloc_mapped(n00b_token_info_t,
                                                  N00B_GC_SCAN_ALL);

    state->token = tok;

    // Clear this to make sure we don't accidentally reuse.
    parser->token_cache = NULL;
    tok->tid            = (*parser->tokenizer)(parser, &tok->user_info);
    tok->line           = parser->current_line;
    tok->column         = parser->current_column;
    tok->index          = state->id;

    if (tok->tid == N00B_TOK_EOF) {
        return;
    }
    if (tok->tid == N00B_TOK_OTHER) {
        tok->value = parser->token_cache;
    }
    else {
        if (tok->tid < N00B_START_TOK_ID) {
            tok->value = n00b_string_from_codepoint(tok->tid);
        }
        else {
            n00b_terminal_t *ti = n00b_list_get(parser->grammar->named_terms,
                                                tok->tid - N00B_START_TOK_ID,
                                                0);
            if (ti->value) {
                tok->value = ti->value;
            }
            else {
                tok->value = parser->token_cache;
            }
        }
    }

    // If we have no value string associated with the token, then we
    // assume it's an invisible token of some sort, and are done.
    if (!tok->value || !tok->value->codepoints) {
        return;
    }

    // Otherwise, we assume the value should be used in advancing the
    // line / column info we keep.
    //
    // If the token has newlines in it, we advance the line count, and
    // base the column count on whatever was after the last newline.

    int last_nl_ix;
    int len      = n00b_string_codepoint_len(tok->value);
    int nl_count = count_newlines(parser, tok->value, &last_nl_ix);

    if (nl_count) {
        parser->current_line += nl_count;
        parser->current_column = len - last_nl_ix + 1;
    }
    else {
        parser->current_column += len;
    }
}

const n00b_vtable_t n00b_parser_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_parser_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};
const n00b_vtable_t n00b_grammar_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_grammar_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

const n00b_vtable_t n00b_terminal_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_terminal_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

const n00b_vtable_t n00b_nonterm_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_nonterm_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};
