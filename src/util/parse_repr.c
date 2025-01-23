#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_utf8_t *
group_format_via_rule(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo);
static n00b_utf8_t *
group_format_via_nt(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo);

n00b_utf8_t *
n00b_repr_parse_node(n00b_parse_node_t *n)
{
    if (n->token) {
        if (n->id == N00B_EMPTY_STRING) {
            return n00b_cstr_format("ε ({})", n->start);
        }

        n00b_utf8_t *start = n00b_repr_token_info(n->info.token);
        n00b_utf8_t *end   = n00b_cstr_format(
            "[atomic lime]({}-{})[/] id #{}",
            n->start,
            n->end,
            n->id);
        return n00b_str_concat(start, end);
    }

    n00b_utf8_t *name    = n->info.name;
    n00b_utf8_t *penalty = n00b_new_utf8("-");
    n00b_utf8_t *empty   = n00b_new_utf8("");

    if (n->id == N00B_EMPTY_STRING) {
        return n00b_cstr_format("ε ({})", n->start);
    }

    n00b_utf8_t *rest = n00b_cstr_format(
        " [atomic lime]({}-{})[/] id #{} [em]{}{}[/]",
        n->start,
        n->end,
        n->id,
        n->penalty ? penalty : empty,
        n->penalty);

    if (n->cost != 0) {
        n00b_utf8_t *cost = n00b_cstr_format(" [h6]cost = {}[/] ", n->cost);
        rest              = n00b_str_concat(rest, cost);
    }

    return n00b_str_concat(name, rest);
}

n00b_utf8_t *
n00b_repr_term(n00b_grammar_t *grammar, int64_t id)
{
    if (id < N00B_START_TOK_ID) {
        if (n00b_codepoint_is_printable(id)) {
            char buf[10] = {
                '\'',
            };
            int l      = utf8proc_encode_char((n00b_codepoint_t)id,
                                         (void *)&buf[1]);
            buf[l + 1] = '\'';
            return n00b_new_utf8(buf);
        }
        return n00b_cstr_format("'U+{:h}'", id);
    }

    id -= N00B_START_TOK_ID;
    n00b_terminal_t *t = n00b_list_get(grammar->named_terms, id, NULL);

    if (!t) {
        N00B_CRAISE(
            "Invalid parse token ID found (mixing "
            "rules from a different grammar?");
    }

    return n00b_cstr_format("'{}'", t->value);
}

