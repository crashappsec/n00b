#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct nb_info_t {
    // Could union a lot of this, but gets confusing.
    n00b_parse_node_t  *pnode;
    n00b_list_t        *opts;  // A set of all possible subtrees for this node.
    n00b_list_t        *slots; // Pre-muxed data.
    int                 num_kids;
    // Realisticially this is more diagnostic.
    n00b_earley_item_t *bottom_item;
    n00b_earley_item_t *top_item;
    // This tracks when the subtree has been built but not post-processed.
    // Post-processing ignores it.
    bool                cached;
    bool                visiting;
} nb_info_t;

static nb_info_t *
populate_subtree(n00b_parser_t *p, n00b_earley_item_t *end);

#ifdef N00B_EARLEY_DEBUG
static inline void
ptree(void *v, char *txt)
{
    if (n00b_type_is_list(n00b_get_my_type(v))) {
        return;
    }

    n00b_tree_node_t *t = (n00b_tree_node_t *)v;
    n00b_printf("[h2]{}", n00b_new_utf8(txt));

    n00b_print(n00b_grid_tree_new(t,
                                  n00b_kw("callback",
                                          n00b_ka(n00b_repr_parse_node))));
}

static inline n00b_utf8_t *
dei_base(n00b_parser_t *p, n00b_earley_item_t *ei)
{
    assert(ei);
    n00b_list_t *repr = n00b_repr_earley_item(p, ei, 0);

    return n00b_cstr_format(" [em]{}:{}[/] {} ({})  [reverse]{}[/]",
                            ei->estate_id,
                            ei->eitem_index,
                            n00b_list_get(repr, 1, NULL),
                            n00b_list_get(repr, 4, NULL),
                            n00b_list_get(repr, 5, NULL));
}

static void
_dei(n00b_parser_t *p, n00b_earley_item_t *start, n00b_earley_item_t *end, char *s)
{
    n00b_utf8_t *custom = s ? n00b_rich_lit(s) : n00b_rich_lit("[h2]dei:[/] ");
    n00b_utf8_t *s1     = dei_base(p, start);
    n00b_utf8_t *s2     = NULL;
    n00b_utf8_t *to_print;

    if (end) {
        s2 = dei_base(p, end);

        to_print = n00b_str_concat(custom, n00b_rich_lit(" [h6]Top: "));
        to_print = n00b_str_concat(to_print, s1);
        to_print = n00b_str_concat(to_print, n00b_rich_lit(" [h6]End: "));
        to_print = n00b_str_concat(to_print, s2);
    }
    else {
        to_print = n00b_str_concat(custom, s1);
    }

    n00b_print(to_print);
}

#define dei(x, y, z) _dei(p, x, y, z)
#define dni(x, y)    _dei(p, x->top_item, x->bottom_item, y)
#else
#define dei(x, y, z)
#define dni(x, y)
#define ptree(x, y)
#endif

uint64_t
n00b_parse_node_hash(n00b_tree_node_t *t)
{
    n00b_parse_node_t *pn = n00b_tree_get_contents(t);

    if (pn->hv) {
        return pn->hv;
    }

    n00b_sha_t *sha    = n00b_new(n00b_type_hash());
    uint64_t    bounds = (uint64_t)pn->start;
    uint64_t    other  = ((uint64_t)pn->penalty) << 32 | (uint64_t)pn->cost;

    bounds <<= 32;
    bounds |= (uint64_t)pn->end;

    if (pn->token) {
        other |= (1ULL << 33);
    }
    else {
        if (pn->group_item) {
            other |= (1ULL << 34);
        }
        else {
            if (pn->group_top) {
                other |= (1ULL << 35);
            }
        }
    }

    n00b_sha_int_update(sha, pn->id);
    n00b_sha_int_update(sha, bounds);
    n00b_sha_int_update(sha, other);

    if (pn->token && pn->info.token) {
        uint64_t tinfo = (uint64_t)pn->info.token->tid;
        tinfo <<= 32;
        tinfo |= (uint64_t)pn->info.token->index;
        n00b_sha_int_update(sha, tinfo);

        n00b_utf8_t *s = pn->info.token->value;
        if (s) {
            n00b_sha_string_update(sha, s);
        }
        else {
            n00b_sha_int_update(sha, 0ULL);
        }
    }
    else {
        n00b_sha_int_update(sha, t->num_kids);
        n00b_sha_string_update(sha, pn->info.name);

        for (int i = 0; i < t->num_kids; i++) {
            n00b_sha_int_update(sha, n00b_parse_node_hash(t->children[i]));
        }
    }

    n00b_buf_t *buf = n00b_sha_finish(sha);
    pn->hv          = ((uint64_t *)buf->data)[0];

    return pn->hv;
}

