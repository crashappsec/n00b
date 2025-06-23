// The Earley algorithm is only a *recognizer*; it doesn't build parse
// trees; we don't use the same approach for parse trees as a lot of
// people here, because:
//
// 1. I don't like the representation for sparse, packed forests; it's
//    hard for mere mortals to work with them;
// 2. I have a 'set' abstraction and am not afraid to use it, so I can
//    keep precise track of n:m mappings between states, and do not
//    have to go back and pick all related states.
//
// Also, right now, I'm not generating nodes as I parse; it's all
// moved to post-parse; see parse_tree.c.
//
// But we do collect the relationships betweeen states here that will
// help us generate the tree.
//
// For non-terminals, we basically track nodes based on two Earley
// states. Consider A rule like:
//
//   Paren  ⟶   '(' Add ')'
//
// When we start processing the `Add` during a scan, we are processing
// an Earley state associated with the above `Paren` rule. It will
// generate one or more new states. Some or all of those states may
// eventually lead to completions. The pairing of 'Add' start states
// with the associated completion state represents a node of type Add
// (of which, in an ambiguous grammar, there could be many valid
// spans). Any such node gets parented to the 2nd item in the Paren
// rule from which it spawned. But, the Paren rule too can be
// ambiguous, so will have the potential to have multiple start / end
// items.
//
// To make our lives easier, we explicitly record all predictions made
// directly from an earley state (generally there can be only one, but
// the way I handle groups, subsequent potential items are also
// considered prodictions of the top of the group). We also keep
// backlinks in Earley states to who predicted us.
//
// All of this gets more complex when we may need to accept an empty
// match. For that reason, we do NOT allow empty rules or empty string
// matches in the grammar. The only `nullable` rules we allow are for
// groups, where we allow things to match 0 items. That constraint
// helps make null processing much easier to get correct.
//
// Valid parses must have a completion in the very final
// state, where the 'start_item' associated with it is in state 0
// somewhere (and must be our start production).
//
// So, when we're done, we can proceed as follows:
//
// 1. Identify all valid end states.
//
// 2. Follow the chain to identify parent subtrees, knowing that those
//    nodes are definitely 'good'.
//
// 3. Work backwords looking at the chain of events to find the other
//    'good' subtrees.
//
// #3 is the hard thing.
//
// However, the more proper way is to identify where we need to
// complete subtrees, and then recursively apply the algorithm. To
// that end, we keep track of the 'prior scan' state. That way, we can
// find the top of a sub-tree; the bottom of it will be found
// somewhere in the Earley state above ours. This is obvious how to do
// when we scan a token, since there's no chance for ambiguity, and no
// cascades of completions. Getting this to work does require us to be
// able to disambiguate in the face of conflicting previous scans,
// which is why it is incredibly helpful not to allow null scans.
//
// Again, building trees from the states though is actually handled in
// parse_tree.c.
//
// The group feature obviously can be directly mapped to BNF, but
// I wanted to be able to handle [m, n] matches without having to
// do crazy grammar expansion. Therefore, I made groups basically
// a first-class part of the algorithm.
//
// They mostly works like regular non-terminals, except we keep a
// little bit of extra info around on the minimum and maximum number
// of matches we accept; if we hit the maximum, we currently keep
// predicting new items, but with a penalty. Conversely, if there is a
// minimum we have not hit, then we penalize 'completing' the
// top-level group. In between, whenever we get to the end of a group
// item, we will both predict a new group item AND complete the
// group. If the penalty gets too high, we stop that branch of our
// parse state.
//
// To handle 0-length matches, the first time we predict a group, we
// also immediately complete it.
//
// The group implementation basically uses anonymous non-terminals
// The only majordifference is we do a bit of accounting per above.
//
// The grammar API can transform grammar rules to do error detection
// automatically (an algorithm by Aho). Basically here, we add in
// productions that match mistakes, but assign those mistakes a
// 'penalty'.
//
// Since the Aho approach requires adding empty-string matches when
// something is omitted, I actually end up doing something slightly
// different. For instance, Aho would convert:
//
// Paren ⟶ '(' Add ')'
//
// To something like:
//
// Paren ⟶ Term_Lparen Add Term_Rparen
// Term_Lparen ⟶ '(' | <<Any> '(' | ε
// Term_Rparen ⟶ ')' | <<Any> ')' | ε
//
// And then would assess a penalty when selecting anything other than
// the bare token. Instead, I currently do something a bit
// weaker. Basically, the idea is you can transform a grammar to
// remove empty string matches (as long as your grammar doesn't accept an
// empty input, but that can also just be special cased).
//
// For instance, something like:
//
// Add ⟶ Add Term_Plus Mul
// Term_Plus ⟶ '+' | <<Any> '+' | ε
//
// Can be rewritten as:
// Add ⟶ Add '+' Mul
// Add ⟶ Add <<Any> '+' Mul
// Add ⟶ Add Mul
//
// Where the second two items are assessed a penalty.
//
// That's effectively what I'm doing (the rewrite is done in
// grammar.c), however:
//
// 1. I do some folding for ambiguity; such a penalty rule could end
//    up having the same structure as an existing rule, in which case
//    I don't add it.
//
// 2. If you've got rules with many tokens in a single, creating the
//    set of resulting rules is exponential. While that's both
//    precomputation, and not really done in any practical grammar
//    (meaning, some insane number of tokens), I don't particularly love
//    it.
//
// There are easy ways to rectify this, like rewriting the grammar
// into a normal form, where it would contain no more than 1 terminal
// and no more than 2 items. However, I don't do that right now,
// because I want the trees that people get to match the grammar as
// they wrote it, and making that mapping is less straightforward.
//
// So for the time being, I'm weakining the matching. Aho's algorithm
// clearly would produce a tree for the 'Paren' rule above if both
// parens are accientally omitted. However, I only produce rules that
// allow for a single omission.
//
// As an example, I *could* translate the Paren rule into the following:
// Paren ⟶ '(' Add ')'
// Paren ⟶  Add ')' # 1 omission
// Paren ⟶ '(' Add # 1 omission
// Paren ⟶ <<Any>> '(' Add ')' # 1 junk item
// Paren ⟶ '(' Add <<Any>> ')' # 1 junk item
// Paren ⟶ <<Any>> '(' Add <<Any>> ')' # 2 junk items.
// Paren ⟶  Add  # 2 omissions
//
// That would be equivolent to the Aho approach, where I properly
// remove nulls from the grammar.
//
// HOWEVER, I do NOT generate the last two rules; I only generate ones
// where there is one error captured.
//
// Therefore, the error recovery here isn't quite as good. BUT, it
// does have the big advantage of avoiding a bunch of unnecessary
// Earley state explosion. Frankly, that's a real-world problem that
// led me to this compromise.
//
// I've tried to make the penalty logic as clear as possible. But the
// idea is, penalty rules (and group misses) add penalty. We consider
// the penalty both when propogating down / up, but we do not
// double-count penalties (e.g., adding for a non-terminal on the way
// down and the way up).
//
// There's also a parallel notion called 'COST' which can be used for
// precedence. This is orthogonal to penalties really, but it works
// the same way. Items with lower costs are more desirable than higher
// cost items, just like lower penalties are more desirable than
// higher ones.
//
// However, no matter how high costs get, something with no penalty is
// in some sense 'correct' and 'allowed', so a completion with 1000
// cost, and 0 penalties is accepted over an otherwise equal
// completion with 0 cost and 1 penalty.