n00b_utf8_t *
n00b_repr_nonterm(n00b_grammar_t *grammar, int64_t id, bool show)
{
    // Rewrite this.
    n00b_nonterm_t *nt = n00b_get_nonterm(grammar, id);

    if (!nt) {
        if (id == N00B_GID_SHOW_GROUP_LHS) {
            return n00b_rich_lit("[em]»group«  [/]");
        }
        else {
            return n00b_rich_lit("[em]»item«  [/]");
        }
    }

    n00b_utf8_t *null = (show && nt->nullable) ? n00b_new_utf8(" (∅-able)") : n00b_new_utf8("");

    if (id == grammar->default_start) {
        return n00b_cstr_format("[yellow i]{}[/]{}", nt->name, null);
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

    return n00b_cstr_format("[em]{}[/]{}", nt->name, null);
}

n00b_utf8_t *
n00b_repr_nonterm_lhs(n00b_grammar_t *g, n00b_nonterm_t *nt)
{
    if (nt->id == g->default_start) {
        return n00b_cstr_format("[yellow i]{}[/]", nt->name);
    }
    else {
        return n00b_cstr_format("[em]{}[/]", nt->name);
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
n00b_utf8_t *
n00b_repr_group(n00b_grammar_t *g, n00b_rule_group_t *group)
{
    n00b_utf8_t *base;

    if (should_hide_group(g, group)) {
        n00b_nonterm_t    *nt = group->contents;
        n00b_parse_rule_t *pr = n00b_list_get(nt->rules, 0, NULL);

        base = n00b_repr_rule(g, pr->contents, -1);
    }
    else {
        base = n00b_repr_nonterm(g, group->contents->id, false);
    }

    if (group->min == 1 && group->max == 1) {
        return n00b_cstr_format("([aqua]{}[/])", base);
    }

    if (group->min == 0 && group->max == 1) {
        return n00b_cstr_format("([aqua]{}[/])?", base);
    }

    if (group->max < 1 && group->min <= 1) {
        if (group->min == 0) {
            return n00b_cstr_format("([aqua]{}[/])*", base);
        }
        else {
            return n00b_cstr_format("([aqua]{}[/])+",
                                    base);
        }
    }
    else {
        return n00b_cstr_format("({}){}{}, {}]",
                                base,
                                n00b_new_utf8("["),
                                group->min,
                                group->max);
    }
}

n00b_utf8_t *
n00b_parse_repr_item(n00b_grammar_t *g, n00b_pitem_t *item)
{
    switch (item->kind) {
    case N00B_P_NULL:
        return n00b_new_utf8("ε");
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
        return n00b_new_utf8("«Any»");
    case N00B_P_BI_CLASS:
        switch (item->contents.class) {
        case N00B_P_BIC_ID_START:
            return n00b_new_utf8("«IdStart»");
        case N00B_P_BIC_ID_CONTINUE:
            return n00b_new_utf8("«IdContinue»");
        case N00B_P_BIC_N00B_ID_START:
            return n00b_new_utf8("«N00bIdStart»");
        case N00B_P_BIC_N00B_ID_CONTINUE:
            return n00b_new_utf8("«N00bIdContinue»");
        case N00B_P_BIC_DIGIT:
            return n00b_new_utf8("«Digit»");
        case N00B_P_BIC_ANY_DIGIT:
            return n00b_new_utf8("«UDigit»");
        case N00B_P_BIC_UPPER:
            return n00b_new_utf8("«Upper»");
        case N00B_P_BIC_UPPER_ASCII:
            return n00b_new_utf8("«AsciiUpper»");
        case N00B_P_BIC_LOWER:
            return n00b_new_utf8("«Lower»");
        case N00B_P_BIC_LOWER_ASCII:
            return n00b_new_utf8("«AsciiLower»");
        case N00B_P_BIC_SPACE:
            return n00b_new_utf8("«WhiteSpace»");
        case N00B_P_BIC_JSON_STR_CONTENTS:
            return n00b_new_utf8("«JsonStrChr»");
        case N00B_P_BIC_HEX_DIGIT:
            return n00b_new_utf8("«HexDigit»");
        case N00B_P_BIC_NONZERO_ASCII_DIGIT:
            return n00b_new_utf8("«NonZeroDigit»");
        default:
            return n00b_new_utf8("«?»");
        }
    case N00B_P_SET:;
        n00b_list_t *l = n00b_list(n00b_type_utf8());
        int          n = n00b_list_len(item->contents.items);

        for (int i = 0; i < n; i++) {
            n00b_list_append(l,
                             n00b_parse_repr_item(g,
                                                  n00b_list_get(item->contents.items,
                                                                i,
                                                                NULL)));
        }

        return n00b_cstr_format("{}{}]",
                                n00b_new_utf8("["),
                                n00b_str_join(l, n00b_new_utf8("|")));
    default:
        n00b_unreachable();
    }
}

// This could be called both stand-alone when printing out a grammar, and
// when printing out Earley states. In the former case, the dot should never
// show up (nor should the iteration count, but that's handled elsewhere).
//
// To that end, if dot_location is negative, we assume we're printing
// a grammar.

n00b_utf8_t *
n00b_repr_rule(n00b_grammar_t *g, n00b_list_t *items, int dot_location)
{
    n00b_list_t *pieces = n00b_list(n00b_type_utf8());
    int          n      = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        if (i == dot_location) {
            n00b_list_append(pieces, n00b_new_utf8("•"));
        }

        n00b_pitem_t *item = n00b_list_get(items, i, NULL);
        n00b_list_append(pieces, n00b_parse_repr_item(g, item));
    }

    if (n == dot_location) {
        n00b_list_append(pieces, n00b_new_utf8("•"));
    }

    return n00b_str_join(pieces, n00b_new_utf8(" "));
}

static inline n00b_utf8_t *
op_to_string(n00b_earley_op op)
{
    switch (op) {
    case N00B_EO_PREDICT_NT:
        return n00b_new_utf8("Predict (N)");
    case N00B_EO_PREDICT_G:
        return n00b_new_utf8("Predict (G)");
    case N00B_EO_COMPLETE_N:
        return n00b_new_utf8("Complete (N)");
    case N00B_EO_SCAN_TOKEN:
        return n00b_new_utf8("Scan (T)");
    case N00B_EO_SCAN_ANY:
        return n00b_new_utf8("Scan (A)");
    case N00B_EO_SCAN_CLASS:
        return n00b_new_utf8("Scan (C)");
    case N00B_EO_SCAN_SET:
        return n00b_new_utf8("Scan (S)");
    case N00B_EO_SCAN_NULL:
        return n00b_new_utf8("Scan (N)");
    case N00B_EO_ITEM_END:
        return n00b_new_utf8("Item End");
    case N00B_EO_FIRST_GROUP_ITEM:
        return n00b_new_utf8("Start Group");
    default:
        n00b_unreachable();
    }
}

static inline n00b_utf8_t *
repr_subtree_info(n00b_subtree_info_t si)
{
    switch (si) {
    case N00B_SI_NONE:
        return n00b_new_utf8(" ");
    case N00B_SI_NT_RULE_START:
        return n00b_new_utf8("⊤");
    case N00B_SI_NT_RULE_END:
        return n00b_new_utf8("⊥");
    case N00B_SI_GROUP_START:
        return n00b_new_utf8("g⊤");
    case N00B_SI_GROUP_END:
        return n00b_new_utf8("⊥g");
    case N00B_SI_GROUP_ITEM_START:
        return n00b_new_utf8("i⊤");
    case N00B_SI_GROUP_ITEM_END:
        return n00b_new_utf8("⊥i");
    default:
        n00b_unreachable();
    }
}

// This returns a list of table elements.
n00b_list_t *
n00b_repr_earley_item(n00b_parser_t *parser, n00b_earley_item_t *cur, int id)
{
    n00b_list_t    *result      = n00b_list(n00b_type_utf8());
    n00b_grammar_t *g           = parser->grammar;
    bool            last_state  = false;
    bool            first_state = false;

    if (n00b_list_len(parser->states) - 2 == cur->estate_id) {
        last_state = true;
    }
    if (!cur->estate_id) {
        first_state = true;
    }

    n00b_list_append(result, n00b_str_from_int(id));

    n00b_utf8_t        *nt;
    n00b_utf8_t        *rule;
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
            rule = n00b_str_concat(n00b_new_utf8("•"), rule);
        }
        else {
            rule = n00b_str_concat(rule, n00b_new_utf8("•"));
        }
    }

    n00b_utf8_t *full = n00b_cstr_format("{} ⟶  {}", nt, rule);

    if (last_state && cur->cursor == n00b_list_len(cur->rule->contents)) {
        if (parser->start == cur->rule->nt->id) {
            full = n00b_cstr_format("[yellow]{}[/]", full);
        }
    }

    if (first_state && !cur->cursor && parser->start == cur->rule->nt->id) {
        full = n00b_cstr_format("[yellow]{}[/]", full);
    }

    n00b_list_append(result, full);

    if (cur->group || cur->group_top) {
        n00b_list_append(result, n00b_str_from_int(cur->match_ct));
    }
    else {
        n00b_list_append(result, n00b_new_utf8(" "));
    }

    n00b_utf8_t         *links;
    uint64_t             n;
    n00b_earley_item_t **clist = hatrack_set_items_sort(start->parent_states,
                                                        &n);

    if (!n) {
        links = n00b_rich_lit(" [i]Predicted by:[/] «Root» ");
    }

    else {
        links = n00b_rich_lit(" [i]Predicted by:[/]");

        for (uint64_t i = 0; i < n; i++) {
            n00b_earley_item_t *rent = clist[i];

            n00b_utf8_t *s = n00b_cstr_format(" {}:{}",
                                              rent->estate_id,
                                              rent->eitem_index);
            links          = n00b_str_concat(links, s);
        }
    }

    n = 0;

    if (cur->completors) {
        clist = hatrack_set_items_sort(cur->completors, &n);
    }
    if (n) {
        links = n00b_str_concat(links, n00b_rich_lit(" [i]Via Completion:[/] "));

        for (uint64_t i = 0; i < n; i++) {
            n00b_earley_item_t *ei = clist[i];
            n00b_utf8_t        *s  = n00b_cstr_format(" {}:{}",
                                              ei->estate_id,
                                              ei->eitem_index);
            links                  = n00b_str_concat(links, s);
        }
    }

    clist = hatrack_set_items_sort(cur->predictions, &n);

    if (n) {
        links = n00b_str_concat(links, n00b_rich_lit(" [i]Predictions:[/] "));

        for (uint64_t i = 0; i < n; i++) {
            n00b_earley_item_t *ei = clist[i];
            n00b_utf8_t        *s  = n00b_cstr_format(" {}:{}",
                                              ei->estate_id,
                                              ei->eitem_index);
            links                  = n00b_str_concat(links, s);
        }
    }

    if (cur->group && cur->subtree_info == N00B_SI_GROUP_ITEM_START) {
        n00b_earley_item_t *prev = start->previous_scan;
        if (prev) {
            prev = prev->start_item;
            n00b_utf8_t *pstr;

            pstr  = n00b_cstr_format(" [i]Prev item end:[/] {}:{}",
                                    prev->estate_id,
                                    prev->eitem_index);
            links = n00b_str_concat(links, pstr);
        }
    }

    if (cur->previous_scan) {
        n00b_utf8_t *scan;
        scan  = n00b_cstr_format(" [i]Prior scan:[/] {}:{}",
                                cur->previous_scan->estate_id,
                                cur->previous_scan->eitem_index);
        links = n00b_str_concat(scan, links);
    }

    if (start) {
        links = n00b_str_concat(links,
                                n00b_cstr_format(" [i]Node Location:[/] {}:{}",
                                                 cur->start_item->estate_id,
                                                 cur->start_item->eitem_index));
    }

    if (cur->penalty) {
        links = n00b_str_concat(links,
                                n00b_cstr_format(" [em]-{}[/]", cur->penalty));
    }

    if (cur->group) {
        links = n00b_str_concat(links,
                                n00b_cstr_format(" (gid: {:x})",
                                                 cur->ruleset_id));
    }

    links = n00b_str_strip(links);

    n00b_list_append(result, n00b_to_utf8(links));
    n00b_list_append(result, op_to_string(cur->op));
    n00b_list_append(result, repr_subtree_info(cur->subtree_info));

    return result;
}