static void
add_penalty_info(n00b_grammar_t    *g,
                 n00b_parse_node_t *pn,
                 n00b_parse_rule_t *bad_rule)
{
    n00b_pitem_t *pi = n00b_list_get(bad_rule->contents,
                                     pn->penalty_location,
                                     NULL);

    if (pi->kind == N00B_P_NULL) {
        pn->missing = true;
    }
    else {
        pn->bad_prefix = true;
    }

    if (bad_rule->penalty_rule) {
        n00b_list_t *contents = NULL;

        if (bad_rule->link) {
            contents = bad_rule->link->contents;
        }

        if (!contents) {
            contents = n00b_list(n00b_type_ref());
            n00b_list_append(contents, n00b_new_pitem(N00B_P_NULL));
        }
        pn->info.name = n00b_repr_rule(g, contents, -1);
    }
}

static void
add_penalty_annotation(n00b_parse_node_t *pn)
{
    n00b_utf8_t *s = NULL;

    if (pn->missing) {
        s = n00b_cstr_format(" [em i](Missing token before position {})",
                             (uint64_t)pn->penalty_location);
    }

    if (pn->bad_prefix) {
        s = n00b_cstr_format(" [em i](Unexpected token at position {})",
                             (uint64_t)pn->penalty_location);
    }

    if (!s) {
        s = n00b_rich_lit(" [em i](Custom Penalty)");
    }

    pn->info.name = n00b_str_concat(pn->info.name, s);
}

n00b_list_t *
n00b_clean_trees(n00b_parser_t *p, n00b_list_t *l)
{
    n00b_set_t  *hashes = n00b_set(n00b_type_int());
    n00b_list_t *result = n00b_list(n00b_type_ref());

    int n = n00b_list_len(l);

    if (n < 1) {
        return l;
    }

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t *t  = n00b_list_get(l, i, NULL);
        uint64_t          hv = n00b_parse_node_hash(t);

        if (hatrack_set_contains(hashes, (void *)hv)) {
            continue;
        }
        hatrack_set_put(hashes, (void *)hv);
        n00b_list_append(result, t);
    }

    return result;
}

static void
add_option(nb_info_t *parent, int i, nb_info_t *kid)
{
    n00b_list_t *options = n00b_list_get(parent->slots, i, NULL);
    if (!options) {
        options = n00b_list(n00b_type_ref());
        n00b_list_set(parent->slots, i, options);
    }

    n00b_list_append(options, kid);
}

