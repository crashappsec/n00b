#include "n00b.h"

static int64_t
n00b_next_highlight(n00b_buf_t       *buf,
                    n00b_highlight_t *h,
                    int64_t           start_offset,
                    n00b_list_t      *entries,
                    int               scale)
{
    int64_t             len            = n00b_len(buf) * scale;
    int64_t             start_text_len = 0;
    int64_t             end_text_len   = 0;
    int64_t             offset;
    n00b_style_entry_t *style = n00b_gc_alloc_mapped(n00b_style_entry_t,
                                                     N00B_GC_SCAN_NONE);

    if (start_offset < 0) {
        start_offset += len;
    }

    if (h->start_text) {
        start_text_len = n00b_len(h->start_text) * scale;
    }
    if (h->end_is_pattern) {
        end_text_len = n00b_len(h->end.text) * scale;
    }

    offset = start_offset;

    if (start_text_len) {
        offset = n00b_buffer_find(buf,
                                  h->start_text,
                                  n00b_kw("start", n00b_ka(start_offset)));
        if (offset == -1) {
            return len;
        }
        style->start = offset;
        offset += start_text_len + h->start_offset;
    }
    else {
        offset += h->start_offset;
        if (offset > len) {
            return len;
        }
        style->start = offset;
    }

    style->info = h->style;

    if (end_text_len) {
        offset = n00b_buffer_find(buf,
                                  h->end.text,
                                  n00b_kw("start", offset));
        if (offset == -1) {
            offset = len;
        }
    }
    else {
        offset += h->end.offset;
        if (offset > len) {
            offset = len;
        }
    }

    style->end = offset;
    n00b_list_append(entries, style);

    return offset;
}

static void
add_styling(n00b_str_t *s, n00b_list_t *styles)
{
    int n = n00b_list_len(styles);

    if (!n) {
        return;
    }

    n00b_alloc_styles(s, n);
    n00b_style_info_t *si = s->styling;

    si->num_entries = n;

    for (int i = 0; i < n; i++) {
        n00b_style_entry_t *e = n00b_list_get(styles, i, NULL);
        si->styles[i]         = *e;
    }

    s->styling = si;
}

static int
preprocess_input(void *mixed, n00b_buf_t **outbuf, n00b_str_t **outstr)
{
    n00b_type_t  *t = n00b_get_my_type(mixed);
    n00b_utf32_t *s;

    if (n00b_type_is_buffer(t)) {
        *outstr = NULL;
        *outbuf = mixed;
        return 1;
    }

    int result = n00b_str_index_width(mixed);

    if (result == 4) {
        s       = n00b_to_utf32(mixed);
        *outstr = s;
    }
    else {
        s       = mixed;
        *outstr = s;
    }

    *outbuf = n00b_new(n00b_type_buffer(),
                       n00b_kw("ptr",
                               s->data,
                               "length",
                               n00b_str_byte_len(*outstr)));

    return result;
}

// For buffers, returns the list of stylings as if the buffer is raw
// UTF-8. Otherwise, returns a copy of the string that's styled.
void *
n00b_apply_highlight(void *mixed, n00b_highlight_t *h)
{
    n00b_buf_t  *buf;
    n00b_utf8_t *outstr;
    int          scale  = preprocess_input(mixed, &buf, &outstr);
    int          ix     = 0;
    int          n      = n00b_len(buf);
    n00b_list_t *styles = n00b_list(n00b_type_ref());

    while (ix < n) {
        ix = n00b_next_highlight(buf, h, ix, styles, scale);
    }

    if (outstr) {
        add_styling(outstr, styles);
        return outstr;
    }
    else {
        return styles;
    }
}

void *
n00b_apply_highlights(void *mixed, n00b_list_t *highlights)
{
    n00b_buf_t  *buf;
    n00b_utf8_t *outstr;
    int          scale  = preprocess_input(mixed, &buf, &outstr);
    int          ix     = 0;
    int          slen   = n00b_len(buf);
    int          hlen   = n00b_list_len(highlights);
    n00b_list_t *styles = n00b_list(n00b_type_ref());

    while (ix < slen) {
        for (int i = 0; i < hlen; i++) {
            n00b_highlight_t *hl = n00b_list_get(highlights, i, NULL);
            ix                   = n00b_next_highlight(buf,
                                     hl,
                                     ix,
                                     styles,
                                     scale);

            if (ix >= slen) {
                break;
            }
        }
    }

    if (outstr) {
        add_styling(outstr, styles);
        return outstr;
    }
    else {
        return styles;
    }
}

n00b_highlight_t *
n00b_new_highlight(n00b_buf_t  *text,
                   int64_t      offset,
                   int64_t      len,
                   n00b_style_t style)
{
    n00b_highlight_t *result = n00b_gc_alloc_mapped(n00b_highlight_t,
                                                    N00B_GC_SCAN_ALL);
    result->start_text       = text;
    result->start_offset     = offset;
    result->end_is_pattern   = false;
    result->end.offset       = len;
    result->style            = style;

    return result;
}

extern n00b_highlight_t *
n00b_new_highlight_to_text(n00b_buf_t  *start_text,
                           int64_t      offset,
                           n00b_buf_t  *end_text,
                           n00b_style_t style)
{
    n00b_highlight_t *result = n00b_gc_alloc_mapped(n00b_highlight_t,
                                                    N00B_GC_SCAN_ALL);
    result->start_text       = start_text;
    result->start_offset     = offset;
    result->end_is_pattern   = true;
    result->end.text         = end_text;
    result->style            = style;

    return result;
}