static inline n00b_utf8_t *
format_rule_items(n00b_grammar_t *g, n00b_parse_rule_t *pr, int dot_loc);

static n00b_utf8_t *
add_min_max_info(n00b_utf8_t *base, n00b_rule_group_t *group)
{
    if (group->min == 1 && group->max == 1) {
        return n00b_cstr_format("([aqua]{}[/])", base);
    }
    if (group->min == 0 && group->max == 1) {
        return n00b_cstr_format("([aqua]{}[/])?", base);
    }

    if (group->max < 1 && group->min <= 1) {
        if (group->min == 0) {
            return n00b_cstr_format("([aqua]{}[/])*", base);
        }
        else {
            return n00b_cstr_format("([aqua]{}[/])+",
                                    base);
        }
    }
    else {
        return n00b_cstr_format("([aqua]{}[/]){}{}, {}]",
                                base,
                                n00b_new_utf8("["),
                                group->min,
                                group->max);
    }
}

static n00b_utf8_t *
group_format_via_rule(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo)
{
    n00b_nonterm_t    *nt   = ginfo->contents;
    n00b_parse_rule_t *pr   = n00b_list_get(nt->rules, 0, NULL);
    n00b_utf8_t       *repr = format_rule_items(grammar, pr, -1);

    return add_min_max_info(repr, ginfo);
}

