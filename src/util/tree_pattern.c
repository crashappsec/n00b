#include "n00b.h"

typedef struct {
    n00b_set_t       *captures;
    n00b_tree_node_t *tree_cur;
    n00b_tpat_node_t *pattern_cur;
    n00b_cmp_fn       cmp;
} search_ctx_t;

#ifdef N00B_N00B_DEBUG_PATTERNS
#define tpat_debug(ctx, txt) n00b_print(n00b_cstr_format( \
    "{:x} : [em]{}[/]",                                 \
    n00b_box_u64((uint64_t)ctx->pattern_cur),            \
    n00b_new_utf8(txt),                                  \
    0))
#else
#define tpat_debug(ctx, txt)
#endif

#define tpat_varargs(result)                                                 \
    va_list args;                                                            \
    int64_t num_kids = 0;                                                    \
    int     i        = 0;                                                    \
                                                                             \
    va_start(args, capture);                                                 \
    num_kids = va_arg(args, int64_t);                                        \
    if (num_kids) {                                                          \
        result->children = n00b_gc_array_alloc(n00b_tpat_node_t **, num_kids); \
                                                                             \
        for (i = 0; i < num_kids; i++) {                                     \
            n00b_tpat_node_t *n = va_arg(args, n00b_tpat_node_t *);            \
            if (!n) {                                                        \
                break;                                                       \
            }                                                                \
            result->children[i] = n;                                         \
        }                                                                    \
    }                                                                        \
    result->num_kids = i;                                                    \
    va_end(args)

n00b_tree_node_t *
n00b_pat_repr(n00b_tpat_node_t   *pat,
             n00b_pattern_fmt_fn content_formatter)
{
    n00b_utf8_t *op       = NULL;
    n00b_utf8_t *contents = (*content_formatter)(pat->contents);
    n00b_utf8_t *capture;

    if (pat->walk) {
        op = n00b_new_utf8(">>>");
    }
    else {
        if (pat->min == 1 && pat->max == 1) {
            if (pat->ignore_kids) {
                op = n00b_new_utf8(".");
            }
            else {
                op = n00b_new_utf8("...");
            }
        }
        if (pat->min == 0 && pat->max == 1) {
            if (pat->ignore_kids) {
                op = n00b_new_utf8("?");
            }
            else {
                op = n00b_new_utf8("???");
            }
        }
        if (pat->min == 0 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = n00b_new_utf8("*");
            }
            else {
                op = n00b_new_utf8("***");
            }
        }
        if (pat->min == 0 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = n00b_new_utf8("*");
            }
            else {
                op = n00b_new_utf8("***");
            }
        }
        if (pat->min == 1 && pat->max == 0x7fff) {
            if (pat->ignore_kids) {
                op = n00b_new_utf8("+");
            }
            else {
                op = n00b_new_utf8("+++");
            }
        }
    }
    if (op == NULL) {
        if (pat->ignore_kids) {
            op = n00b_cstr_format("{}:{}", pat->min, pat->max);
        }
        else {
            op = n00b_cstr_format("{}:::{}", pat->min, pat->max);
        }
    }

    if (pat->capture) {
        capture = n00b_new_utf8(" !");
    }
    else {
        capture = n00b_new_utf8(" ");
    }

    n00b_utf8_t *txt = n00b_cstr_format("{:x} [em]{}{}[/] :",
                                      n00b_box_u64((uint64_t)pat),
                                      op,
                                      capture);

    txt = n00b_str_concat(txt, contents);

    n00b_tree_node_t *result = n00b_new(n00b_type_tree(n00b_type_utf8()),
                                      n00b_kw("contents", n00b_ka(txt)));

    for (unsigned int i = 0; i < pat->num_kids; i++) {
        n00b_tree_node_t *kid = n00b_pat_repr(pat->children[i],
                                            content_formatter);
        n00b_tree_adopt_node(result, kid);
    }

    return result;
}

static inline n00b_set_t *
merge_captures(n00b_set_t *s1, n00b_set_t *s2)
{
    n00b_set_t *result;

    if (s1 == NULL && s2 == NULL) {
        return NULL;
    }
    if (s1 == NULL) {
        result = s2;
    }
    else {
        if (s2 == NULL) {
            result = s1;
        }
        else {
            result = n00b_set_union(s1, s2);
        }
    }

    return result;
}

