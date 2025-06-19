#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_string_t *
group_format_via_rule(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo);
static n00b_string_t *
group_format_via_nt(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo);

n00b_string_t *
n00b_repr_parse_node(n00b_parse_node_t *n)
{
    if (n->token) {
        if (n->id == N00B_EMPTY_STRING) {
            return n00b_cformat("ε («#»)", n->start);
        }

        n00b_string_t *start = n00b_repr_token_info(n->info.token);
        n00b_string_t *end   = n00b_cformat(
            "«em1»(«#0»-«#1»)«/» id #«#2»",
            n->start,
            n->end,
            n->id);
        return n00b_string_concat(start, end);
    }

    n00b_string_t *name    = n->info.name;
    n00b_string_t *penalty = n00b_cached_minus();
    n00b_string_t *empty   = n00b_cached_empty_string();

    if (n->id == N00B_EMPTY_STRING) {
        return n00b_cformat("ε («#»)", n->start);
    }

    n00b_string_t *rest = n00b_cformat(
        " «em1»(«#0»-«#1»)«/» id #«#0» «em1»«#1»«#2»«/»",
        n->start,
        n->end,
        n->id,
        n->penalty ? penalty : empty,
        n->penalty);

    if (n->cost != 0) {
        n00b_string_t *cost = n00b_cformat(" «em6»cost = «#»«/» ", n->cost);
        rest                = n00b_string_concat(rest, cost);
    }

    return n00b_string_concat(name, rest);
}

n00b_string_t *
n00b_repr_term(n00b_grammar_t *grammar, int64_t id)
{
    if (id < N00B_START_TOK_ID) {
        if (n00b_codepoint_has_width(id)) {
            char buf[10] = {
                '\'',
            };
            int l      = utf8proc_encode_char((n00b_codepoint_t)id,
                                         (void *)&buf[1]);
            buf[l + 1] = '\'';
            return n00b_cstring(buf);
        }
        return n00b_cformat("'U+«:x»'", id);
    }

    id -= N00B_START_TOK_ID;
    n00b_terminal_t *t = n00b_list_get(grammar->named_terms, id, NULL);

    if (!t) {
        N00B_CRAISE(
            "Invalid parse token ID found (mixing "
            "rules from a different grammar?");
    }

    return n00b_cformat("'«#»'", t->value);
}

n00b_string_t *
n00b_repr_nonterm(n00b_grammar_t *grammar, int64_t id, bool show)
{
    // Rewrite this.
    n00b_nonterm_t *nt = n00b_get_nonterm(grammar, id);

    if (!nt) {
        if (id == N00B_GID_SHOW_GROUP_LHS) {
            return n00b_crich("«em2»>>group<<  «/»");
        }
        else {
            return n00b_crich("«em2»>>item<<  «/»");
        }
    }

    n00b_string_t *null = (show && nt->nullable)
                            ? n00b_cstring(" (∅-able)")
                            : n00b_cached_empty_string();

    if (id == grammar->default_start) {
        return n00b_cformat("«em3»«#0»«/»«#1»", nt->name, null);
    }

    if (n00b_hide_penalties(grammar)) {
        for (int i = 0; i < n00b_list_len(nt->rules); i++) {
            n00b_parse_rule_t *r = n00b_list_get(nt->rules, i, NULL);

            n00b_pitem_t *sub = n00b_list_get(r->contents, 0, NULL);
            if (sub->kind != N00B_P_NT && sub->kind != N00B_P_NULL) {
                return n00b_parse_repr_item(grammar, sub);
            }
        }
    }

    return n00b_cformat("«em2»«#0»«/»«#1»", nt->name, null);
}

n00b_string_t *
n00b_repr_nonterm_lhs(n00b_grammar_t *g, n00b_nonterm_t *nt)
{
    if (nt->id == g->default_start) {
        return n00b_cformat("«em3»«#0»«/»", nt->name);
    }
    else {
        return n00b_cformat("«em2»«#0»«/»", nt->name);
    }
}