#define N00B_USE_INTERNAL_API
#define N00B_PARSE_DEBUG

#include "n00b.h"

static inline n00b_pitem_t *
get_rule_pitem(n00b_parse_rule_t *r, int i)
{
    return n00b_list_get(r->contents, i, NULL);
}

static inline n00b_pitem_t *
get_ei_pitem(n00b_earley_item_t *ei, int i)
{
    return get_rule_pitem(ei->rule, i);
}

static inline int
ei_rule_len(n00b_earley_item_t *ei)
{
    return n00b_list_len(ei->rule->contents);
}

static inline void
set_subtree_info(n00b_parser_t *p, n00b_earley_item_t *ei)
{
    n00b_earley_item_t *start = ei->start_item;

    if (!ei->cursor) {
        if (start->group) {
            if (start->double_dot) {
                ei->subtree_info = N00B_SI_GROUP_START;
            }
            else {
                ei->subtree_info = N00B_SI_GROUP_ITEM_START;
            }
        }
        else {
            ei->subtree_info = N00B_SI_NT_RULE_START;
        }
    }

    if (ei->cursor == ei_rule_len(start)) {
        if (start->group) {
            if (start->double_dot) {
                ei->subtree_info = N00B_SI_GROUP_END;
                ei->double_dot   = true;
            }
            else {
                ei->subtree_info = N00B_SI_GROUP_ITEM_END;
            }
        }
        else {
            ei->subtree_info = N00B_SI_NT_RULE_END;
        }
    }
}