void
n00b_tpat_gc_bits(uint64_t *bitmap, n00b_tpat_node_t *n)
{
    n00b_mark_raw_to_addr(bitmap, n, &n->contents);
}

static inline n00b_tpat_node_t *
tpat_base(void *contents, int64_t min, int64_t max, bool walk, int64_t capture)
{
    n00b_tpat_node_t *result = n00b_gc_alloc_mapped(n00b_tpat_node_t,
                                                  n00b_tpat_gc_bits);

    result->min      = min;
    result->max      = max;
    result->contents = contents;

    if (walk) {
        result->walk = 1;
    }
    if (capture) {
        result->capture = 1;
    }

    return result;
}

n00b_tpat_node_t *
_n00b_tpat_find(void *contents, int64_t capture, ...)
{
    n00b_tpat_node_t *result = tpat_base(contents, 1, 1, true, capture);
    tpat_varargs(result);
    return result;
}

n00b_tpat_node_t *
_n00b_tpat_match(void *contents, int64_t capture, ...)
{
    n00b_tpat_node_t *result = tpat_base(contents, 1, 1, false, capture);
    tpat_varargs(result);

    return result;
}

n00b_tpat_node_t *
_n00b_tpat_opt_match(void *contents, int64_t capture, ...)
{
    n00b_tpat_node_t *result = tpat_base(contents, 0, 1, false, capture);
    tpat_varargs(result);

    return result;
}

n00b_tpat_node_t *
_n00b_tpat_n_m_match(void *contents, int64_t min, int64_t max, int64_t capture, ...)
{
    n00b_tpat_node_t *result = tpat_base(contents, min, max, false, capture);
    tpat_varargs(result);

    return result;
}

n00b_tpat_node_t *
n00b_tpat_content_find(void *contents, int64_t capture)
{
    n00b_tpat_node_t *result = tpat_base(contents, 1, 1, true, capture);
    result->ignore_kids     = 1;

    return result;
}

n00b_tpat_node_t *
n00b_tpat_content_match(void *contents, int64_t capture)
{
    n00b_tpat_node_t *result = tpat_base(contents, 1, 1, false, capture);
    result->ignore_kids     = 1;

    return result;
}

n00b_tpat_node_t *
n00b_tpat_opt_content_match(void *contents, int64_t capture)
{
    n00b_tpat_node_t *result = tpat_base(contents, 0, 1, false, capture);
    result->ignore_kids     = 1;

    return result;
}

n00b_tpat_node_t *
n00b_tpat_n_m_content_match(void *contents, int64_t min, int64_t max, int64_t capture)
{
    n00b_tpat_node_t *result = tpat_base(contents, min, max, false, capture);
    result->ignore_kids     = 1;

    return result;
}

// This is a naive implementation. I won't use patterns that trigger the
// non-linear cost, but if this gets more general purpose use, should probably
// implement a closer-to-optimal general purpose search.

static bool full_match(search_ctx_t *, void *);

bool
n00b_tree_match(n00b_tree_node_t *tree,
               n00b_tpat_node_t *pat,
               n00b_cmp_fn       cmp,
               n00b_list_t     **match_loc)
{
#if 0
    search_ctx_t search_state = {
        .tree_cur    = tree,
        .pattern_cur = pat,
        .cmp         = cmp,
        .captures    = NULL,
    };
#endif

    search_ctx_t *search_state = n00b_gc_alloc_mapped(search_ctx_t, N00B_GC_SCAN_ALL);

    search_state->tree_cur    = tree;
    search_state->pattern_cur = pat;
    search_state->cmp         = cmp;

    tpat_debug(search_state, "start");

    if (pat->min != 1 && pat->max != 1) {
        N00B_CRAISE("Pattern root must be a single node (non-optional) match.");
    }

    bool result = full_match(search_state, pat->contents);

    if (match_loc != NULL) {
        *match_loc = n00b_set_to_xlist(search_state->captures);
    }

    if (result) {
        tpat_debug(search_state, "end: success!");
    }
    else {
        tpat_debug(search_state, "end: fail :(");
    }
    return result;
}

