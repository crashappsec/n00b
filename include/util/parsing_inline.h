#pragma once
#include "n00b.h"

static inline n00b_pitem_t *
n00b_new_pitem(n00b_pitem_kind kind)
{
    n00b_pitem_t *result = n00b_gc_alloc_mapped(n00b_pitem_t, N00B_GC_SCAN_ALL);

    result->kind = kind;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_terminal_from_int(n00b_grammar_t *g, int64_t n)
{
    n00b_pitem_t    *result = n00b_new_pitem(N00B_P_TERMINAL);
    n00b_terminal_t *term;
    int              ix;

    result->contents.terminal = n;
    ix                        = n - N00B_START_TOK_ID;
    term                      = n00b_list_get(g->named_terms, ix, NULL);
    result->s                 = term->value;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_term_raw(n00b_grammar_t *p, n00b_string_t *name)
{
    n00b_pitem_t    *result   = n00b_new_pitem(N00B_P_TERMINAL);
    n00b_terminal_t *tok      = n00b_new(n00b_type_terminal(), p, name);
    result->contents.terminal = tok->id;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_from_nt(n00b_nonterm_t *nt)
{
    n00b_pitem_t *result     = n00b_new_pitem(N00B_P_NT);
    result->contents.nonterm = nt->id;
    result->s                = nt->name;

    return result;
}

static inline int64_t
n00b_grammar_add_term(n00b_grammar_t *g, n00b_string_t *s)
{
    n00b_pitem_t *pi = n00b_pitem_term_raw(g, s);
    return pi->contents.terminal;
}

static inline n00b_pitem_t *
n00b_pitem_terminal_cp(n00b_codepoint_t cp)
{
    n00b_pitem_t *result      = n00b_new_pitem(N00B_P_TERMINAL);
    result->contents.terminal = cp;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_nonterm_raw(n00b_grammar_t *p, n00b_string_t *name)
{
    n00b_pitem_t   *result   = n00b_new_pitem(N00B_P_NT);
    n00b_nonterm_t *nt       = n00b_new(n00b_type_ruleset(), p, name);
    result->contents.nonterm = nt->id;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_choice_raw(n00b_grammar_t *p, n00b_list_t *choices)
{
    n00b_pitem_t *result   = n00b_new_pitem(N00B_P_SET);
    result->contents.items = choices;

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_any_terminal_raw(n00b_grammar_t *p)
{
    n00b_pitem_t *result = n00b_new_pitem(N00B_P_ANY);

    return result;
}

static inline n00b_pitem_t *
n00b_pitem_builtin_raw(n00b_bi_class_t class)
{
    // Builtin character classes.
    n00b_pitem_t *result   = n00b_new_pitem(N00B_P_BI_CLASS);
    result->contents.class = class;

    return result;
}

static inline void
n00b_grammar_set_default_start(n00b_grammar_t *grammar, n00b_nonterm_t *nt)
{
    grammar->default_start = nt->id;
}

static inline void
n00b_ruleset_set_user_data(n00b_nonterm_t *nonterm, void *data)
{
    nonterm->user_data = data;
}

static inline void
n00b_terminal_set_user_data(n00b_terminal_t *term, void *data)
{
    term->user_data = data;
}

static inline n00b_table_t *
n00b_parse_tree_format(n00b_tree_node_t *t)
{
    return n00b_tree_format(t, n00b_repr_parse_node, NULL, false);
}

static inline void *
n00b_parse_get_user_data(n00b_grammar_t *g, n00b_parse_node_t *node)
{
    int64_t id;

    if (!node) {
        return NULL;
    }

    if (node->token) {
        id = node->info.token->tid;
        if (id < N00B_START_TOK_ID) {
            return NULL;
        }

        id -= N00B_START_TOK_ID;
        if (id >= n00b_list_len(g->named_terms)) {
            return NULL;
        }

        n00b_terminal_t *t = n00b_list_get(g->named_terms, id, NULL);

        return t->user_data;
    }

    id = node->id;

    if (id < 0 || id > n00b_list_len(g->rules)) {
        return NULL;
    }

    n00b_nonterm_t *nt = n00b_list_get(g->nt_list, id, NULL);

    if (!nt) {
        return NULL;
    }

    return nt->user_data;
}

#ifdef N00B_USE_INTERNAL_API
static inline bool
n00b_is_non_terminal(n00b_pitem_t *pitem)
{
    switch (pitem->kind) {
    case N00B_P_NT:
    case N00B_P_GROUP:
        return true;
    default:
        return false;
    }
}

static inline n00b_terminal_t *
n00b_get_terminal(n00b_grammar_t *g, int64_t id)
{
    return n00b_list_get(g->named_terms, id - N00B_START_TOK_ID, NULL);
}

static inline n00b_earley_cmp_t
n00b_earley_cost_cmp(n00b_earley_item_t *left, n00b_earley_item_t *right)
{
    int diff = left->penalty - right->penalty;
    if (diff) {
        return (diff < 0) ? N00B_EARLEY_CMP_LT : N00B_EARLEY_CMP_GT;
    }

    diff = left->cost - right->cost;
    if (diff) {
        return (diff < 0) ? N00B_EARLEY_CMP_LT : N00B_EARLEY_CMP_GT;
    }

    return N00B_EARLEY_CMP_EQ;
}

static inline void
n00b_nonterm_set_walk_action(n00b_nonterm_t *nt, n00b_parse_action_t action)
{
    nt->action = action;
}

static inline n00b_earley_state_t *
n00b_new_earley_state(int id)
{
    n00b_earley_state_t *result;

    result = n00b_gc_alloc_mapped(n00b_earley_state_t, N00B_GC_SCAN_ALL);

    // List of n00b_earley_item_t objects.
    result->items = n00b_list(n00b_type_ref());
    result->id    = id;

    return result;
}

static inline bool
n00b_hide_groups(n00b_grammar_t *g)
{
    return g->hide_groups && !g->suspend_group_hiding;
}

static inline bool
n00b_hide_penalties(n00b_grammar_t *g)
{
    return g->hide_penalty_rewrites & !g->suspend_penalty_hiding;
}

#endif