static inline bool
should_hide_group(n00b_grammar_t *g, n00b_rule_group_t *group)
{
    if (!g->hide_groups) {
        return false;
    }
    if (g->suspend_group_hiding) {
        return false;
    }

    return true;
}
n00b_string_t *
n00b_repr_group(n00b_grammar_t *g, n00b_rule_group_t *group)
{
    n00b_string_t *base;

    if (should_hide_group(g, group)) {
        n00b_nonterm_t    *nt = group->contents;
        n00b_parse_rule_t *pr = n00b_list_get(nt->rules, 0, NULL);

        base = n00b_repr_rule(g, pr->contents, -1);
    }
    else {
        base = n00b_repr_nonterm(g, group->contents->id, false);
    }

    if (group->min == 1 && group->max == 1) {
        return n00b_cformat("(«em4»«#0»«/»)", base);
    }

    if (group->min == 0 && group->max == 1) {
        return n00b_cformat("(«em3»«#0»«/»)?", base);
    }

    if (group->max < 1 && group->min <= 1) {
        if (group->min == 0) {
            return n00b_cformat("(«em4»«#0»«/»)*", base);
        }
        else {
            return n00b_cformat("(«em4»«#0»«/»)+",
                                base);
        }
    }
    else {
        return n00b_cformat("(«#0»)«#1»«#2», «#3»]",
                            base,
                            n00b_cached_lbracket(),
                            group->min,
                            group->max);
    }
}

n00b_string_t *
n00b_parse_repr_item(n00b_grammar_t *g, n00b_pitem_t *item)
{
    switch (item->kind) {
    case N00B_P_NULL:
        return n00b_cstring("ε");
    case N00B_P_GROUP:
        if (g->hide_groups && !g->suspend_group_hiding) {
            return group_format_via_rule(g, item->contents.group);
        }
        else {
            return group_format_via_nt(g, item->contents.group);
        }

        //        return n00b_repr_group(g, item->contents.group);
    case N00B_P_NT:
        return n00b_repr_nonterm(g, item->contents.nonterm, false);
    case N00B_P_TERMINAL:
        return n00b_repr_term(g, item->contents.terminal);
    case N00B_P_ANY:
        return n00b_cstring("«Any»");
    case N00B_P_BI_CLASS:
        switch (item->contents.class) {
        case N00B_P_BIC_ID_START:
            return n00b_cstring("«IdStart»");
        case N00B_P_BIC_ID_CONTINUE:
            return n00b_cstring("«IdContinue»");
        case N00B_P_BIC_N00B_ID_START:
            return n00b_cstring("«N00bIdStart»");
        case N00B_P_BIC_N00B_ID_CONTINUE:
            return n00b_cstring("«N00bIdContinue»");
        case N00B_P_BIC_DIGIT:
            return n00b_cstring("«Digit»");
        case N00B_P_BIC_ANY_DIGIT:
            return n00b_cstring("«UDigit»");
        case N00B_P_BIC_UPPER:
            return n00b_cstring("«Upper»");
        case N00B_P_BIC_UPPER_ASCII:
            return n00b_cstring("«AsciiUpper»");
        case N00B_P_BIC_LOWER:
            return n00b_cstring("«Lower»");
        case N00B_P_BIC_LOWER_ASCII:
            return n00b_cstring("«AsciiLower»");
        case N00B_P_BIC_SPACE:
            return n00b_cstring("«WhiteSpace»");
        case N00B_P_BIC_JSON_string_CONTENTS:
            return n00b_cstring("«JsonStrChr»");
        case N00B_P_BIC_HEX_DIGIT:
            return n00b_cstring("«HexDigit»");
        case N00B_P_BIC_NONZERO_ASCII_DIGIT:
            return n00b_cstring("«NonZeroDigit»");
        default:
            return n00b_cstring("«?»");
        }
    case N00B_P_SET:;
        n00b_list_t *l = n00b_list(n00b_type_string());
        int          n = n00b_list_len(item->contents.items);

        for (int i = 0; i < n; i++) {
            n00b_list_append(l,
                             n00b_parse_repr_item(g,
                                                  n00b_list_get(item->contents.items,
                                                                i,
                                                                NULL)));
        }

        return n00b_cformat("[«#»]",
                            n00b_string_join(l, n00b_cstring("|")));
    default:
        n00b_unreachable();
    }
}

// This could be called boh5 stand-alone when printing out a grammar, and
// when printing out Earley states. In h5e former case, h5e dot should never
// show up (nor should h5e iteration count, but h5at's handled elsewhere).
//
// To h5at end, if dot_location is negative, we assume we're printing
// a grammar.