static inline void
set_next_action(n00b_parser_t *p, n00b_earley_item_t *ei)
{
    n00b_earley_item_t *start = ei->start_item;

    if (ei->cursor == ei_rule_len(start)) {
        switch (ei->subtree_info) {
        case N00B_SI_GROUP_END:
        case N00B_SI_NT_RULE_END:
            ei->op = N00B_EO_COMPLETE_N;
            return;
        default:
            ei->op = N00B_EO_ITEM_END;
            return;
        }
    }

    if (!ei->cursor && ei->double_dot) {
        ei->op = N00B_EO_FIRST_GROUP_ITEM;
        return;
    }

    n00b_pitem_t *next = get_ei_pitem(ei->start_item, ei->cursor);

    switch (next->kind) {
    case N00B_P_TERMINAL:
        ei->op = N00B_EO_SCAN_TOKEN;
        return;
    case N00B_P_ANY:
        ei->op = N00B_EO_SCAN_ANY;
        return;
    case N00B_P_BI_CLASS:
        ei->op = N00B_EO_SCAN_CLASS;
        return;
    case N00B_P_SET:
        ei->op = N00B_EO_SCAN_SET;
        return;
    case N00B_P_NT:
        if (n00b_is_nullable_pitem(p->grammar, next, n00b_list(n00b_type_ref()))) {
            ei->null_prediction = true;
        }
        ei->op = N00B_EO_PREDICT_NT;
        return;
    case N00B_P_GROUP:
        ei->op = N00B_EO_PREDICT_G;
        return;
    case N00B_P_NULL:
        ei->op = N00B_EO_SCAN_NULL;
        return;
    default:
        n00b_unreachable();
    }
}

#if 0
        printf("prop_check_ei of property " #prop " (%p vs %p)\n", \
               old->prop,                                          \
               new->prop);
#endif

#define prop_check_ei(prop)                                              \
    if (((void *)(int64_t)old->prop) != ((void *)(int64_t) new->prop)) { \
        return false;                                                    \
    }
#define prop_check_start(prop)  \
    if (os->prop != ns->prop) { \
        return false;           \
    }

static inline bool
are_dupes(n00b_earley_item_t *old, n00b_earley_item_t *new)
{
    n00b_earley_item_t *os = old->start_item;
    n00b_earley_item_t *ns = new->start_item;

    // If we're not the original prediction in a rule, then if the
    // links to the start items are different, we want separate items.

    if (os && ns && os != old) {
        if (ns != os) {
            return false;
        }
    }

    if (os && os != old) {
        prop_check_start(previous_scan);
        prop_check_start(rule);
        prop_check_start(cursor);
        prop_check_start(group);
        prop_check_start(group_top);
        prop_check_start(double_dot);
        prop_check_start(null_prediction);
    }

    prop_check_ei(previous_scan);
    prop_check_ei(cursor);
    prop_check_ei(group);
    prop_check_ei(group_top);
    prop_check_ei(double_dot);
    prop_check_ei(rule);
    prop_check_ei(null_prediction);

    return true;
}

static inline n00b_earley_item_t *
search_for_existing_state(n00b_earley_state_t *s, n00b_earley_item_t *ei, int n)
{
    for (int i = 0; i < n; i++) {
        n00b_earley_item_t *existing = n00b_list_get(s->items, i, NULL);

        if (are_dupes(existing, ei)) {
            return existing;
        }
    }

    return NULL;
}

static inline void
calculate_group_end_penalties(n00b_earley_item_t *ei)
{
    // We don't track penalties for not meeting the minimum when
    // setting items, and we don't propogate the penalty we calculated
    // when predicting the group. So we calculate the correct value
    // here.

    int match_min    = ei->group->min;
    int match_max    = ei->group->max;
    int new_match_ct = ei->match_ct;

    if (new_match_ct < match_min) {
        ei->group_penalty = new_match_ct - match_min;
    }
    else {
        if (match_max && new_match_ct > match_max) {
            ei->group_penalty = (match_max - new_match_ct);
        }
    }

    ei->penalty = ei->group_penalty + ei->my_penalty + ei->sub_penalties;
}

