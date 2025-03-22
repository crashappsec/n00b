#include "n00b.h"

// Assume a possible break every 2^n codepoints when allocating, but
// be prepared to alloc extra if needed.
//
// n will vary based on the operation; for a grapheme break list, we
// would expect almost 1:1 in most text, but word boundaries (for line
// breaks) n = 2 probably minimizes our need to realloc.
//
// And, if we do run out of room, we double the number of slots, from
// this initial estimate.
//
// For long text, this might suck a bit, but it's probably better than
// lots of little allocs.
//
// Over time, if it seems like we neeed to, we may make this smarter.

const int n00b_minimum_break_slots = 16;

n00b_break_info_t *
n00b_breaks_grapheme(n00b_string_t *s,
                     int32_t        start_ix,
                     int32_t        end_ix)
{
    if (!s) {
        return NULL;
    }

    n00b_break_info_t *res   = n00b_break_alloc(s, 1);
    int32_t            state = 0;
    int32_t            cps   = n00b_string_codepoint_len(s);
    int32_t            len;

    if (start_ix < 0) {
        start_ix += cps;
    }

    if (end_ix <= 0) {
        end_ix += cps;
    }

    if (start_ix > cps || end_ix <= start_ix) {
        return res;
    }

    len = end_ix - start_ix;

    if (len < 2) {
        n00b_break_add(&res, end_ix);
        return res;
    }

    int32_t  prev;
    int32_t  cur;
    int32_t  i = 0;
    uint8_t *p = (uint8_t *)s->data;

    while (i <= start_ix) {
        p += utf8proc_iterate(p, 4, &prev);
        i++;
    }

    while (i < end_ix) {
        p += utf8proc_iterate(p, 4, &cur);
        if (utf8proc_grapheme_break_stateful(prev, cur, &state)) {
            n00b_break_add(&res, i);
        }
        i += 1;
        prev = cur;
    }
    return res;
}

static inline bool
is_line_break(int32_t cp)
{
    if (cp == '\n' || cp == '\r') {
        return true;
    }

    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_ZL:
    case UTF8PROC_CATEGORY_ZP:
        return true;
    default:
        return false;
    }
}

n00b_break_info_t *
n00b_breaks_line(n00b_string_t *s)
{
    if (!s) {
        return NULL;
    }

    n00b_break_info_t *res = n00b_break_alloc(s, 6); // 2^6 = 64.
    int32_t            l   = n00b_string_codepoint_len(s);

    int32_t  cp;
    uint8_t *p = (uint8_t *)s->data;

    for (int i = 0; i < l; i++) {
        p += utf8proc_iterate(p, 4, &cp);
        if (is_line_break(cp)) {
            n00b_break_add(&res, i);
        }
    }

    return res;
}

n00b_break_info_t *
n00b_breaks_all_options(n00b_string_t *s)
{
    if (!s) {
        return NULL;
    }

    int32_t            l   = n00b_string_codepoint_len(s);
    n00b_break_info_t *res = n00b_break_alloc(s, 0);
    char              *br_raw;

    br_raw = (char *)n00b_gc_raw_alloc(n00b_string_utf8_len(s), NULL);
    set_linebreaks_utf8_per_code_point((const utf8_t *)s->data,
                                       n00b_string_utf8_len(s),
                                       "en",
                                       br_raw);

    for (int i = 0; i < l; i++) {
        if (br_raw[i] < N00B_LB_NOBREAK) {
            n00b_break_add(&res, i);
        }
    }

    return res;
}

static int32_t
find_hwrap(n00b_string_t *s, int32_t offset, int32_t width)
{
    uint32_t *u32 = (uint32_t *)s->u32_data;
    int       l   = n00b_string_codepoint_len(s);

    for (int i = offset; i < l; i++) {
        width -= n00b_codepoint_width(u32[i]);
        if (width < 0) {
            return i;
        }
    }
    return l;
}

/**
 ** This doesn't directly change the string; it instead
 ** returns a list of codepoints that start lines, when
 ** wrapping to a given width.
 **
 ** This is a very simple best-fit algorithm right now.
 **
 ** If there is an actual newline in the contents, it is always
 ** returned, which can lead to short lines. This is done because we
 ** expect this to represent a paragraph break.
 **/