n00b_string_t *
n00b_repr_rule(n00b_grammar_t *g, n00b_list_t *items, int dot_location)
{
    n00b_list_t *pieces = n00b_list(n00b_type_string());
    int          n      = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        if (i == dot_location) {
            n00b_list_append(pieces, n00b_cstring("•"));
        }

        n00b_pitem_t *item = n00b_list_get(items, i, NULL);
        n00b_list_append(pieces, n00b_parse_repr_item(g, item));
    }

    if (n == dot_location) {
        n00b_list_append(pieces, n00b_cstring("•"));
    }

    return n00b_string_join(pieces, n00b_cached_space());
}

static inline n00b_string_t *
op_to_string(n00b_earley_op op)
{
    switch (op) {
    case N00B_EO_PREDICT_NT:
        return n00b_cstring("Predict (N)");
    case N00B_EO_PREDICT_G:
        return n00b_cstring("Predict (G)");
    case N00B_EO_COMPLETE_N:
        return n00b_cstring("Complete (N)");
    case N00B_EO_SCAN_TOKEN:
        return n00b_cstring("Scan (T)");
    case N00B_EO_SCAN_ANY:
        return n00b_cstring("Scan (A)");
    case N00B_EO_SCAN_CLASS:
        return n00b_cstring("Scan (C)");
    case N00B_EO_SCAN_SET:
        return n00b_cstring("Scan (S)");
    case N00B_EO_SCAN_NULL:
        return n00b_cstring("Scan (N)");
    case N00B_EO_ITEM_END:
        return n00b_cstring("Item End");
    case N00B_EO_FIRST_GROUP_ITEM:
        return n00b_cstring("Start Group");
    default:
        n00b_unreachable();
    }
}

static inline n00b_string_t *
repr_subtree_info(n00b_subtree_info_t si)
{
    switch (si) {
    case N00B_SI_NONE:
        return n00b_cached_space();
    case N00B_SI_NT_RULE_START:
        return n00b_cstring("⊤");
    case N00B_SI_NT_RULE_END:
        return n00b_cstring("⊥");
    case N00B_SI_GROUP_START:
        return n00b_cstring("g⊤");
    case N00B_SI_GROUP_END:
        return n00b_cstring("⊥g");
    case N00B_SI_GROUP_ITEM_START:
        return n00b_cstring("i⊤");
    case N00B_SI_GROUP_ITEM_END:
        return n00b_cstring("⊥i");
    default:
        n00b_unreachable();
    }
}

