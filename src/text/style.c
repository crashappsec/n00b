#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_string_t *
n00b_string_style(n00b_string_t *s, n00b_text_element_t *style)
{
    n00b_string_t *result = n00b_string_reuse_text(s);

    if (!s->styling || s->styling->num_styles < 1) {
        result->styling = n00b_gc_flex_alloc(
            n00b_string_style_info_t,
            n00b_style_record_t,
            1,
            N00B_GC_SCAN_ALL);
        result->styling->num_styles      = 1;
        result->styling->styles[0].start = 0;
        result->styling->styles[0].end   = s->codepoints;
        result->styling->styles[0].info  = style;

        return result;
    }

    result->styling = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                         n00b_style_record_t,
                                         s->styling->num_styles,
                                         N00B_GC_SCAN_ALL);

    result->styling->num_styles = s->styling->num_styles;

    for (int i = 0; i < result->styling->num_styles; i++) {
        result->styling->styles[i].start = s->styling->styles[i].start;
        result->styling->styles[i].end   = s->styling->styles[i].end;
        result->styling->styles[i].info  = s->styling->styles[i].info;
    }

    result->styling->base_style = style;

    return result;
}

n00b_string_t *
n00b_string_style_extended(n00b_string_t *s, n00b_text_element_t *style)
{
    // Turns on style extension, as long as the string you're
    // calling this on isn't pre-styled.

    n00b_string_t *result = n00b_string_style(s, style);

    if (!result->styling->base_style) {
        result->styling->styles[0].end = -1;
    }

    return result;
}

n00b_text_element_t *
n00b_text_style_overlay(n00b_text_element_t *base, n00b_text_element_t *new)
{
    n00b_text_element_t *result = n00b_gc_alloc_mapped(n00b_text_element_t,
                                                       N00B_GC_SCAN_ALL);
    memcpy(result, base, sizeof(n00b_text_element_t));

    if (new->fg_palate_index != N00B_COLOR_PALATE_UNSPECIFIED) {
        result->fg_palate_index = new->fg_palate_index;
    }
    if (new->bg_palate_index != N00B_COLOR_PALATE_UNSPECIFIED) {
        result->bg_palate_index = new->bg_palate_index;
    }
    if (new->underline != N00B_3_UNSPECIFIED) {
        result->underline = new->underline;
    }
    if (new->double_underline != N00B_3_UNSPECIFIED) {
        result->double_underline = new->double_underline;
    }
    if (new->bold != N00B_3_UNSPECIFIED) {
        result->bold = new->bold;
    }
    if (new->italic != N00B_3_UNSPECIFIED) {
        result->italic = new->italic;
    }
    if (new->strikethrough != N00B_3_UNSPECIFIED) {
        result->strikethrough = new->strikethrough;
    }
    if (new->reverse != N00B_3_UNSPECIFIED) {
        result->reverse = new->reverse;
    }
    if (new->text_case != N00B_TEXT_UNSPECIFIED) {
        result->text_case = new->text_case;
    }

    return result;
}