static n00b_utf8_t *
group_format_via_nt(n00b_grammar_t *grammar, n00b_rule_group_t *ginfo)
{
    n00b_utf8_t *nt_repr = n00b_repr_nonterm_lhs(grammar, ginfo->contents);
    return add_min_max_info(nt_repr, ginfo);
}

static inline n00b_utf8_t *
format_rule_items(n00b_grammar_t *g, n00b_parse_rule_t *pr, int dot_location)
{
    n00b_list_t *pieces = n00b_list(n00b_type_utf8());
    int          n      = n00b_list_len(pr->contents);

    for (int i = 0; i < n; i++) {
        if (i == dot_location) {
            n00b_list_append(pieces, n00b_new_utf8("•"));
        }

        // Current issue in getopts: pulling an NT here not a pitem.
        n00b_pitem_t *item = n00b_list_get(pr->contents, i, NULL);
        n00b_list_append(pieces, n00b_parse_repr_item(g, item));
    }

    if (n == dot_location) {
        n00b_list_append(pieces, n00b_new_utf8("•"));
    }

    return n00b_str_join(pieces, n00b_new_utf8(" "));
}

n00b_list_t *
n00b_format_one_production(n00b_grammar_t *g, n00b_parse_rule_t *pr)
{
    if (g->hide_penalty_rewrites && pr->penalty_rule) {
        return NULL;
    }

    n00b_list_t *result = n00b_list(n00b_type_ref());
    n00b_utf8_t *lhs    = n00b_repr_nonterm_lhs(g, pr->nt);
    n00b_utf8_t *arrow  = n00b_rich_lit("[yellow]⟶ ");
    n00b_utf8_t *rhs    = format_rule_items(g, pr, -1);
    n00b_utf8_t *cost   = n00b_new_utf8("");

    if (pr->cost != 0) {
        cost = n00b_cstr_format("[i]cost:[/] [u]{}[/] ", pr->cost);
    }

    if (pr->penalty_rule) {
        cost = n00b_str_concat(cost, n00b_rich_lit(" [u](error)[/] "));
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
        assert(pr->nt == nt);
        n00b_list_t *one = n00b_format_one_production(g, pr);
        if (one != NULL) {
            n00b_list_append(result, one);
        }
    }

    return result;
}