// This returns a list of table elements.
n00b_list_t *
n00b_repr_earley_item(n00b_parser_t *parser, n00b_earley_item_t *cur, int id)
{
    n00b_list_t    *result      = n00b_list(n00b_type_string());
    n00b_grammar_t *g           = parser->grammar;
    bool            last_state  = false;
    bool            first_state = false;

    if (n00b_list_len(parser->states) - 2 == cur->estate_id) {
        last_state = true;
    }
    if (!cur->estate_id) {
        first_state = true;
    }

    n00b_list_append(result, n00b_string_from_int(id));

    n00b_string_t      *nt;
    n00b_string_t      *rule;
    n00b_earley_item_t *start = cur->start_item;

    if (cur->double_dot) {
        nt                    = n00b_repr_nonterm(g,
                               N00B_GID_SHOW_GROUP_LHS,
                               true);
        n00b_parse_rule_t *pr = n00b_list_get(start->group->contents->rules,
                                              0,
                                              NULL);
        rule                  = n00b_repr_rule(g, pr->contents, cur->cursor);
    }
    else {
        nt   = n00b_repr_nonterm(g, start->ruleset_id, true);
        rule = n00b_repr_rule(g, start->rule->contents, cur->cursor);
    }

    if (start->double_dot) {
        if (cur->cursor == 0) {
            rule = n00b_string_concat(n00b_cstring("•"), rule);
        }
        else {
            rule = n00b_string_concat(rule, n00b_cstring("•"));
        }
    }

    n00b_string_t *full = n00b_cformat("«#0» ⟶  «#1»", nt, rule);

    if (last_state && cur->cursor == n00b_list_len(cur->rule->contents)) {
        if (parser->start == cur->rule->nt->id) {
            full = n00b_cformat("«em4»«#0»«/»", full);
        }
    }

    if (first_state && !cur->cursor && parser->start == cur->rule->nt->id) {
        full = n00b_cformat("«em4»«#0»«/»", full);
    }

    n00b_list_append(result, full);

    if (cur->group || cur->group_top) {
        n00b_list_append(result, n00b_string_from_int(cur->match_ct));
    }
    else {
        n00b_list_append(result, n00b_cached_space());
    }

    n00b_string_t *links;
    n00b_list_t   *clist = n00b_set_items(start->parent_states);
    int            n     = n00b_list_len(clist);

    if (!n) {
        links = n00b_cformat(" «i»Predicted by:«/» «#0»",
                             n00b_cstring("«Root» "));
    }

    else {
        links = n00b_crich(" «i»Predicted by:«/»");

        for (int i = 0; i < n; i++) {
            n00b_earley_item_t *rent = n00b_list_get(clist, i, NULL);

            n00b_string_t *s = n00b_cformat(" «#0»:«#1»",
                                            rent->estate_id,
                                            rent->eitem_index);
            links            = n00b_string_concat(links, s);
        }
    }

    n = 0;

    if (cur->completors) {
        clist = n00b_set_items(cur->completors);
        n     = n00b_list_len(clist);
    }
    if (n) {
        links = n00b_string_concat(links,
                                   n00b_crich(" «i»Via Completion:«/» "));

        for (int i = 0; i < n; i++) {
            n00b_earley_item_t *ei = n00b_list_get(clist, i, NULL);
            n00b_string_t      *s  = n00b_cformat(" «#0»:«#1»",
                                            ei->estate_id,
                                            ei->eitem_index);
            links                  = n00b_string_concat(links, s);
        }
    }

    clist = n00b_set_items(cur->predictions);
    n     = n00b_list_len(clist);

    if (n) {
        links = n00b_string_concat(links, n00b_crich(" «i»Predictions:«/» "));

        for (int i = 0; i < n; i++) {
            n00b_earley_item_t *ei = n00b_list_get(clist, i, NULL);
            n00b_string_t      *s  = n00b_cformat(" «#0»:«#1»",
                                            ei->estate_id,
                                            ei->eitem_index);
            links                  = n00b_string_concat(links, s);
        }
    }

    if (cur->group && cur->subtree_info == N00B_SI_GROUP_ITEM_START) {
        n00b_earley_item_t *prev = start->previous_scan;
        if (prev) {
            prev = prev->start_item;
            n00b_string_t *pstr;

            pstr  = n00b_cformat(" «i»Prev item end:«/» «#0»:«#1»",
                                prev->estate_id,
                                prev->eitem_index);
            links = n00b_string_concat(links, pstr);
        }
    }

    if (cur->previous_scan) {
        n00b_string_t *scan;
        scan  = n00b_cformat(" «i»Prior scan:«/» «#0»:«#1»",
                            cur->previous_scan->estate_id,
                            cur->previous_scan->eitem_index);
        links = n00b_string_concat(scan, links);
    }

    if (start) {
        links = n00b_string_concat(
            links,
            n00b_cformat(" «i»Node Location:«/» «#0»:«#1»",
                         cur->start_item->estate_id,
                         cur->start_item->eitem_index));
    }

    if (cur->penalty) {
        links = n00b_string_concat(links,
                                   n00b_cformat(" «em2»-«#0»«/»",
                                                cur->penalty));
    }

    if (cur->group) {
        links = n00b_string_concat(links,
                                   n00b_cformat(" (gid: «#0:x»)",
                                                cur->ruleset_id));
    }

    links = n00b_string_strip(links);

    n00b_list_append(result, links);
    n00b_list_append(result, op_to_string(cur->op));
    n00b_list_append(result, repr_subtree_info(cur->subtree_info));

    return result;
}

static inline n00b_string_t *
format_rule_items(n00b_grammar_t *g, n00b_parse_rule_t *pr, int dot_loc);

static n00b_string_t *
add_min_max_info(n00b_string_t *base, n00b_rule_group_t *group)
{
    if (group->min == 1 && group->max == 1) {
        return n00b_cformat("(«em4»«#0»«/»)", base);
    }
    if (group->min == 0 && group->max == 1) {
        return n00b_cformat("(«em4»«#0»«/»)?", base);
    }

    if (group->max < 1 && group->min <= 1) {
        if (group->min == 0) {
            return n00b_cformat("(«em4»«#0»«/»)*", base);
        }
        else {
            return n00b_cformat("(«em4»«#»«/»)+",
                                base);
        }
    }
    else {
        return n00b_cformat("(«em4»«#»«/»)«#»«#», «#»»",
                            base,
                            n00b_cached_lbracket(),
                            group->min,
                            group->max);
    }
}