n00b_string_t *
_n00b_string_style_ranges(n00b_string_t       *s,
                          n00b_text_element_t *style,
                          int64_t              start,
                          int64_t              end,
                          ...)
{
    int64_t        last_end = 0;
    n00b_string_t *result   = n00b_string_reuse_text(s);
    n00b_list_t   *info     = n00b_list(n00b_type_ref());

    va_list vargs;
    va_start(vargs, end);

    n00b_assert(style);

    while (true) {
        if (start < 0) {
            start += s->codepoints;
        }
        if (end < 0) {
            end += s->codepoints;
        }
        else {
            if (end > s->codepoints) {
                N00B_CRAISE("String range end out of bounds.");
            }
        }
        if (start > end) {
            N00B_CRAISE("String range start past range end.");
        }
        if (start == end) {
            goto next_range;
        }
        if (start < last_end) {
            N00B_CRAISE("String ranges cannot overlap when styling.");
        }
        n00b_private_list_append(info, (void *)(int64_t)start);
        n00b_private_list_append(info, (void *)(int64_t)end);
        n00b_private_list_append(info, (void *)style);
        last_end = end;

next_range:
        style = va_arg(vargs, n00b_text_element_t *);

        if (!style) {
            break;
        }

        start = va_arg(vargs, int64_t);
        end   = va_arg(vargs, int64_t);
    }

    int n = n00b_list_len(info) / 3;
    int j = 0;

    result->styling             = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                         n00b_style_record_t,
                                         n,
                                         N00B_GC_SCAN_ALL);
    result->styling->num_styles = n;

    for (int i = 0; i < n; i++) {
        start = (int64_t)n00b_private_list_get(info, j++, NULL);
        end   = (int64_t)n00b_private_list_get(info, j++, NULL);
        style = n00b_private_list_get(info, j++, NULL);

        result->styling->styles[i].start = start;
        result->styling->styles[i].end   = end;
        result->styling->styles[i].info  = style;
    }

    va_end(vargs);

    return result;
}

// Overhead: 3 (ESC, [, m)
// Bold: 2
// Rev: 2
// St: 2
// Italic: 2
// Ul: 3 (when double)
// BG: 17 (when truecolor)
// FG: 16 (truecolor; never a ; after)
//
// Total = 47
#define LONGEST   47
#define RESET_LEN 5