static nb_info_t *
get_node(n00b_parser_t *p, n00b_earley_item_t *b)
{
    n00b_earley_item_t *top = b->start_item;

    // When we pass non-terminals to get their node, the top (t)
    // should be the first prediction involving the subtree, and the
    // bottom (b) should be the state that completes the non-terminal.
    n00b_dict_t *cache = top->cache;

    if (!cache) {
        // We're going to map a cache of any token to null, so use int
        // not ref so we don't try to cache the hash value and crash.
        cache      = n00b_dict(n00b_type_int(), n00b_type_ref());
        top->cache = cache;
    }

    nb_info_t *result = hatrack_dict_get(cache, b, NULL);

    if (result) {
        return result;
    }

    n00b_parse_node_t *pn = n00b_gc_alloc_mapped(n00b_parse_node_t,
                                                 N00B_GC_SCAN_ALL);

    result              = n00b_gc_alloc_mapped(nb_info_t, N00B_GC_SCAN_ALL);
    result->pnode       = pn;
    result->bottom_item = b;
    result->top_item    = top;
    result->slots       = n00b_list(n00b_type_ref());

    pn->id         = top->ruleset_id;
    pn->rule_index = top->rule_index;
    pn->start      = top->estate_id;
    pn->end        = b->estate_id;
    // Will add in kid penalties too, later.
    pn->penalty    = b->penalty;
    pn->cost       = b->cost;

    hatrack_dict_put(cache, b, result);

    n00b_parse_rule_t *rule = top->rule;

    // For group nodes in the tree, the number of kids is determined
    // by the match count. Everything else that might get down here is
    // a non-terminal, and the number of kids is determined by the
    // rule length
    n00b_utf8_t *s1;

    if (b->subtree_info == N00B_SI_GROUP_END) {
        s1            = n00b_repr_nonterm(p->grammar,
                               N00B_GID_SHOW_GROUP_LHS,
                               false);
        pn->group_top = true;
    }
    else {
        result->num_kids = n00b_list_len(rule->contents);
        if (top->group_top) {
            s1             = n00b_repr_group(p->grammar, top->group_top->group);
            pn->group_item = true;
        }
        else {
            s1 = n00b_repr_nonterm(p->grammar, top->ruleset_id, false);
        }
    }

    for (int i = 0; i < result->num_kids; i++) {
        n00b_list_append(result->slots, NULL);
    }

    n00b_utf8_t *s2;

    if (top->group) {
        s2 = n00b_repr_group(p->grammar, top->group);
    }
    else {
        s2 = n00b_repr_rule(p->grammar, rule->contents, -1);
    }

    pn->info.name = n00b_cstr_format("{}  [yellow]⟶  [/]{}", s1, s2);

    if (rule->penalty_rule) {
        add_penalty_info(p->grammar, pn, rule);
        if (p->tree_annotations) {
            add_penalty_annotation(pn); // Expect to make this an option.
        }
    }

    return result;
}

static n00b_list_t *postprocess_subtree(n00b_parser_t *p, nb_info_t *);

static inline n00b_list_t *
process_slot(n00b_parser_t *p, n00b_list_t *ni_options)
{
    n00b_list_t *result = n00b_list(n00b_type_ref());
    int          n      = n00b_list_len(ni_options);

    for (int i = 0; i < n; i++) {
        nb_info_t *sub = n00b_list_get(ni_options, i, NULL);

        n00b_list_plus_eq(result, postprocess_subtree(p, sub));
    }

    return result;
}

static inline n00b_list_t *
score_filter(n00b_list_t *opts)
{
    uint32_t     penalty = ~0;
    uint32_t     cost    = ~0;
    int          n       = n00b_list_len(opts);
    n00b_list_t *results = n00b_list(n00b_type_ref());

    // First, calculate the lowest penalty, cost tuple that we see.

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t  *t = n00b_list_get(opts, i, NULL);
        n00b_parse_node_t *p = n00b_tree_get_contents(t);
        if (p->penalty < penalty) {
            penalty = p->penalty;
            cost    = p->cost;
        }
        else {
            if (p->penalty == penalty && p->cost < cost) {
                cost = p->cost;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t  *t = n00b_list_get(opts, i, NULL);
        n00b_parse_node_t *p = n00b_tree_get_contents(t);

        if (p->penalty == penalty && p->cost == cost) {
            n00b_list_append(results, t);
        }
    }

    return results;
}

static inline n00b_parse_node_t *
copy_parse_node(n00b_parse_node_t *in)
{
    n00b_parse_node_t *out = n00b_gc_alloc_mapped(n00b_parse_node_t,
                                                  N00B_GC_SCAN_ALL);

    memcpy(out, in, sizeof(n00b_parse_node_t));

    return out;
}

static inline n00b_list_t *
package_single_slot_options(nb_info_t *ni, n00b_list_t *t_opts)
{
    n00b_list_t       *output_opts = n00b_list(n00b_type_ref());
    int                n           = n00b_list_len(t_opts);
    n00b_parse_node_t *pn;

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t *kid_t = n00b_list_get(t_opts, i, NULL);

        pn                        = copy_parse_node(ni->pnode);
        n00b_parse_node_t *kid_pn = n00b_tree_get_contents(kid_t);
        n00b_tree_node_t  *t      = n00b_new_tree_node(n00b_type_ref(), pn);

        if (kid_pn->penalty > pn->penalty) {
            pn->penalty = kid_pn->penalty;
        }

        n00b_tree_adopt_node(t, kid_t);
        n00b_list_append(output_opts, t);
    }

    ni->opts = output_opts;

    return score_filter(n00b_clean_trees(NULL, output_opts));
}