static n00b_string_t *
group_format_via_rule(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo)
{
    n00b_nonterm_t    *nt   = ginfo->contents;
    n00b_parse_rule_t *pr   = n00b_list_get(nt->rules, 0, NULL);
    n00b_string_t     *repr = format_rule_items(grammar, pr, -1);

    return add_min_max_info(repr, ginfo);
}

static n00b_string_t *
group_format_via_nt(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo)
{
    n00b_string_t *nt_repr = n00b_repr_nonterm_lhs(grammar, ginfo->contents);
    return add_min_max_info(nt_repr, ginfo);
}

static inline n00b_string_t *
format_rule_items(n00b_grammar_t *g, n00b_parse_rule_t *pr, int dot_location)
{
    n00b_list_t *pieces = n00b_list(n00b_type_string());
    int          n      = n00b_list_len(pr->contents);

    for (int i = 0; i < n; i++) {
        if (i == dot_location) {
            n00b_list_append(pieces, n00b_cstring("•"));
        }

        // Current issue in getopts: pulling an NT here not a pitem.
        n00b_pitem_t *item = n00b_list_get(pr->contents, i, NULL);
        n00b_list_append(pieces, n00b_parse_repr_item(g, item));
    }

    if (n == dot_location) {
        n00b_list_append(pieces, n00b_cstring("•"));
    }

    return n00b_string_join(pieces, n00b_cached_space());
}

n00b_list_t *
n00b_format_one_production(n00b_grammar_t *g, n00b_parse_rule_t *pr)
{
    if (g->hide_penalty_rewrites && pr->penalty_rule) {
        return NULL;
    }

    n00b_list_t   *result = n00b_list(n00b_type_ref());
    n00b_string_t *lhs    = n00b_repr_nonterm_lhs(g, pr->nt);
    n00b_string_t *arrow  = n00b_crich("«em2»⟶ ");
    n00b_string_t *rhs    = format_rule_items(g, pr, -1);
    n00b_string_t *cost   = n00b_cached_empty_string();

    if (pr->cost != 0) {
        cost = n00b_cformat("«i»cost:«/» «u»«#»«/» ", pr->cost);
    }

    if (pr->penalty_rule) {
        cost = n00b_string_concat(cost, n00b_crich(" «u»(error)«/» "));
    }

    n00b_list_append(result, lhs);
    n00b_list_append(result, arrow);
    n00b_list_append(result, rhs);
    n00b_list_append(result, cost);

    return result;
}

n00b_list_t *
n00b_format_nt_productions(n00b_grammar_t *g, n00b_nonterm_t *nt)
{
    int          n      = n00b_list_len(nt->rules);
    n00b_list_t *result = n00b_list(n00b_type_ref());

    for (int i = 0; i < n; i++) {
        n00b_parse_rule_t *pr = n00b_list_get(nt->rules, i, NULL);
        n00b_assert(pr->nt == nt);
        n00b_list_t *one = n00b_format_one_production(g, pr);
        if (one != NULL) {
            n00b_list_append(result, one);
        }
    }

    return result;
}

n00b_table_t *
n00b_grammar_format(n00b_grammar_t *grammar)
{
    int32_t       n     = (int32_t)n00b_list_len(grammar->rules);
    n00b_table_t *table = n00b_new(n00b_type_table(),
                                   columns : 4,
                                   style : N00B_TABLE_FLOW);

    for (int32_t i = 0; i < n; i++) {
        n00b_parse_rule_t *pr = n00b_list_get(grammar->rules, i, NULL);

        if (!n00b_hide_groups(grammar) || !pr->nt->group_nt) {
            n00b_list_t *l = n00b_format_one_production(grammar, pr);
            if (l != NULL) {
                n00b_table_add_row(table, l);
            }
        }
    }

    return table;
}