static void
add_item(n00b_parser_t       *p,
         n00b_earley_item_t  *from_state,
         n00b_earley_item_t **newptr,
         bool                 next_state)
{
    n00b_earley_item_t *new = *newptr;

    if (new->group &&new->cursor == n00b_list_len(new->rule->contents)) {
        if (new->group->min < new->match_ct) {
            calculate_group_end_penalties(new);
        }
    }

    if (new->penalty > p->grammar->max_penalty) {
        return;
    }

    n00b_assert(new->rule);
    // The state we use for duping could also be hashed to avoid the
    // state scan if that ever were an issue.
    n00b_earley_state_t *state   = next_state ? p->next_state : p->current_state;
    int                  n       = n00b_list_len(state->items);
    new->estate_id               = state->id;
    new->eitem_index             = n;
    n00b_earley_item_t *existing = search_for_existing_state(state, new, n);

    if (existing) {
        switch (n00b_earley_cost_cmp(new, existing)) {
        case N00B_EARLEY_CMP_LT:
            existing->completors    = new->completors;
            existing->penalty       = new->penalty;
            existing->sub_penalties = new->sub_penalties;
            existing->my_penalty    = new->penalty;
            existing->group_penalty = new->group_penalty;
            existing->previous_scan = new->previous_scan;
            existing->predictions   = new->predictions;
            existing->cost          = new->cost;
            break;
        case N00B_EARLEY_CMP_GT:
            // This will effectively abandon the state; the penalty is too
            // high.
            return;
        default:
            break;
        }

        existing->completors = n00b_set_union(existing->completors,
                                              new->completors);

        *newptr = existing;
        return;
    }
    n00b_list_append(state->items, new);
    set_subtree_info(p, new);
    set_next_action(p, new);
}

static inline n00b_earley_item_t *
new_earley_item(void)
{
    n00b_earley_item_t *result = n00b_gc_alloc_mapped(n00b_earley_item_t,
                                                      N00B_GC_SCAN_ALL);
    result->noscan             = N00B_NOSCAN;
    return result;
}

static void
terminal_scan(n00b_parser_t *p, n00b_earley_item_t *old, bool not_null)
{
    n00b_earley_item_t *new   = new_earley_item();
    n00b_earley_item_t *start = old->start_item;
    new->start_item           = start;
    new->cursor               = old->cursor + 1;
    new->parent_states        = old->parent_states;
    new->penalty              = old->penalty;
    new->sub_penalties        = old->sub_penalties;
    new->my_penalty           = old->my_penalty;
    new->previous_scan        = old;
    new->rule                 = old->rule;
    new->cost                 = old->cost;

    old->no_reprocessing = true;
    add_item(p, NULL, &new, not_null);
}

static void
register_prediction(n00b_earley_item_t *predictor, n00b_earley_item_t *predicted)
{
    if (!predictor) {
        return;
    }
    if (!predictor->predictions) {
        predictor->predictions = n00b_set(n00b_type_ref());
    }
    if (!predicted->parent_states) {
        predicted->parent_states = n00b_set(n00b_type_ref());
    }
    hatrack_set_add(predictor->predictions, predicted);
    hatrack_set_add(predicted->parent_states, predictor);
}

static void
add_one_nt_prediction(n00b_parser_t      *p,
                      n00b_earley_item_t *predictor,
                      n00b_nonterm_t     *nt,
                      int                 rule_ix)
{
    n00b_earley_item_t *ei = new_earley_item();
    ei->ruleset_id         = nt->id;
    ei->rule               = n00b_list_get(nt->rules, rule_ix, NULL);
    ei->rule_index         = rule_ix;
    ei->start_item         = ei;

    if (predictor) {
        n00b_earley_item_t *prestart = predictor->start_item;
        ei->predictor_ruleset_id     = prestart->ruleset_id;
        ei->predictor_rule_index     = prestart->rule_index;
        ei->predictor_cursor         = prestart->cursor;
    }

    if (ei->rule->penalty_rule) {
        ei->my_penalty += 1;
    }

    ei->penalty = ei->my_penalty;

    add_item(p, predictor, &ei, false);
    register_prediction(predictor, ei);
}