static inline bool
content_matches(search_ctx_t *ctx, void *contents)
{
    return (*ctx->cmp)(contents, ctx->tree_cur);
}

static inline void
capture(search_ctx_t *ctx, n00b_tree_node_t *node)
{
    if (ctx->captures == NULL) {
        ctx->captures = n00b_set(n00b_type_tree(n00b_type_ref()));
    }

    hatrack_set_add(ctx->captures, node);
}

static int
count_consecutive_matches(search_ctx_t    *ctx,
                          n00b_tree_node_t *parent,
                          int              next_child,
                          void            *contents,
                          int              max,
                          n00b_list_t     **captures)
{
    n00b_set_t  *saved_captures     = ctx->captures;
    n00b_list_t *per_match_captures = NULL;
    int         result             = 0;

    ctx->captures = NULL;

    if (captures != NULL) {
        per_match_captures = n00b_list(n00b_type_tree(n00b_type_ref()));
        *captures          = per_match_captures;
    }

    for (; next_child < parent->num_kids; next_child++) {
        ctx->tree_cur = parent->children[next_child];

        if (!full_match(ctx, contents)) {
            break;
        }

        if (++result == max) {
            break;
        }

        if (captures != NULL) {
            n00b_list_append(per_match_captures, ctx->captures);
        }
        ctx->captures = NULL;
    }

    if (captures != NULL) {
        n00b_list_append(per_match_captures, ctx->captures);
    }

    ctx->captures = saved_captures;
    return result;
}

int debug_flag = false;
static bool
kid_match_from(search_ctx_t    *ctx,
               n00b_tree_node_t *parent,
               n00b_tpat_node_t *parent_pattern,
               int              next_child,
               int              next_pattern,
               void            *contents)
{
    n00b_tpat_node_t *subpattern   = parent_pattern->children[next_pattern];
    n00b_list_t      *kid_captures = NULL;
    int              num_matches;

    ctx->pattern_cur = subpattern;
    // We start by seeing how many sequential matches we can find.
    // The call will limit itself to the pattern's 'max' value,
    // but we check the 'min' field in this function after.

    num_matches = count_consecutive_matches(ctx,
                                            parent,
                                            next_child,
                                            contents,
                                            subpattern->max,
                                            &kid_captures);
    if (num_matches < subpattern->min) {
        return false;
    }

    int kid_capture_ix = 0;

    // Capture any nodes that are definitely part of this match.
    for (int i = next_child; i < subpattern->min; i++) {
        n00b_set_t *one_set = n00b_list_get(kid_captures,
                                          kid_capture_ix++,
                                          NULL);
        ctx->captures      = merge_captures(ctx->captures, one_set);
    }

    // Here we are looking to advance to the next pattern, but if
    // there isn't one, we just need to know if what we matched consumes
    // all possible child nodes. If it does, then we successfully matched,
    // and if there are leftover child nodes, we did not match.

    if (next_pattern + 1 >= (int)parent_pattern->num_kids) {
        if (next_child + num_matches < parent->num_kids) {
            return false;
        }

        for (int i = kid_capture_ix; i < n00b_list_len(kid_captures); i++) {
            n00b_set_t *one_set = n00b_list_get(kid_captures,
                                              i,
                                              NULL);

            ctx->captures = merge_captures(ctx->captures, one_set);
        }
        return true;
    }

    // Here, we know we have more patterns to check.
    // If this pattern is an n:m match where n:m, we're going to
    // try every valid match until the rest of the siblings match or
    // we run out of options.
    //
    // We could do better, but it's much more complicated to do so.
    // As is, we do this essentially via recursion;
    //
    // MINIMAL munch... we always return the shortest path to a
    // sibling match.  Meaning, if we do: .(x?, <any_text>*, t) and ask
    // to capture node 1, it will never capture because we first accept
    // the null match, try the rest, and only accept the one-node match
    // if the null match fails.
    //
    // If it turns out there's a good reason for maximal munch, we
    // just need to run this loop backwards.
    next_pattern++;

    next_child += subpattern->min;

    n00b_tpat_node_t *pnew          = parent_pattern->children[next_pattern];
    void            *next_contents = pnew->contents;

    for (int i = 0; i <= num_matches; i++) {
        n00b_set_t *copy = NULL;

        if (ctx->captures != NULL) {
            copy = n00b_set_shallow_copy(ctx->captures);
        }

        if (kid_match_from(ctx,
                           parent,
                           parent_pattern,
                           next_child + i,
                           next_pattern,
                           next_contents)) {
            static int i = 0;
            if (i++ == 45) {
                debug_flag = true;
            }

            ctx->captures = merge_captures(ctx->captures, copy);
            if (kid_capture_ix < n00b_list_len(kid_captures)) {
                n00b_set_t *one_set = n00b_list_get(kid_captures,
                                                  kid_capture_ix,
                                                  NULL);
                ctx->captures      = merge_captures(ctx->captures, one_set);
            }
            return true;
        }
        else {
            ctx->captures = copy;

            // Since the next rule didn't work w/ this
            // node that DOES work for us, if there's a capture
            // it's time to stash it.
            n00b_set_t *one_set = n00b_list_get(kid_captures,
                                              kid_capture_ix++,
                                              NULL);
            ctx->captures      = merge_captures(ctx->captures, one_set);
        }
    }

    return false;
}