static inline n00b_list_t *
parse_node_zipper(n00b_list_t *kid_sets, n00b_list_t *options)
{
    // If there's only one option in the slot, it should be valid;
    // we can just append it and move on.
    int n              = n00b_list_len(options);
    int num_start_sets = n00b_list_len(kid_sets);

    if (n == 1) {
        n00b_tree_node_t *t = n00b_list_get(options, 0, NULL);

        for (int i = 0; i < num_start_sets; i++) {
            n00b_list_t *kid_set = n00b_list_get(kid_sets, i, NULL);
            n00b_list_append(kid_set, t);
        }

        return kid_sets;
    }

    n00b_list_t *results = n00b_list(n00b_type_ref());

    for (int i = 0; i < n; i++) {
        n00b_tree_node_t  *option_t = n00b_list_get(options, i, NULL);
        n00b_parse_node_t *opt_pn   = n00b_tree_get_contents(option_t);

        for (int j = 0; j < num_start_sets; j++) {
            n00b_list_t      *s1     = n00b_list_get(kid_sets, j, NULL);
            int               slen   = n00b_list_len(s1);
            n00b_tree_node_t *prev_t = n00b_list_get(s1, slen - 1, NULL);
            if (!prev_t) {
                continue;
            }
            n00b_parse_node_t *prev_pn = n00b_tree_get_contents(prev_t);

            if (prev_pn->end != opt_pn->start) {
                continue;
            }

            n00b_list_t *new_set = n00b_shallow(s1);
            assert(option_t);
            n00b_list_append(new_set, option_t);
            n00b_list_append(results, new_set);
        }
    }

    return results;
}

static inline n00b_parse_node_t *
new_epsilon_node(void)
{
    n00b_parse_node_t *pn = n00b_gc_alloc_mapped(n00b_parse_node_t,
                                                 N00B_GC_SCAN_ALL);
    pn->info.name         = n00b_new_utf8("ε");
    pn->id                = N00B_EMPTY_STRING;

    return pn;
}

static inline void
package_kid_sets(nb_info_t *ni, n00b_list_t *kid_sets)
{
    int n_sets = n00b_list_len(kid_sets);
    ni->opts   = n00b_list(n00b_type_ref());

    for (int i = 0; i < n_sets; i++) {
        n00b_list_t       *a_set       = n00b_list_get(kid_sets, i, NULL);
        n00b_parse_node_t *copy        = copy_parse_node(ni->pnode);
        n00b_tree_node_t  *t           = n00b_new_tree_node(n00b_type_ref(), copy);
        n00b_parse_node_t *kpn         = NULL;
        uint32_t           old_penalty = copy->penalty;

        copy->penalty = 0;

        for (int j = 0; j < ni->num_kids; j++) {
            n00b_tree_node_t *kid_t = n00b_list_get(a_set, j, NULL);

            if (!kid_t) {
                continue;
            }

            kpn = n00b_tree_get_contents(kid_t);

            if (!kpn) {
                kpn = new_epsilon_node();
                n00b_tree_replace_contents(kid_t, kpn);
            }
            else {
                copy->penalty += kpn->penalty;
            }

            n00b_tree_adopt_node(t, kid_t);
        }

        n00b_list_append(ni->opts, t);

        if (copy->penalty < old_penalty) {
            copy->penalty = old_penalty;
        }
    }

    ni->opts = score_filter(n00b_clean_trees(NULL, ni->opts));
}