static n00b_earley_item_t *
add_one_group_prediction(n00b_parser_t *p, n00b_earley_item_t *predictor)
{
    // There are never penalties when predicting a group.

    n00b_earley_item_t *ps   = predictor->start_item;
    n00b_pitem_t       *next = get_ei_pitem(ps, predictor->cursor);
    n00b_rule_group_t  *g    = next->contents.group;
    n00b_earley_item_t *gei  = new_earley_item();
    gei->start_item          = gei;
    gei->cursor              = 0;
    gei->double_dot          = true;
    gei->group               = g;
    gei->ruleset_id          = g->gid;
    gei->rule                = n00b_list_get(g->contents->rules, 0, NULL);

    n00b_assert(gei->rule);

    gei->predictor_ruleset_id = ps->ruleset_id;
    gei->predictor_rule_index = ps->rule_index;
    gei->predictor_cursor     = ps->cursor;

    add_item(p, predictor, &gei, false);
    register_prediction(predictor, gei);

    return gei;
}

static void
add_first_group_item(n00b_parser_t *p, n00b_earley_item_t *gei)
{
    // We don't deal with penalties for a regex being too short until
    // completion time. So the first item doesn't need any penalty
    // accounting.
    n00b_earley_item_t *ei = new_earley_item();
    ei->start_item         = ei;
    ei->cursor             = 0;
    ei->rule               = n00b_list_get(gei->group->contents->rules, 0, NULL);
    ei->ruleset_id         = gei->group->gid;
    ei->rule_index         = 0;
    ei->match_ct           = 0;
    ei->group              = gei->group;
    ei->group_top          = gei;
    ei->double_dot         = false;

    n00b_assert(gei);

    ei->predictor_ruleset_id = ei->group_top->ruleset_id;
    ei->predictor_rule_index = ei->group_top->rule_index;
    ei->predictor_cursor     = ei->group_top->cursor;

    add_item(p, gei, &ei, false);

    register_prediction(gei, ei);
}

static void
empty_group_completion(n00b_parser_t *p, n00b_earley_item_t *gei)
{
    if (!gei->group->min) {
        n00b_earley_item_t *gend = new_earley_item();
        gend->start_item         = gei;
        gend->cursor             = n00b_list_len(gei->rule->contents);
        gend->double_dot         = true;
        gend->group              = gei->group;
        gend->ruleset_id         = gei->group->gid;
        gend->rule               = gei->rule;
        gend->match_ct           = 0;
        gend->parent_states      = gei->parent_states;

        add_item(p, gei, &gend, false);
    }

    gei->no_reprocessing = true;
}

static void
add_next_group_item(n00b_parser_t      *p,
                    n00b_earley_item_t *last_end)
{
    n00b_earley_item_t *ei         = new_earley_item();
    n00b_earley_item_t *last_start = last_end->start_item;

    last_end->no_reprocessing = true;

    ei->start_item    = ei;
    ei->cursor        = 0;
    ei->previous_scan = last_end;
    ei->rule          = last_start->rule;
    ei->ruleset_id    = last_start->ruleset_id;
    ei->parent_states = n00b_shallow(last_start->parent_states);
    ei->group_top     = last_start->group_top;
    ei->match_ct      = last_end->match_ct;
    ei->group         = last_start->group;
    ei->my_penalty    = last_end->my_penalty;
    ei->sub_penalties = last_end->sub_penalties;
    ei->cost          = last_end->cost;

    int max_items = ei->group_top->group->max;

    n00b_assert(ei->group_top);

    ei->predictor_ruleset_id = ei->group_top->ruleset_id;
    ei->predictor_rule_index = ei->group_top->rule_index;
    ei->predictor_cursor     = ei->group_top->cursor;

    // Again, items only track penalties if / when we exceed the max;
    // min is handled at completion.

    if (max_items && ei->match_ct > max_items) {
        ei->group_penalty = ei->match_ct - max_items;
    }

    // The group penalty will disappear after the lead item, and come
    // back when we complete the group.
    ei->penalty = ei->group_penalty + ei->my_penalty + ei->sub_penalties;

    add_item(p, last_end, &ei, false);

    if (!ei->completors) {
        ei->completors = n00b_set(n00b_type_ref());
    }

    hatrack_set_put(ei->completors, ei->group_top);
    register_prediction(ei->group_top, ei);
}