static inline bool
children_match(search_ctx_t *ctx)
{
    if (ctx->pattern_cur->ignore_kids == 1) {
        return true;
    }

    n00b_tree_node_t *saved_parent  = ctx->tree_cur;
    n00b_tpat_node_t *saved_pattern = ctx->pattern_cur;
    bool             result;

    if (saved_pattern->num_kids == 0) {
        if (saved_parent->num_kids == 0 || saved_pattern->ignore_kids) {
            return true;
        }
        return false;
    }

    void *contents = ctx->pattern_cur->children[0]->contents;

    result = kid_match_from(ctx,
                            saved_parent,
                            saved_pattern,
                            0,
                            0,
                            contents);

    ctx->tree_cur    = saved_parent;
    ctx->pattern_cur = saved_pattern;

    return result;
}

static bool
walk_match(search_ctx_t *ctx, void *contents, bool capture_match)
{
    // This only gets called from full_match, which saves/restores.
    // Since this function always goes down, and never needs to
    // ascend, we don't restore when we're done; full_match does it.

    n00b_tree_node_t *current_tree_node = ctx->tree_cur;

    if (content_matches(ctx, contents)) {
        if (children_match(ctx)) {
            if (capture_match) {
                capture(ctx, current_tree_node);
            }
            return true;
        }
    }

    for (int i = 0; i < current_tree_node->num_kids; i++) {
        ctx->tree_cur = n00b_tree_get_child(current_tree_node, i);

        if (walk_match(ctx, contents, capture_match)) {
            return true;
        }
    }

    return false;
}

static bool
full_match(search_ctx_t *ctx, void *contents)
{
    // Full match doesn't look at min/max; it checks content and
    // children only.

    n00b_tree_node_t *saved_tree_node = ctx->tree_cur;
    n00b_tpat_node_t *saved_pattern   = ctx->pattern_cur;
    n00b_set_t       *saved_captures  = ctx->captures;

    bool result = false;
    bool capture_result;

    if (ctx->pattern_cur->walk) {
        capture_result   = (bool)saved_pattern->capture;
        result           = walk_match(ctx, contents, capture_result);
        ctx->tree_cur    = saved_tree_node;
        ctx->pattern_cur = saved_pattern;
        ctx->captures    = merge_captures(saved_captures, ctx->captures);

        return result;
    }

    if (!(result = content_matches(ctx, contents))) {
        ctx->tree_cur    = saved_tree_node;
        ctx->pattern_cur = saved_pattern;
        ctx->captures    = saved_captures;
        return false;
    }

    capture_result   = (bool)saved_pattern->capture;
    result           = children_match(ctx);
    ctx->captures    = merge_captures(saved_captures, ctx->captures);
    ctx->tree_cur    = saved_tree_node;
    ctx->pattern_cur = saved_pattern;

    if (result == true && capture_result == true) {
        capture(ctx, saved_tree_node);
    }

    return result;
}