n00b_break_info_t *
n00b_break_wrap(n00b_string_t *s, int32_t width, int32_t hang)
{
    n00b_string_require_u32(s);

    if (width <= 0) {
        width = n00b_max(N00B_MIN_RENDER_WIDTH, n00b_terminal_width());
    }

    n00b_break_info_t *line_breaks  = n00b_breaks_line(s);
    n00b_break_info_t *break_ops    = n00b_breaks_all_options(s);
    int32_t            n            = 32 - __builtin_clz(width);
    int32_t            l            = n00b_string_codepoint_len(s);
    n00b_break_info_t *res          = n00b_break_alloc(s, n);
    int32_t            cur_start    = 0;
    int32_t            last_ok_br   = 0;
    int32_t            lb_ix        = 0;
    int32_t            bo_ix        = 0;
    int32_t            hard_wrap_ix = find_hwrap(s, 0, width);
    int32_t            hang_width   = width - hang;
    int32_t            next_lb;

    if (line_breaks->num_breaks == 0) {
        next_lb = l;
    }
    else {
        next_lb = line_breaks->breaks[0];
        lb_ix   = 1;
    }

    n00b_break_add(&res, 0);

    while (cur_start < l) {
find_next_break:
        last_ok_br = cur_start;

        while (bo_ix < break_ops->num_breaks) {
            int32_t cur_break = break_ops->breaks[bo_ix];

            if (cur_break >= hard_wrap_ix) {
                if (last_ok_br == cur_start) {
                    // No valid break; hard wrap it.
                    n00b_break_add(&res, hard_wrap_ix);
                    cur_start    = hard_wrap_ix;
                    hard_wrap_ix = find_hwrap(s, cur_start, hang_width);
                    goto find_next_break;
                }
                else {
                    n00b_break_add(&res, last_ok_br);
                    cur_start    = last_ok_br;
                    hard_wrap_ix = find_hwrap(s, cur_start, hang_width);
                    goto find_next_break;
                }
            }

            if (next_lb == cur_break) {
                n00b_break_add(&res, next_lb + 1);
                cur_start    = next_lb + 1;
                hard_wrap_ix = find_hwrap(s, cur_start, hang_width);
                if (lb_ix == line_breaks->num_breaks) {
                    next_lb = l;
                }
                else {
                    next_lb = line_breaks->breaks[lb_ix++];
                }
                bo_ix++;
                goto find_next_break;
            }

            last_ok_br = cur_break + 1;
            bo_ix++;
        }
        break;
    }

    while (true) {
        if (l == cur_start) {
            break;
        }
        if (l <= hard_wrap_ix) {
            break;
        }

        if (last_ok_br > cur_start) {
            n00b_break_add(&res, last_ok_br);
            cur_start    = last_ok_br;
            hard_wrap_ix = find_hwrap(s, cur_start, hang_width);
        }
        else {
            n00b_break_add(&res, hard_wrap_ix);
            cur_start    = hard_wrap_ix;
            hard_wrap_ix = find_hwrap(s, cur_start, hang_width);
        }
    }

    return res;
}

#define advance()                                \
    l = utf8proc_iterate((uint8_t *)p, 4, &cur); \
    assert(l > 0);                               \
    p += l

static inline bool
is_AHLetter(n00b_wb_kind k)
{
    if (k == N00B_WB_ALetter || k == N00B_WB_Hebrew_Letter) {
        return true;
    }
    return false;
}

static inline bool
is_MidNumLetQ(n00b_wb_kind k)
{
    if (k == N00B_WB_MidNumLet || k == N00B_WB_Single_Quote) {
        return true;
    }

    return false;
}

static inline bool
is_CRLF(n00b_wb_kind k)
{
    if (k == N00B_WB_CR || k == N00B_WB_LF) {
        return true;
    }

    return false;
}

static inline bool
is_new_line(n00b_wb_kind k)
{
    return is_CRLF(k) || k == N00B_WB_Newline;
}

static inline bool
is_ignored(n00b_wb_kind k)
{
    switch (k) {
    case N00B_WB_Extend:
    case N00B_WB_Format:
    case N00B_WB_ZWJ:
        return true;
    default:
        return false;
    }
}

static inline bool
in_no_break_punct(n00b_wb_kind k1, n00b_wb_kind k2, n00b_wb_kind k3)
{
    if (!is_AHLetter(k1) || !is_AHLetter(k3)) {
        return false;
    }

    return is_MidNumLetQ(k2) || (k2 == N00B_WB_MidLetter);
}

static inline bool
is_number_punctuation(n00b_wb_kind k1, n00b_wb_kind k2, n00b_wb_kind k3)
{
    if (k1 != N00B_WB_Numeric || k3 != N00B_WB_Numeric) {
        return false;
    }

    if (is_MidNumLetQ(k2) || k2 == N00B_WB_MidNum) {
        return true;
    }

    return false;
}

static inline bool
is_extender_prefix(n00b_wb_kind k)
{
    switch (k) {
    case N00B_WB_ALetter:
    case N00B_WB_Hebrew_Letter:
    case N00B_WB_Numeric:
    case N00B_WB_Katakana:
    case N00B_WB_ExtendNumLet:
        return true;
    default:
        return false;
    }
}