n00b_string_t *
n00b_repr_token_info(n00b_token_info_t *tok)
{
    if (!tok) {
        return n00b_cstring("No token?");
    }
    if (tok->value) {
        return n00b_cformat(
            "«em3»«#»«/» "
            "«i»(tok #«#»; line #«#»; col #«#»; tid=«#»)«/»",
            tok->value,
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
    else {
        return n00b_cformat(
            "«em3»EOF«/» "
            "«i»(tok #«#»; line #«#»; col #«#»; tid=«#»)«/»",
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
}

n00b_table_t *
n00b_forest_format(n00b_list_t *trees)
{
    n00b_table_t *table = n00b_new(n00b_type_table(),
                                   columns : 1,
                                   style : N00B_TABLE_FLOW);

    int num_trees = n00b_list_len(trees);

    if (!num_trees) {
        return n00b_call_out(n00b_cstring("No valid parses."));
    }

    if (num_trees == 1) {
        return n00b_parse_tree_format(n00b_list_get(trees, 0, NULL));
    }

    n00b_table_add_cell(table,
                        n00b_cformat("«em1»«#» trees returned.", num_trees));

    for (int i = 0; i < num_trees; i++) {
        n00b_table_add_cell(table,
                            n00b_cformat("«em4»Parse #«#» of «#»",
                                         i + 1,
                                         num_trees));
        n00b_tree_node_t *t = n00b_list_get(trees, i, NULL);
        if (!t) {
            continue;
        }
        n00b_table_add_cell(table, n00b_parse_tree_format(t));
    }

    return table;
}

static inline void
add_highlights(n00b_parser_t *parser, n00b_list_t *row, int eix, int v)
{
    if (!parser->debug_highlights) {
        return;
    }

    int64_t     key = eix;
    int64_t     cur = v;
    n00b_set_t *s   = n00b_dict_get(parser->debug_highlights,
                                  (void *)key,
                                  NULL);

    if (!s) {
        return;
    }

    if (n00b_set_contains(s, (void *)cur)) {
        n00b_string_t *s0 = n00b_list_get(row, 0, NULL);
        n00b_string_t *s1 = n00b_list_get(row, 1, NULL);
        n00b_list_set(row, 0, n00b_cformat("«em4»«#»«/»", s0));
        n00b_list_set(row, 1, n00b_cformat("«em4»«#»«/»", s1));
    }
}

n00b_table_t *
n00b_repr_state_table(n00b_parser_t *parser, bool show_all)
{
    n00b_list_t *hdr = n00b_list(n00b_type_string());

    n00b_table_t *table = n00b_new(n00b_type_table(),
                                   columns : (show_all ? 6 : 4));

    n00b_table_next_column_fit(table);
    n00b_table_next_column_flex(table, 1);
    n00b_table_next_column_fit(table);
    n00b_table_next_column_flex(table, 1);

    n00b_list_append(hdr, n00b_crich("«em5»#"));
    n00b_list_append(hdr, n00b_crich("«em5»Rule State"));
    n00b_list_append(hdr, n00b_crich("«em5»Matches"));
    n00b_list_append(hdr, n00b_crich("«em5»Info"));

    if (show_all) {
        n00b_list_append(hdr, n00b_crich("«em5»Op"));
        n00b_list_append(hdr, n00b_crich("«em5»NI"));
    }

    int n = n00b_list_len(parser->states) - 1;

    for (int i = 0; i < n; i++) {
        n00b_earley_state_t *s = n00b_list_get(parser->states, i, NULL);
        n00b_string_t       *desc;

        if (i + 1 == n) {
            desc = n00b_cformat("«em2»State «#»«/» (End)", i);
        }
        else {
            desc = n00b_cformat(
                "«em2»«u»State «reverse»«#»«/reverse» "
                "Input: «reverse»«#»«/reverse»«em2»«u» «/»",
                i,
                s->token->value);
        }

        n00b_table_add_cell(table, desc, N00B_COL_SPAN_ALL);
        n00b_table_end_row(table);

        int m = n00b_list_len(s->items);

        n00b_table_add_row(table, hdr);

        for (int j = 0; j < m; j++) {
            n00b_earley_item_t *item     = n00b_list_get(s->items, j, NULL);
            int                 rule_len = n00b_list_len(item->rule->contents);

            if (show_all || rule_len == item->cursor) {
                n00b_list_t *row = n00b_repr_earley_item(parser, item, j);
                add_highlights(parser, row, i, j);
                n00b_table_add_row(table, row);
            }
        }
    }

    return table;
}