static n00b_list_t *
postprocess_subtree(n00b_parser_t *p, nb_info_t *ni)
{
    n00b_list_t      *slot_options;
    n00b_list_t      *klist;
    n00b_tree_node_t *t;

    if (ni->opts) {
        return ni->opts;
    }

    if (ni->visiting) {
        return n00b_list(n00b_type_ref());
    }

    ni->visiting = true;

    // Leaf nodes just need to be wrapped.
    if (!ni->num_kids) {
        ni->opts = n00b_list(n00b_type_ref());
        t        = n00b_new_tree_node(n00b_type_ref(), ni->pnode);

        n00b_list_append(ni->opts, t);

        return ni->opts;
    }

    // Go ahead and fully process the FIRST slot.

    slot_options = process_slot(p, n00b_list_get(ni->slots, 0, NULL));

    // If it's the ONLY slot, score and wrap. No need to zipper.
    if (ni->num_kids == 1) {
        n00b_list_t *result = package_single_slot_options(ni, slot_options);
        ni->visiting        = false;
        return result;
    }

    // Otherwise, we're going to incrementally 'zipper' options from
    // left to right. We start by turning our one column with n options
    // for the first node into a list of n possible incremental sets
    // of kids.

    n00b_list_t *kid_sets = n00b_list(n00b_type_ref());
    int          n        = n00b_list_len(slot_options);

    for (int i = 0; i < n; i++) {
        t     = n00b_list_get(slot_options, i, NULL);
        klist = n00b_list(n00b_type_ref());

        n00b_list_append(klist, t);
        n00b_list_append(kid_sets, klist);
    }

    // Basically, for each slot / column, we postprocess down, and
    // create a 'new' column that will be populated with tnodes, not
    // node_info structures. Once we create that column, we zipper it
    // into the kid_sets if possible.
    for (int i = 1; i < ni->num_kids; i++) {
        slot_options = process_slot(p, n00b_list_get(ni->slots, i, NULL));
        kid_sets     = parse_node_zipper(kid_sets, slot_options);
    }

    package_kid_sets(ni, kid_sets); // sets ni-opts.

    ni->visiting = false;
    return ni->opts;
}

// This is a primitive to extract the raw Earley items we care about
// from the final state, which are ones that are completions of a
// proper non-terminal that matches our start id. Grammars always
// start w/ non-terminals; they can never start w/ a group or token.
//
// Here we do not filter them out by start state (yet).
static inline n00b_list_t *
search_for_end_items(n00b_parser_t *p)
{
    n00b_earley_state_t *state   = p->current_state;
    n00b_list_t         *items   = state->items;
    n00b_list_t         *results = n00b_list(n00b_type_ref());
    int                  n       = n00b_list_len(items);

    if (!n00b_list_len(items)) {
        return results;
    }

    while (--n) {
        n00b_earley_item_t *ei  = n00b_list_get(items, n, NULL);
        n00b_earley_item_t *top = ei->start_item;

        if (top->subtree_info != N00B_SI_NT_RULE_START) {
            continue;
        }

        if (top->ruleset_id != p->start) {
            continue;
        }

        if (ei->cursor != n00b_len(top->rule->contents)) {
            continue;
        }

        n00b_list_append(results, ei);
    }

    return results;
}