static void
add_one_completion(n00b_parser_t      *p,
                   n00b_earley_item_t *cur,
                   n00b_earley_item_t *parent_ei)
{
    // The node we're producing will map to the end of a single tree
    // node started by the the parent EI, but also the beginning of
    // the next node. So we advance the scan pointer.

    n00b_earley_item_t *ei           = new_earley_item();
    n00b_earley_item_t *parent_start = parent_ei->start_item;

    ei->start_item    = parent_start;
    ei->cursor        = parent_ei->cursor + 1;
    ei->previous_scan = parent_ei;
    ei->group_top     = parent_ei->group_top;
    // The new items inherits penalty data from two places; any
    // sibling in its rule (which we are calling 'parent' here because
    // it's the parent node we're bumping back up to), and the item
    // that causes the completion (cur).  Fort the completion, we
    // inherit ALL penalties ascribed to that node. The result for all
    // of those inputs goes into sub_penalties.
    // Also, we subtract out the inherited penalty at this point; it
    // *should* be the same as cur-penalty making it a waste, but
    // as I massage things I want the extra assurance.
    ei->my_penalty    = parent_ei->my_penalty;
    ei->sub_penalties = parent_ei->sub_penalties + cur->penalty;
    ei->penalty       = ei->sub_penalties + ei->my_penalty + ei->group_penalty;
    ei->rule          = parent_start->rule;
    ei->cost          = cur->start_item->rule->cost;
    ei->cost += parent_ei->cost + cur->cost;

    if (ei->group_top) {
        ei->match_ct = parent_start->match_ct;
    }

    if (ei->penalty < cur->penalty) {
        ei->penalty = cur->penalty;
    }

    // Don't allow empty group items.
    if (ei->group_top && ei->start_item->estate_id == p->current_state->id) {
        return;
    }

    ei->parent_states = n00b_shallow(parent_ei->parent_states);

    add_item(p, cur, &ei, false);

    if (!ei->completors) {
        ei->completors = n00b_set(n00b_type_ref());
    }
    hatrack_set_put(ei->completors, cur);
    parent_ei->no_reprocessing = true;
}

static n00b_earley_item_t *
add_group_completion(n00b_parser_t      *p,
                     n00b_earley_item_t *cur)
{
    n00b_earley_item_t *ei     = new_earley_item();
    n00b_earley_item_t *istart = cur->start_item;
    n00b_earley_item_t *gstart = istart->group_top;

    n00b_assert(gstart);

    ei->start_item    = gstart;
    ei->rule          = gstart->rule;
    ei->cursor        = n00b_list_len(ei->rule->contents);
    ei->match_ct      = cur->match_ct;
    ei->group         = gstart->group;
    ei->double_dot    = true;
    ei->completors    = n00b_set(n00b_type_ref());
    ei->parent_states = gstart->parent_states;

    calculate_group_end_penalties(ei);
    ei->penalty += cur->my_penalty + cur->sub_penalties;
    ei->cost = cur->cost;

    add_item(p, cur, &ei, false);

    hatrack_set_put(ei->completors, cur);
    cur->no_reprocessing = true;
    return ei;
}

static inline void
predict_nt(n00b_parser_t *p, n00b_nonterm_t *nt, n00b_earley_item_t *ei)
{
    int n = n00b_list_len(nt->rules);

    n00b_assert(n00b_list_len(nt->rules));

    for (int64_t i = 0; i < n; i++) {
        add_one_nt_prediction(p, ei, nt, i);
    }
}

static inline void
predict_nt_via_ei(n00b_parser_t *p, n00b_earley_item_t *ei)
{
    n00b_earley_item_t *s  = ei->start_item;
    n00b_pitem_t       *nx = get_ei_pitem(s, ei->cursor);
    n00b_nonterm_t     *nt = n00b_get_nonterm(p->grammar, nx->contents.nonterm);

    if (ei->null_prediction) {
        terminal_scan(p, ei, true);
    }

    predict_nt(p, nt, ei);
}

static inline void
predict_group(n00b_parser_t *p, n00b_earley_item_t *predictor)
{
    n00b_earley_item_t *gei = add_one_group_prediction(p, predictor);
    add_first_group_item(p, gei);
}

