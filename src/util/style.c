#include "n00b.h"

n00b_style_t default_style = 0;

n00b_style_t
n00b_apply_bg_color(n00b_style_t style, n00b_utf8_t *name)
{
    int64_t color = (int64_t)n00b_lookup_color(name);

    style &= N00B_STY_CLEAR_BG;
    style |= N00B_STY_BG;
    style |= (color << N00B_BG_SHIFT);

    return style;
}

n00b_style_t
n00b_apply_fg_color(n00b_style_t style, n00b_utf8_t *name)
{
    int64_t color = (int64_t)n00b_lookup_color(name);

    style &= N00B_STY_CLEAR_FG;
    style |= N00B_STY_FG;
    style |= color;

    return style;
}
void
n00b_style_gaps(n00b_str_t *s, n00b_style_t gapstyle)
{
    if (!s->styling || !s->styling->num_entries) {
        n00b_str_apply_style(s, gapstyle, 0);
        return;
    }

    int num_gaps = 0;
    int last_end = 0;
    int num_cp   = n00b_str_codepoint_len(s);

    for (int i = 0; i < s->styling->num_entries; i++) {
        n00b_style_entry_t style = s->styling->styles[i];
        if (style.start > last_end) {
            num_gaps++;
        }
        last_end = style.end;
    }
    if (num_cp > last_end) {
        num_gaps++;
    }

    if (!num_gaps) {
        return;
    }
    n00b_style_info_t *old    = s->styling;
    int               new_ix = 0;

    n00b_alloc_styles(s, old->num_entries + num_gaps);

    last_end = 0;

    for (int i = 0; i < old->num_entries; i++) {
        n00b_style_entry_t style = old->styles[i];

        if (style.start > last_end) {
            n00b_style_entry_t filler = {
                .start = last_end,
                .end   = style.start,
                .info  = gapstyle,
            };

            s->styling->styles[new_ix++] = filler;
        }

        s->styling->styles[new_ix++] = old->styles[i];
        last_end                     = old->styles[i].end;
    }

    if (last_end != num_cp) {
        n00b_style_entry_t filler = {
            .start = last_end,
            .end   = num_cp,
            .info  = gapstyle,
        };

        s->styling->styles[new_ix] = filler;
    }
}

void
n00b_str_layer_style(n00b_str_t  *s,
                    n00b_style_t additions,
                    n00b_style_t subtractions)
{
    if (!s->styling || !s->styling->num_entries) {
        n00b_str_set_style(s, additions);
        return;
    }

    n00b_style_t turn_off = ~(subtractions & ~N00B_STY_CLEAR_FLAGS);

    if (additions & N00B_STY_FG) {
        turn_off |= N00B_STY_FG_BITS;
    }
    if (additions & N00B_STY_BG) {
        turn_off |= N00B_STY_BG_BITS;
    }

    for (int i = 0; i < s->styling->num_entries; i++) {
        s->styling->styles[i].info &= turn_off;
        s->styling->styles[i].info |= additions;
    }
}