static inline bool
is_extender_postfix(n00b_wb_kind k)
{
    switch (k) {
    case N00B_WB_ALetter:
    case N00B_WB_Hebrew_Letter:
    case N00B_WB_Numeric:
    case N00B_WB_Katakana:
        return true;
    default:
        return false;
    }
}

n00b_break_info_t *
n00b_word_breaks(n00b_string_t *s)
{
    n00b_break_info_t *res = n00b_break_alloc(s, 6);
    n00b_codepoint_t   cur = 0;
    n00b_wb_kind       next_cat;
    n00b_wb_kind       cur_cat;
    n00b_wb_kind       prev_cat = N00B_WB_Other;
    char              *p        = s->data;
    char              *end      = p + n00b_string_byte_len(s);
    int                l;
    int                i = 0;

    // WB1
    if (!s || !s->codepoints) {
        return res;
    }
    n00b_break_add(&res, 0);

    advance();
    cur_cat = n00b_codepoint_word_break_prop(cur);

    while (p < end) {
        advance();
        i++;
        next_cat = n00b_codepoint_word_break_prop(cur);

        // WB3
        if (cur_cat == N00B_WB_CR && next_cat == N00B_WB_LF) {
            goto next;
        }
        // WB3a/b
        if (is_new_line(cur_cat) || is_new_line(next_cat)) {
            goto add_break;
        }

        // Not doing WB3c yet.
        // WB3d
        if (cur_cat == N00B_WB_WSegSpace && next_cat == N00B_WB_WSegSpace) {
            goto next;
        }
        // WB4
        if (is_ignored(next_cat)) {
            continue;
        }

        if (is_AHLetter(cur_cat)) {
            // WB5
            if (is_AHLetter(next_cat)) {
                goto next;
            }
            // WB9
            if (next_cat == N00B_WB_Numeric) {
                goto next;
            }
        }

        // WB6 and WB7; In this case, we will have already added a break
        // between AHLetter (MidLetter | MidNumLetQ) that we need to remove.
        if (in_no_break_punct(prev_cat, cur_cat, next_cat)) {
            n00b_undo_last_break(res);
            goto next;
        }

        // WB 7A
        if (cur_cat == N00B_WB_Hebrew_Letter
            && next_cat == N00B_WB_Single_Quote) {
            goto next;
        }

        // WB 7B and 7C
        if (prev_cat == N00B_WB_Hebrew_Letter && cur_cat == N00B_WB_Double_Quote
            && next_cat == N00B_WB_Hebrew_Letter) {
            n00b_undo_last_break(res);
            goto next;
        }

        if (cur_cat == N00B_WB_Numeric) {
            // WB8
            if (next_cat == N00B_WB_Numeric) {
                goto next;
            }
            // WB10
            if (is_AHLetter(next_cat)) {
                goto next;
            }
        }

        // WB11 and WB12
        if (is_number_punctuation(prev_cat, cur_cat, next_cat)) {
            n00b_undo_last_break(res);
            goto next;
        }

        // WB13
        if (cur_cat == N00B_WB_Katakana && next_cat == cur_cat) {
            goto next;
        }

        // WB13a
        if (next_cat == N00B_WB_ExtendNumLet && is_extender_prefix(cur_cat)) {
            goto next;
        }

        // WB13b
        if (cur_cat == N00B_WB_ExtendNumLet && is_extender_postfix(next_cat)) {
            goto next;
        }

        // WB15 and WB16. Basically we really just need to keep track
        // of pairs of Regional Indicators, so whenever we see a 'first'
        // one, in addition to not breaking, we set its value to
        // 'Next_Breaks' to let us know we can go ahead and break.
        if (cur_cat == N00B_WB_Regional_Indicator) {
            if (prev_cat != N00B_WB_Regional_Indicator_Next_Breaks) {
                cur_cat = N00B_WB_Regional_Indicator_Next_Breaks;
                goto next;
            }
        }

        // There is no WB14 (probably removed?)  However, I do not
        // like how the algorithm fails to group punctuation. For
        // instance, in: "What-- He said that?!?!?!"
        //
        // The algorithm makes each bit of punctuation here its own
        // word. So we have our own rule for that.

        if (cur_cat == N00B_WB_Other && next_cat == N00B_WB_Other) {
            goto next;
        }

        // fallthrough
add_break:
        // WB999
        n00b_break_add(&res, i);
        // fallthrough
next:
        prev_cat = cur_cat;
        cur_cat  = next_cat;
    }

    // WB2
    if (i) {
        n00b_break_add(&res, i);
    }

    return res;
}