bool
char_in_class(n00b_codepoint_t cp, n00b_bi_class_t class)
{
    if (!n00b_codepoint_valid(cp)) {
        return false;
    }

    switch (class) {
    case N00B_P_BIC_ID_START:
        return n00b_codepoint_is_id_start(cp);
        break;
    case N00B_P_BIC_N00B_ID_START:
        return n00b_codepoint_is_n00b_id_start(cp);
        break;
    case N00B_P_BIC_ID_CONTINUE:
        return n00b_codepoint_is_id_continue(cp);
        break;
    case N00B_P_BIC_N00B_ID_CONTINUE:
        return n00b_codepoint_is_n00b_id_continue(cp);
        break;
    case N00B_P_BIC_DIGIT:
        return n00b_codepoint_is_ascii_digit(cp);
        break;
    case N00B_P_BIC_ANY_DIGIT:
        return n00b_codepoint_is_unicode_digit(cp);
        break;
    case N00B_P_BIC_UPPER:
        return n00b_codepoint_is_unicode_upper(cp);
        break;
    case N00B_P_BIC_UPPER_ASCII:
        return n00b_codepoint_is_ascii_upper(cp);
        break;
    case N00B_P_BIC_LOWER:
        return n00b_codepoint_is_unicode_lower(cp);
        break;
    case N00B_P_BIC_LOWER_ASCII:
        return n00b_codepoint_is_ascii_lower(cp);
        break;
    case N00B_P_BIC_ALPHA_ASCII:
        return n00b_codepoint_is_ascii_alpha(cp);
        break;
    case N00B_P_BIC_SPACE:
        return n00b_codepoint_is_space(cp);
        break;
    case N00B_P_BIC_JSON_string_CONTENTS:
        return n00b_codepoint_is_valid_json_string_char(cp);
    case N00B_P_BIC_HEX_DIGIT:
        return n00b_codepoint_is_hex_digit(cp);
    case N00B_P_BIC_NONZERO_ASCII_DIGIT:
        return n00b_codepoint_is_ascii_digit(cp) && cp != '0';
    }

    n00b_unreachable();
}

static inline bool
matches_set(n00b_parser_t *parser, n00b_codepoint_t cp, n00b_list_t *set_items)
{
    // Return true if there's a match.
    int n = n00b_list_len(set_items);

    for (int i = 0; i < n; i++) {
        n00b_pitem_t *sub = n00b_list_get(set_items, i, NULL);

        switch (sub->kind) {
        case N00B_P_ANY:
            return true;
        case N00B_P_TERMINAL:
            if (cp == (int32_t)sub->contents.terminal) {
                return true;
            }
            continue;
        case N00B_P_BI_CLASS:
            if (char_in_class(cp, sub->contents.class)) {
                return true;
            }
            continue;
        default:
            n00b_unreachable();
        }
    }

    return false;
}

static inline void
scan_class(n00b_parser_t *parser, n00b_earley_item_t *ei)
{
    n00b_codepoint_t cp   = parser->current_state->token->tid;
    n00b_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (char_in_class(cp, next->contents.class)) {
        terminal_scan(parser, ei, true);
    }
}

static inline void
scan_terminal(n00b_parser_t *parser, n00b_earley_item_t *ei)
{
    n00b_codepoint_t cp   = parser->current_state->token->tid;
    n00b_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (cp == next->contents.terminal) {
        terminal_scan(parser, ei, true);
    }
}

static inline void
scan_set(n00b_parser_t *parser, n00b_earley_item_t *ei)
{
    n00b_codepoint_t cp   = parser->current_state->token->tid;
    n00b_pitem_t    *next = get_ei_pitem(ei->start_item, ei->cursor);

    if (matches_set(parser, cp, next->contents.items)) {
        terminal_scan(parser, ei, true);
    }
}

static void
complete_group_item(n00b_parser_t *parser, n00b_earley_item_t *ei)
{
    ei->match_ct++;
    add_next_group_item(parser, ei);
}

static void
complete(n00b_parser_t *parser, n00b_earley_item_t *ei)
{
    uint64_t             n;
    n00b_set_t          *start_set = ei->start_item->parent_states;
    n00b_earley_item_t **parents   = hatrack_set_items_sort(start_set, &n);

    for (uint64_t i = 0; i < n; i++) {
        add_one_completion(parser, ei, parents[i]);
    }
}

static inline void
run_state_predictions(n00b_parser_t *parser)
{
    int                 i  = 0;
    n00b_earley_item_t *ei = n00b_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case N00B_EO_PREDICT_NT:
                predict_nt_via_ei(parser, ei);
                break;
            case N00B_EO_PREDICT_G:
                predict_group(parser, ei);
                ei->no_reprocessing = true;
                break;
            case N00B_EO_FIRST_GROUP_ITEM:
                // EI should be the double dot; Generate the first item
                // under it, and possibly a completion.
                add_first_group_item(parser, ei);
                empty_group_completion(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = n00b_list_get(parser->current_state->items, ++i, NULL);
    }
}