n00b_grid_t *
n00b_grammar_format(n00b_grammar_t *grammar)
{
    int32_t n = (int32_t)n00b_list_len(grammar->rules);

    n00b_grid_t *grid = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_cols",
                                         n00b_ka(4),
                                         "header_rows",
                                         n00b_ka(0),
                                         "container_tag",
                                         n00b_ka(n00b_new_utf8("flow"))));

    n00b_utf8_t *snap = n00b_new_utf8("snap");
    n00b_set_column_style(grid, 0, snap);
    n00b_set_column_style(grid, 1, snap);
    n00b_set_column_style(grid, 2, snap);
    n00b_set_column_style(grid, 3, snap);

    for (int32_t i = 0; i < n; i++) {
        n00b_parse_rule_t *pr = n00b_list_get(grammar->rules, i, NULL);

        if (!n00b_hide_groups(grammar) || !pr->nt->group_nt) {
            n00b_list_t *l = n00b_format_one_production(grammar, pr);
            if (l != NULL) {
                n00b_grid_add_row(grid, l);
            }
        }
    }

    return grid;
}

n00b_utf8_t *
n00b_repr_token_info(n00b_token_info_t *tok)
{
    if (!tok) {
        return n00b_new_utf8("No token?");
    }
    if (tok->value) {
        return n00b_cstr_format(
            "[h3]{}[/] "
            "[i](tok #{}; line #{}; col #{}; tid={})[/]",
            tok->value,
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
    else {
        return n00b_cstr_format(
            "[h3]EOF[/] "
            "[i](tok #{}; line #{}; col #{}; tid={})[/]",
            tok->index,
            tok->line,
            tok->column,
            tok->tid);
    }
}

n00b_grid_t *
n00b_forest_format(n00b_list_t *trees)
{
    int num_trees = n00b_list_len(trees);

    if (!num_trees) {
        return n00b_callout(n00b_new_utf8("No valid parses."));
    }

    if (num_trees == 1) {
        return n00b_parse_tree_format(n00b_list_get(trees, 0, NULL));
    }

    n00b_list_t *glist = n00b_list(n00b_type_grid());
    n00b_grid_t *cur;

    cur = n00b_new_cell(n00b_cstr_format("{} trees returned.", num_trees),
                        n00b_new_utf8("h1"));

    n00b_list_append(glist, cur);

    for (int i = 0; i < num_trees; i++) {
        cur = n00b_new_cell(n00b_cstr_format("Parse #{} of {}", i + 1, num_trees),
                            n00b_new_utf8("h4"));
        n00b_list_append(glist, cur);
        n00b_tree_node_t *t = n00b_list_get(trees,
                                            i,
                                            NULL);
        if (!t) {
            continue;
        }
        n00b_list_append(glist, n00b_parse_tree_format(t));
    }

    return n00b_grid_flow_from_list(glist);
}

static inline void
add_highlights(n00b_parser_t *parser, n00b_list_t *row, int eix, int v)
{
    if (!parser->debug_highlights) {
        return;
    }

    int64_t     key = eix;
    int64_t     cur = v;
    n00b_set_t *s   = hatrack_dict_get(parser->debug_highlights,
                                     (void *)key,
                                     NULL);

    if (!s) {
        return;
    }

    if (hatrack_set_contains(s, (void *)cur)) {
        n00b_utf8_t *s0 = n00b_list_get(row, 0, NULL);
        n00b_utf8_t *s1 = n00b_list_get(row, 1, NULL);
        n00b_list_set(row, 0, n00b_cstr_format("[aqua]{}[/]", s0));
        n00b_list_set(row, 1, n00b_cstr_format("[aqua]{}[/]", s1));
    }
}

n00b_grid_t *
n00b_repr_state_table(n00b_parser_t *parser, bool show_all)
{
    n00b_grid_t *grid = n00b_new(n00b_type_grid(),
                                 n00b_kw("start_cols",
                                         n00b_ka(show_all ? 6 : 4),
                                         "header_rows",
                                         n00b_ka(0),
                                         "container_tag",
                                         n00b_ka(n00b_new_utf8("table2")),
                                         "stripe",
                                         n00b_ka(true)));

    n00b_utf8_t *snap = n00b_new_utf8("snap");
    n00b_utf8_t *flex = n00b_new_utf8("flex");
    n00b_list_t *hdr  = n00b_new_table_row();
    n00b_list_t *row;

    n00b_list_append(hdr, n00b_rich_lit("[th]#"));
    n00b_list_append(hdr, n00b_rich_lit("[th]Rule State"));
    n00b_list_append(hdr, n00b_rich_lit("[th]Matches"));
    n00b_list_append(hdr, n00b_rich_lit("[th]Info"));

    if (show_all) {
        n00b_list_append(hdr, n00b_rich_lit("[th]Op"));
        n00b_list_append(hdr, n00b_rich_lit("[th]NI"));
    }

    int n = n00b_list_len(parser->states) - 1;

    for (int i = 0; i < n; i++) {
        n00b_earley_state_t *s = n00b_list_get(parser->states, i, NULL);
        n00b_utf8_t         *desc;

        if (i + 1 == n) {
            desc = n00b_cstr_format("[em]State {}[/] (End)", i);
        }
        else {
            desc = n00b_cstr_format(
                "[em u]State [reverse]{}[/reverse] "
                "Input: [reverse]{}[/reverse][em u] [/]",
                i,
                s->token->value);
        }

        n00b_list_t *l = n00b_new_table_row();
        n00b_list_append(l, n00b_new_utf8(""));
        n00b_list_append(l, desc);

        n00b_grid_add_row(grid, l);

        int m = n00b_list_len(s->items);

        n00b_grid_add_row(grid, hdr);

        for (int j = 0; j < m; j++) {
            n00b_earley_item_t *item     = n00b_list_get(s->items, j, NULL);
            int                 rule_len = n00b_list_len(item->rule->contents);

            if (show_all || rule_len == item->cursor) {
                row = n00b_repr_earley_item(parser, item, j);
                add_highlights(parser, row, i, j);
                n00b_grid_add_row(grid, row);
            }
        }
    }

    n00b_set_column_style(grid, 0, snap);
    n00b_set_column_style(grid, 1, flex);
    n00b_set_column_style(grid, 2, snap);
    n00b_set_column_style(grid, 3, flex);
    if (show_all) {
        n00b_set_column_style(grid, 4, snap);
        n00b_set_column_style(grid, 5, snap);
    }

    return grid;
}