static void
scan_group_items(n00b_parser_t *p, nb_info_t *group_ni, n00b_earley_item_t *end)
{
    // We could have multiple group items ending the group with the same
    // number of matches, if we have specific enough ambiguity.
    //
    // So follow each path back seprately.

    uint64_t             n;
    n00b_earley_item_t **clist  = hatrack_set_items_sort(end->completors, &n);
    uint32_t             minp   = ~0;
    uint32_t             nitems = ~0;

    for (uint64_t i = 0; i < n; i++) {
        n00b_earley_item_t *cur = clist[i];
        if (cur->penalty < minp) {
            minp   = cur->penalty;
            nitems = cur->match_ct;
        }
        if (cur->penalty == minp && (uint32_t)cur->match_ct < nitems) {
            nitems = cur->match_ct;
        }
    }

    for (uint64_t i = 0; i < n; i++) {
        n00b_earley_item_t *cur = clist[i];

        if (cur->penalty != minp || (uint32_t)cur->match_ct != nitems) {
            continue;
        }

        int ix             = cur->match_ct;
        group_ni->num_kids = cur->match_ct;

        while (cur && ix-- > 0) {
            nb_info_t          *possible_node = populate_subtree(p, cur);
            n00b_earley_item_t *start         = cur->start_item;
            add_option(group_ni, ix, possible_node);
            cur = start->previous_scan;
        }

        break;
    }
}

static void
add_token_node(n00b_parser_t *p, nb_info_t *node, n00b_earley_item_t *ei)
{
    // For the moment, we wrap the parse node in a dummy nb_info_t.
    nb_info_t           *ni = n00b_gc_alloc_mapped(nb_info_t, N00B_GC_SCAN_ALL);
    n00b_parse_node_t   *pn = n00b_gc_alloc_mapped(n00b_parse_node_t,
                                                 N00B_GC_SCAN_ALL);
    n00b_earley_state_t *s  = n00b_list_get(p->states, ei->estate_id, NULL);

    pn->start      = s->id;
    pn->end        = s->id + 1;
    pn->info.token = s->token;
    if (s->token) {
        pn->id = s->token->tid;
    }
    pn->token       = true;
    ni->top_item    = ei;
    ni->bottom_item = ei;
    ni->pnode       = pn;

    add_option(node, ei->cursor, ni);
}

static void
add_epsilon_node(nb_info_t *node, n00b_earley_item_t *ei)
{
    // For the moment, we wrap the parse node in a dummy nb_info_t.
    nb_info_t         *ni = n00b_gc_alloc_mapped(nb_info_t, N00B_GC_SCAN_ALL);
    n00b_parse_node_t *pn = new_epsilon_node();

    pn->start       = ei->estate_id;
    pn->end         = ei->estate_id;
    pn->info.name   = n00b_new_utf8("ε");
    pn->id          = N00B_EMPTY_STRING;
    ni->top_item    = ei;
    ni->bottom_item = ei;
    ni->pnode       = pn;

    add_option(node, ei->cursor, ni);
}

static void
scan_rule_items(n00b_parser_t *p, nb_info_t *parent_ni, n00b_earley_item_t *end)
{
    n00b_earley_item_t *start = end->start_item;
    n00b_earley_item_t *cur   = end;
    n00b_earley_item_t *prev  = end;

    for (int i = 0; i < n00b_list_len(start->rule->contents); i++) {
        cur        = cur->previous_scan;
        int cursor = cur->cursor;

        assert(prev->cursor == cur->cursor + 1);

        n00b_pitem_t        *pi = n00b_list_get(start->rule->contents,
                                         cur->cursor,
                                         NULL);
        uint64_t             n_bottoms;
        n00b_earley_item_t **bottoms;

        switch (pi->kind) {
        case N00B_P_NULL:
            add_epsilon_node(parent_ni, cur);
            prev = cur;
            break;
        case N00B_P_TERMINAL:
        case N00B_P_ANY:
        case N00B_P_BI_CLASS:
        case N00B_P_SET:
            add_token_node(p, parent_ni, cur);
            prev = cur;
            break;
        case N00B_P_GROUP:
            // fallthrough
        default:
            // We piece together subtrees by looking at the each
            // subtree that resulted in the right-hand EI being
            // generated; if the start state for that subtree was
            // predicted by the LEFT-HAND item, then we are golden
            // (and I believe it always should be; I am checking to be
            // safe).
            bottoms = hatrack_set_items(prev->completors, &n_bottoms);

            for (uint64_t i = 0; i < n_bottoms; i++) {
                n00b_earley_item_t *subtree_end = bottoms[i];

                // The tops of the subtrees should have been predicted
                // by the LEFT ei.
                nb_info_t *subnode = populate_subtree(p, subtree_end);

                // Add the option to *any* possible parent, not just the one
                // that called us.
                add_option(parent_ni, cursor, subnode);
            }
        }
        prev = cur;
    }
}