static inline void
run_state_scans(n00b_parser_t *parser)
{
    int                 i  = 0;
    n00b_earley_item_t *ei = n00b_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case N00B_EO_SCAN_TOKEN:
                scan_terminal(parser, ei);
                break;
            case N00B_EO_SCAN_ANY:
                terminal_scan(parser, ei, true);
                break;
            case N00B_EO_SCAN_NULL:
                terminal_scan(parser, ei, false);
                break;
            case N00B_EO_SCAN_CLASS:
                scan_class(parser, ei);
                break;
            case N00B_EO_SCAN_SET:
                scan_set(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = n00b_list_get(parser->current_state->items, ++i, NULL);
    }
}

static inline void
run_state_completions(n00b_parser_t *parser)
{
    int                 i  = 0;
    n00b_earley_item_t *ei = n00b_list_get(parser->current_state->items, i, NULL);

    while (ei != NULL) {
        if (!ei->no_reprocessing) {
            switch (ei->op) {
            case N00B_EO_COMPLETE_N:
                complete(parser, ei);
                break;
            case N00B_EO_ITEM_END:
                complete_group_item(parser, ei);
                add_group_completion(parser, ei);
                break;
            default:
                break;
            }
        }
        ei = n00b_list_get(parser->current_state->items, ++i, NULL);
    }
}

static void
process_current_state(n00b_parser_t *parser)
{
    int prev = n00b_list_len(parser->current_state->items);
    int cur;

    while (true) {
        run_state_completions(parser);
        run_state_predictions(parser);
        run_state_scans(parser);
        cur = n00b_list_len(parser->current_state->items);
        if (cur == prev) {
            break;
        }

        prev = cur;
    }
}

static void
enter_next_state(n00b_parser_t *parser)
{
    // If next_state is NULL, then we skip advancing the state, because
    // we're really going from -1 into 0.

    if (parser->next_state != NULL) {
        parser->position++;
        parser->current_state = parser->next_state;
    }
    else {
        // Load the start ruleset.
        n00b_grammar_t *grammar = parser->grammar;
        int             ix      = parser->start;
        n00b_nonterm_t *start   = n00b_get_nonterm(grammar, ix);

        predict_nt(parser, start, NULL);
    }

    n00b_assert(parser->position == n00b_list_len(parser->states) - 1);
    parser->next_state = n00b_new_earley_state(n00b_list_len(parser->states));
    n00b_list_append(parser->states, parser->next_state);

    n00b_parser_load_token(parser);
}

static void
run_parsing_mainloop(n00b_parser_t *parser)
{
    // Shouldn't need this, but the way I'm currently disambiguating
    // is forcing it now. So I mark when items never need
    // reprocessing... could do more by also marking the minimum state
    // to process a completion for.
    do {
        enter_next_state(parser);
        process_current_state(parser);
    } while (parser->current_state->token->tid != N00B_TOK_EOF);
}

static void
internal_parse(n00b_parser_t *parser, n00b_nonterm_t *start)
{
    if (!parser->grammar) {
        N00B_CRAISE("Call to parse before grammar is set.");
    }

    n00b_prep_first_parse(parser->grammar);

    if (parser->run) {
        N00B_CRAISE("Attempt to re-run parser w/o resetting state.");
    }

    if (!parser->tokenizer && !parser->token_cache) {
        N00B_CRAISE("Call to parse without associating tokens.");
    }

    if (start != NULL) {
        parser->start = start->id;
    }
    else {
        parser->start = parser->grammar->default_start;
    }

    run_parsing_mainloop(parser);

    if (parser->show_debug) {
        n00b_print(n00b_repr_state_table(parser, true));
    }
}

void
n00b_parse_token_list(n00b_parser_t  *parser,
                      n00b_list_t    *toks,
                      n00b_nonterm_t *start)
{
    n00b_parser_reset(parser);
    parser->preloaded_tokens = true;
    parser->token_cache      = toks;
    parser->tokenizer        = NULL;
    internal_parse(parser, start);
}

void
n00b_parse_string(n00b_parser_t  *parser,
                  n00b_string_t  *s,
                  n00b_nonterm_t *start)
{
    n00b_parser_reset(parser);
    parser->user_context = s;
    parser->tokenizer    = n00b_token_stream_codepoints;
    internal_parse(parser, start);
}

void
n00b_parse_string_list(n00b_parser_t  *parser,
                       n00b_list_t    *items,
                       n00b_nonterm_t *start)
{
    n00b_parser_reset(parser);
    parser->user_context = items;
    parser->tokenizer    = n00b_token_stream_strings;
    internal_parse(parser, start);
}