static inline void
stash_ansi_codes(n00b_text_element_t *t,
                 n00b_theme_t        *theme,
                 n00b_text_element_t *ih)
{
    if (t->ansi_cache && t->last_theme == t && t->last_inherit == ih) {
        return;
    }

    bool bold   = t->bold == N00B_3_YES;
    bool ul     = t->underline == N00B_3_YES;
    bool uul    = t->double_underline == N00B_3_YES;
    bool rev    = t->reverse == N00B_3_YES;
    bool st     = t->strikethrough == N00B_3_YES;
    bool italic = t->italic == N00B_3_YES;
    int  bg_ix  = t->bg_palate_index;
    int  fg_ix  = t->fg_palate_index;

    if (ih) {
        if (!bold && t->bold == N00B_3_UNSPECIFIED) {
            if (ih->bold == N00B_3_YES) {
                bold = true;
            }
        }
        if (!ul && t->underline == N00B_3_UNSPECIFIED) {
            if (ih->underline == N00B_3_YES) {
                ul = true;
            }
        }
        if (!uul && t->double_underline == N00B_3_UNSPECIFIED) {
            if (ih->double_underline == N00B_3_YES) {
                uul = true;
            }
        }
        if (!rev && t->reverse == N00B_3_UNSPECIFIED) {
            if (ih->reverse == N00B_3_YES) {
                rev = true;
            }
        }
        if (!st && t->strikethrough == N00B_3_UNSPECIFIED) {
            if (ih->strikethrough == N00B_3_YES) {
                st = true;
            }
        }

        if (!italic && t->italic == N00B_3_UNSPECIFIED) {
            if (ih->italic == N00B_3_YES) {
                italic = true;
            }
        }
        if (bg_ix == N00B_COLOR_PALATE_UNSPECIFIED) {
            bg_ix = ih->bg_palate_index;
        }
        if (fg_ix == N00B_COLOR_PALATE_UNSPECIFIED) {
            fg_ix = ih->fg_palate_index;
        }
    }

    char code[LONGEST] = {
        '\e',
        '[',
        0,
    };
    char *p;

    p = &code[2];

    if (bold) {
        *p++ = '1';
        *p++ = ';';
    }
    if (rev) {
        *p++ = '7';
        *p++ = ';';
    }
    if (st) {
        *p++ = '9';
        *p++ = ';';
    }
    if (italic) {
        *p++ = '3';
        *p++ = ';';
    }

    if (ul && !uul) {
        *p++ = '4';
        *p++ = ';';
    }
    if (uul) {
        *p++ = '2';
        *p++ = '1';
        *p++ = ';';
    }

    int32_t color;
    int32_t tmp;

    if (bg_ix != N00B_COLOR_PALATE_UNSPECIFIED) {
        color = theme->palate[bg_ix];

        if (n00b_use_truecolor()) {
            uint8_t r, g, b;
            r    = color >> 16;
            g    = (color & 0x00ffff) >> 8;
            b    = color & 0xff;
            *p++ = '4';
            *p++ = '8';
            *p++ = ';';
            *p++ = '2';
            *p++ = ';';

            tmp  = r / 100;
            *p++ = '0' + tmp;
            tmp  = (r % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + r % 10;
            *p++ = ';';

            tmp  = g / 100;
            *p++ = '0' + tmp;
            tmp  = (g % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + g % 10;
            *p++ = ';';

            tmp  = b / 100;
            *p++ = '0' + tmp;
            tmp  = (b % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + b % 10;
        }
        else {
            *p++  = '4';
            *p++  = '8';
            *p++  = ';';
            *p++  = '5';
            *p++  = ';';
            color = n00b_to_vga(color);

            *p++ = '0' + color / 100;
            color %= 100;
            *p++ = '0' + color / 10;
            color %= 10;
            *p++ = '0' + color;
        }
        *p++ = ';';
    }
    if (fg_ix != N00B_COLOR_PALATE_UNSPECIFIED) {
        color = theme->palate[fg_ix];

        if (n00b_use_truecolor()) {
            uint8_t r, g, b;
            r    = color >> 16;
            g    = (color & 0x00ffff) >> 8;
            b    = color & 0xff;
            *p++ = '3';
            *p++ = '8';
            *p++ = ';';
            *p++ = '2';
            *p++ = ';';

            tmp  = r / 100;
            *p++ = '0' + tmp;
            tmp  = (r % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + r % 10;
            *p++ = ';';

            tmp  = g / 100;
            *p++ = '0' + tmp;
            tmp  = (g % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + g % 10;
            *p++ = ';';

            tmp  = b / 100;
            *p++ = '0' + tmp;
            tmp  = (b % 100) / 10;
            *p++ = '0' + tmp;
            *p++ = '0' + b % 10;
        }
        else {
            *p++  = '3';
            *p++  = '8';
            *p++  = ';';
            *p++  = '5';
            *p++  = ';';
            color = n00b_to_vga(color);

            *p++ = '0' + color / 100;
            color %= 100;
            *p++ = '0' + color / 10;
            color %= 10;
            *p++ = '0' + color;
        }
    }

    if (*(p - 1) == ';') {
        p--;
    }

    *p++          = 'm';
    *p            = 0;
    t->last_theme = theme;
    t->ansi_len   = p - &code[0];

    if (t->ansi_len == 3) {
        // No codes set.
        t->ansi_len = 0;
        return;
    }

    t->ansi_cache = n00b_gc_array_value_alloc(char, t->ansi_len + 1);
    memcpy(t->ansi_cache, &code[0], t->ansi_len);
}

static inline void
copy_style_cache(char **pp, n00b_text_element_t *style)
{
    char *p = *pp;
    char *q = style->ansi_cache;

    for (int i = 0; i < style->ansi_len; i++) {
        *p++ = *q++;
    }

    *pp = p;
}

static inline void
add_reset_code(char **pp)
{
    char *p = *pp;
    *p++    = '\e';
    *p++    = '[';
    *p++    = '0';
    *p++    = 'm';

    *pp = p;
}

static n00b_codepoint_t
apply_case(n00b_codepoint_t     cp,
           n00b_text_element_t *cur,
           n00b_text_element_t *def,
           bool                *state)
{
    n00b_text_case_t tc = cur->text_case;
    n00b_codepoint_t result;

    if (tc == N00B_TEXT_UNSPECIFIED) {
        tc = def->text_case;
    }

    switch (tc) {
    case N00B_TEXT_UPPER: // Title Case
        result = n00b_codepoint_title_case(cp, state);
        break;
    case N00B_TEXT_LOWER:
        result = n00b_codepoint_lower_case(cp);
        break;
    case N00B_TEXT_ALL_CAPS:
        result = n00b_codepoint_upper_case(cp);
        break;
    default:
        result = cp;
    }

    switch (utf8proc_category(result)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
        *state = true;
        break;
    default:
        *state = false;
        break;
    }

    return result;
}

static inline void
copy_codepoint(char               **destp,
               char               **srcp,
               n00b_text_element_t *cur,
               n00b_text_element_t *def,
               bool                *state)

{
    char            *dest = *destp;
    char            *src  = *srcp;
    n00b_codepoint_t cp;

    int l = utf8proc_iterate((uint8_t *)src, 4, &cp);

    n00b_assert(l >= 0);

    *srcp = src + l;
    cp    = apply_case(cp, cur, def, state);

    l = utf8proc_encode_char(cp, (uint8_t *)dest);

    n00b_assert(l >= 0);
    *destp = dest + l;
}

char *
n00b_rich_to_ansi(n00b_string_t *s, n00b_theme_t *theme)
{
    if (!theme) {
        theme = n00b_get_current_theme();
    }

    n00b_text_element_t *inherited_defaults = NULL;

    if (s->styling) {
        inherited_defaults = s->styling->base_style;
    }

    if (!inherited_defaults) {
        inherited_defaults = &theme->box_info[N00B_BOX_THEME_FLOW]->text_style;
    }

    if (inherited_defaults) {
        stash_ansi_codes(inherited_defaults, theme, NULL);
    }

    // We will calculate the length before allocating.
    int                  len  = s->u8_bytes;
    int                  gaps = 0;
    int                  last = 0;
    int                  n    = 0;
    n00b_text_element_t *style;

    if (s->styling) {
        n = s->styling->num_styles;
    }

    for (int i = 0; i < n; i++) {
        if (n00b_style_start(s, i) > last) {
            gaps++;
        }
        last  = n00b_style_end(s, i);
        style = s->styling->styles[i].info;
        stash_ansi_codes(style, theme, inherited_defaults);
        len += style->ansi_len + RESET_LEN;
    }

    if (last < s->codepoints) {
        gaps++;
    }

    if (inherited_defaults) {
        len += (inherited_defaults->ansi_len + RESET_LEN) * gaps;
    }

    // TODO: Figure out what I'm doing wrong that this is needed.
    len <<= 2;

    char *result     = n00b_gc_array_value_alloc(char, len + 1);
    char *srcp       = s->data;
    char *srcend     = s->data + s->u8_bytes;
    char *dstp       = result;
    int   style_ix   = 0;
    int   i          = 0;
    int   next_start = n00b_style_start(s, 0);
    bool  case_state = false;
    int   end;

    n00b_string_sanity_check(s);

    while (srcp < srcend) {
        if (style_ix < n && i == next_start) {
            copy_style_cache(&dstp, s->styling->styles[style_ix].info);
            end = n00b_style_end(s, style_ix);

            while (srcp < srcend && i < end) {
                copy_codepoint(&dstp,
                               &srcp,
                               s->styling->styles[style_ix].info,
                               inherited_defaults,
                               &case_state);
                i++;
            }
            add_reset_code(&dstp);
            next_start = n00b_style_start(s, ++style_ix);
        }
        if (i >= next_start) {
            i = next_start;
            continue;
        }
        // We're in fill space. We go from the current character
        // until we hit next_start, possibly filling the style.
        if (inherited_defaults && i < next_start) {
            copy_style_cache(&dstp, inherited_defaults);
            while (srcp < srcend && i < next_start) {
                copy_codepoint(&dstp,
                               &srcp,
                               inherited_defaults,
                               inherited_defaults,
                               &case_state);
                i++;
            }
            add_reset_code(&dstp);
        }
        else {
            while (i < next_start) {
                copy_codepoint(&dstp,
                               &srcp,
                               inherited_defaults,
                               inherited_defaults,
                               &case_state);
                i++;
            }
        }
    }
    *dstp = 0;

    return result;
}