static nb_info_t *
populate_subtree(n00b_parser_t *p, n00b_earley_item_t *end)
{
    nb_info_t *ni = get_node(p, end);

    if (ni->cached) {
        return ni;
    }

    if (ni->visiting) {
        return ni;
    }
    ni->visiting = true;

    // We handle populating subnodes in a group differently from
    // populating group items and other non-terminals.
    if (end->subtree_info == N00B_SI_GROUP_END) {
        scan_group_items(p, ni, end);
    }
    else {
        scan_rule_items(p, ni, end);
    }

    ni->cached   = true;
    ni->visiting = false;

    return ni;
}

n00b_list_t *
n00b_build_forest(n00b_parser_t *p)
{
    n00b_list_t *results = NULL;
    n00b_list_t *roots   = n00b_list(n00b_type_ref());
    n00b_list_t *ends    = search_for_end_items(p);
    int          n       = n00b_list_len(ends);

    p->grammar->suspend_penalty_hiding++;

    for (int i = 0; i < n; i++) {
        n00b_earley_item_t *end = n00b_list_get(ends, i, NULL);

        if (end->start_item->estate_id != 0) {
            continue;
        }

        nb_info_t *res = populate_subtree(p, end);

        if (!res) {
            continue;
        }
        n00b_list_append(roots, res);
    }

    n = n00b_list_len(roots);

    // We've built the content of the subtrees, but with the exception
    // of tokens, we've not created proper tree nodes. Plus, we can
    // easily have have ambiguous subtrees, too.

    for (int i = 0; i < n; i++) {
        nb_info_t   *ni        = n00b_list_get(roots, i, NULL);
        n00b_list_t *possibles = postprocess_subtree(p, ni);

        results = n00b_list_plus(results, possibles);
    }

    p->grammar->suspend_penalty_hiding--;
    return results;
}

n00b_list_t *
n00b_parse_get_parses(n00b_parser_t *p)
{
    // This one provides ambiguous trees, but stops at the maximum penalty.
    n00b_list_t *results = score_filter(n00b_build_forest(p));

    results = n00b_clean_trees(p, results);

    return results;
}

void *
n00b_parse_tree_walk(n00b_parser_t *p, n00b_tree_node_t *node, void *thunk)
{
    n00b_list_t *sub_results;
    void        *si;

    n00b_parse_node_t *pn = n00b_tree_get_contents(node);

    if (pn->token) {
        return pn->info.token;
    }
    if (pn->id == N00B_EMPTY_STRING) {
        return NULL;
    }

    if (node->num_kids) {
        sub_results = n00b_list(n00b_type_ref());
        for (int i = 0; i < node->num_kids; i++) {
            si = n00b_parse_tree_walk(p, node->children[i], thunk);
            n00b_list_append(sub_results, si);
        }
    }
    else {
        sub_results = NULL;
    }

    n00b_nonterm_t *nt = n00b_list_get(p->grammar->nt_list, pn->id, NULL);
    if (!nt) {
        N00B_CRAISE("Tree does not seem to match the parser.");
    }

    if (!nt->action) {
        return sub_results;
    }

    return (*nt->action)(pn, sub_results, thunk);
}
