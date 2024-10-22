#include "n00b.h"

N00B_STATIC_ASCII_STR(n00b_empty_string_const, "");
N00B_STATIC_ASCII_STR(n00b_newline_const, "\n");
N00B_STATIC_ASCII_STR(n00b_crlf_const, "\r\n");

void
n00b_internal_utf8_set_codepoint_count(n00b_utf8_t *instr)
{
    uint8_t        *p   = (uint8_t *)instr->data;
    uint8_t        *end = p + instr->byte_len;
    n00b_codepoint_t cp;

    instr->codepoints = 0;
    while (p < end) {
        instr->codepoints += 1;
        int n = utf8proc_iterate(p, 4, &cp);
        if (n < 0) {
            // Assume we have a partial code point at the end.
            return;
        }
        p += n;
    }
}

int64_t
n00b_utf8_validate(const n00b_utf8_t *instr)
{
    uint8_t        *p   = (uint8_t *)instr->data;
    uint8_t        *end = p + instr->byte_len;
    n00b_codepoint_t cp;
    int64_t         n = 0;

    while (p < end) {
        int to_add = utf8proc_iterate(p, 4, &cp);

        if (to_add < 0) {
            return n;
        }

        p += to_add;

        n++;
    }

    return 0;
}

// For now, we're going to do this just for u32, so u8 will convert to
// u32, in full.
n00b_utf32_t *
n00b_str_slice(const n00b_str_t *instr, int64_t start, int64_t end)
{
    if (!instr || n00b_str_codepoint_len(instr) == 0) {
        return n00b_to_utf32(n00b_empty_string());
    }
    n00b_utf32_t *s   = n00b_to_utf32(instr);
    int64_t      len = n00b_str_codepoint_len(s);

    if (end < 0) {
        end += len;
    }
    else {
        if (end > len) {
            end = len;
        }
    }
    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return n00b_to_utf32(n00b_empty_string());
        }
    }
    if ((start | end) < 0 || start >= end) {
        return n00b_to_utf32(n00b_empty_string());
    }

    int64_t      slice_len = end - start;
    n00b_utf32_t *res       = n00b_new(n00b_type_utf32(),
                               n00b_kw("length", n00b_ka(slice_len)));

    res->codepoints = slice_len;

    n00b_codepoint_t *src = (n00b_codepoint_t *)s->data;
    n00b_codepoint_t *dst = (n00b_codepoint_t *)res->data;

    for (int i = 0; i < slice_len; i++) {
        dst[i] = src[start + i];
    }

    while (res->codepoints != 0) {
        if (dst[res->codepoints - 1] == 0) {
            --res->codepoints;
        }
        else {
            break;
        }
    }

    if (s->styling && s->styling->num_entries) {
        int64_t first = -1;
        int64_t last  = 0;
        int64_t i     = 0;

        for (i = 0; i < s->styling->num_entries; i++) {
            if (s->styling->styles[i].end < start) {
                continue;
            }
            if (s->styling->styles[i].start >= end) {
                break;
            }
            if (first == -1) {
                first = i;
            }
            last = i + 1;
        }

        if (first == -1) {
            return res;
        }

        last = i;
        while (true) {
            if (s->styling->styles[++last].end >= end) {
                break;
            }
            if (i == s->styling->num_entries) {
                break;
            }
            if (s->styling->styles[last].start >= end) {
                break;
            }
        }

        if (last == -1) {
            last = first + 1;
        }
        int64_t sliced_style_count = last - first;
        n00b_alloc_styles(res, sliced_style_count);

        for (i = 0; i < sliced_style_count; i++) {
            int64_t     sold = s->styling->styles[i + first].start;
            n00b_style_t info = s->styling->styles[i + first].info;
            int64_t     snew = n00b_max(sold - start, 0);
            int64_t     enew = n00b_min(s->styling->styles[i + first].end,
                                   end)
                         - start;

            if (enew > slice_len) {
                enew = slice_len;
            }

            if (snew >= enew) {
                res->styling->num_entries = i;
                break;
            }

            res->styling->styles[i].start = snew;
            res->styling->styles[i].end   = enew;
            res->styling->styles[i].info  = info;
        }
    }

    return res;
}

static inline n00b_codepoint_t
n00b_utf8_index(const n00b_utf8_t *s, int64_t n)
{
    int64_t l = n00b_str_codepoint_len(s);

    if (n < 0) {
        n += l;

        if (n < 0) {
            N00B_CRAISE("Index would be before the start of the string.");
        }
    }

    if (n >= l) {
        N00B_CRAISE("Index out of bounds.");
    }

    char           *p = (char *)s->data;
    n00b_codepoint_t cp;

    for (int i = 0; i <= n; i++) {
        int val = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (val < 0) {
            N00B_CRAISE("Index out of bounds.");
        }
        p += val;
    }

    return cp;
}

static inline n00b_codepoint_t
n00b_utf32_index(const n00b_utf32_t *s, int64_t i)
{
    int64_t l = n00b_str_codepoint_len(s);

    if (i < 0) {
        i += l;

        if (i < 0) {
            N00B_CRAISE("Index would be before the start of the string.");
        }
    }

    if (i >= l) {
        N00B_CRAISE("Index out of bounds.");
    }

    n00b_codepoint_t *p = (n00b_codepoint_t *)s->data;

    return p[i];
}

n00b_codepoint_t
n00b_index(const n00b_str_t *s, int64_t i)
{
    if (!n00b_str_is_u8(s)) {
        return n00b_utf32_index(s, i);
    }
    else {
        return n00b_utf8_index(s, i);
    }
}

static char n00b_hex_map_lower[16] = "0123456789abcdef";
static char n00b_hex_map_upper[16] = "0123456789ABCDEF";

n00b_utf8_t *
n00b_str_to_hex(n00b_str_t *s, bool upper)
{
    s = n00b_to_utf8(s);

    n00b_utf8_t *result = n00b_new(n00b_type_utf8(),
                                 n00b_kw("length",
                                        n00b_ka(s->byte_len * 2)));
    char       *map    = upper ? n00b_hex_map_upper : n00b_hex_map_lower;

    char *src = s->data;
    char *dst = result->data;

    for (int i = 0; i < s->byte_len; i++) {
        char c = *src++;
        *dst++ = map[c >> 4];
        *dst++ = map[c & 0x0f];
    }
    *dst = 0;

    result->byte_len   = 2 * s->byte_len;
    result->codepoints = 2 * s->byte_len;

    return result;
}

n00b_utf32_t *
_n00b_str_strip(const n00b_str_t *s, ...)
{
    // TODO: this is needlessly slow for u8 since we convert it to u32
    // twice, both here and in slice.

    bool front = true;
    bool back  = true;

    n00b_karg_only_init(s);
    n00b_kw_bool("front", front);
    n00b_kw_bool("back", back);

    n00b_utf32_t     *as32  = n00b_to_utf32(s);
    n00b_codepoint_t *p     = (n00b_codepoint_t *)as32->data;
    int64_t          start = 0;
    int              len   = n00b_str_codepoint_len(as32);
    int              end   = len;

    if (front) {
        while (start < end && n00b_codepoint_is_space(p[start]))
            start++;
    }

    if (back && (end > start)) {
        while (--end != start) {
            if (!n00b_codepoint_is_space(p[end])) {
                break;
            }
        }
        end++;
    }

    if (!start && end == len) {
        return as32;
    }

    return n00b_str_slice(as32, start, end);
}

n00b_utf32_t *
n00b_str_concat(const n00b_str_t *p1, const n00b_str_t *p2)
{
    n00b_utf32_t *s1     = n00b_to_utf32(p1);
    n00b_utf32_t *s2     = n00b_to_utf32(p2);
    int64_t      s1_len = n00b_str_codepoint_len(s1);
    int64_t      s2_len = n00b_str_codepoint_len(s2);

    if (!s1_len) {
        return s2;
    }

    if (!s2_len) {
        return s1;
    }

    int64_t      n = s1_len + s2_len;
    n00b_utf32_t *r = n00b_new(n00b_type_utf32(),
                             n00b_kw("length", n00b_ka(n)));

    uint64_t num_entries = n00b_style_num_entries(s1);
    num_entries          = num_entries + n00b_style_num_entries(s2);

    if (!s1_len) {
        return s2;
    }

    if (!s2_len) {
        return s1;
    }

    n00b_alloc_styles(r, num_entries);

    int start = n00b_style_num_entries(s1);
    if (start) {
        for (unsigned int i = 0; i < s1->styling->num_entries; i++) {
            r->styling->styles[i] = s1->styling->styles[i];
        }
    }

    if (n00b_style_num_entries(s2)) {
        int start = n00b_style_num_entries(s1);
        for (unsigned int i = 0; i < s2->styling->num_entries; i++) {
            r->styling->styles[i + start] = s2->styling->styles[i];
        }

        // Here, we loop through after the copy to adjust the offsets.
        for (uint64_t i = n00b_style_num_entries(s1); i < num_entries; i++) {
            r->styling->styles[i].start += s1_len;
            r->styling->styles[i].end += s1_len;
        }
    }

    // Now copy the actual string data.
    uint32_t *ptr = (uint32_t *)r->data;
    memcpy(r->data, s1->data, s1_len * 4);
    ptr += s1_len;

    memcpy(ptr, s2->data, s2_len * 4);

    r->codepoints = n;

    return r;
}

static n00b_utf8_t *
n00b_utf8_join(n00b_list_t *l, const n00b_utf8_t *joiner, bool add_trailing)
{
    int64_t     num_items = n00b_list_len(l);
    int64_t     new_len   = 0;
    int         jlen      = 0;
    n00b_list_t *tmplist   = n00b_list(n00b_type_utf8());

    for (int i = 0; i < num_items; i++) {
        n00b_str_t *s = n00b_list_get(l, i, NULL);
        if (!s) {
            continue;
        }
        s = n00b_to_utf8(s);
        new_len += s->byte_len;
        n00b_list_append(tmplist, s);
    }

    if (joiner != NULL) {
        jlen = joiner->byte_len;
        new_len += jlen * (num_items - (add_trailing ? 0 : 1));
    }

    if (new_len <= 0) {
        return n00b_empty_string();
    }

    n00b_utf8_t *result = n00b_new(n00b_type_utf8(),
                                 n00b_kw("length", n00b_ka(new_len)));
    char       *p      = result->data;
    n00b_utf8_t *cur    = n00b_list_get(tmplist, 0, NULL);

    if (cur && cur->byte_len) {
        memcpy(p, cur->data, cur->byte_len);
    }

    for (int i = 1; i < num_items; i++) {
        p += cur->byte_len;

        if (jlen != 0) {
            memcpy(p, joiner->data, jlen);
            p += jlen;
        }

        cur = n00b_list_get(tmplist, i, NULL);
        if (cur && cur->byte_len) {
            memcpy(p, cur->data, cur->byte_len);
        }
    }

    if (add_trailing && jlen != 0) {
        p += cur->byte_len;
        memcpy(p, joiner->data, jlen);
    }

    while (new_len && result->data[new_len - 1] == 0) {
        new_len--;
    }

    result->byte_len      = new_len;
    result->data[new_len] = 0;
    n00b_internal_utf8_set_codepoint_count(result);

    return result;
}

static n00b_utf32_t *
n00b_utf32_join(n00b_list_t *l, const n00b_utf32_t *joiner, bool add_trailing)
{
    int64_t n_parts = n00b_list_len(l);
    int64_t joinlen = joiner ? n00b_str_codepoint_len(joiner) : 0;
    int64_t len     = joinlen * n_parts; // An overestimate when !add_trailing

    for (int i = 0; i < n_parts; i++) {
        n00b_str_t *part = (n00b_str_t *)n00b_list_get(l, i, NULL);
        len += n00b_str_codepoint_len(part);
    }

    n00b_utf32_t     *result = n00b_new(n00b_type_utf32(),
                                  n00b_kw("length", n00b_ka(len)));
    n00b_codepoint_t *p      = (n00b_codepoint_t *)result->data;
    n00b_utf32_t     *j      = joinlen ? n00b_to_utf32(joiner) : NULL;

    result->codepoints = len;

    if (!add_trailing) {
        --n_parts; // skip the last item during the loop.
    }

    for (int i = 0; i < n_parts; i++) {
        n00b_str_t   *v    = (n00b_str_t *)n00b_list_get(l, i, NULL);
        n00b_utf32_t *part = n00b_to_utf32(v);
        int64_t      n_cp = n00b_str_codepoint_len(part);

        if (!n_cp) {
            continue;
        }
        memcpy(p, part->data, n_cp * 4);
        p += n_cp;

        if (joinlen != 0) {
            memcpy(p, j->data, joinlen * 4);
            p += joinlen;
        }
    }

    if (!add_trailing) {
        uint64_t wrong_cp  = result->codepoints;
        result->codepoints = wrong_cp - joinlen;

        n00b_utf32_t *line = n00b_to_utf32((n00b_str_t *)n00b_list_get(l,
                                                                   n_parts,
                                                                   NULL));
        int64_t      n_cp = n00b_str_codepoint_len(line);

        memcpy(p, line->data, n_cp * 4);
    }

    return result;
}

n00b_str_t *
_n00b_str_join(n00b_list_t *l, const n00b_str_t *joiner, ...)
{
    n00b_karg_only_init(joiner);

    bool       add_trailing = false;
    n00b_str_t *result;

    n00b_kw_bool("add_trailing", add_trailing);

    if (joiner && joiner->utf32) {
        result = n00b_utf32_join(l, joiner, add_trailing);
    }
    else {
        result = n00b_utf8_join(l, joiner, add_trailing);
    }

    int64_t n_parts  = n00b_list_len(l);
    int64_t n_styles = 0;
    int64_t joinlen  = joiner ? n00b_str_codepoint_len(joiner) : 0;

    for (int i = 0; i < n_parts; i++) {
        n00b_str_t *part = (n00b_str_t *)n00b_list_get(l, i, NULL);
        n_styles += n00b_style_num_entries(part);
    }

    int txt_ix   = 0;
    int style_ix = 0;

    n00b_alloc_styles(result, n_styles);

    for (int i = 0; i < n_parts; i++) {
        n00b_str_t *part = (n00b_str_t *)n00b_list_get(l, i, NULL);
        int64_t    n_cp = n00b_str_codepoint_len(part);

        if (!n_cp) {
            continue;
        }
        style_ix = n00b_copy_and_offset_styles(part, result, style_ix, txt_ix);
        txt_ix += n_cp;

        if (joinlen != 0) {
            if (i + 1 == n_parts && !add_trailing) {
                break;
            }
            style_ix = n00b_copy_and_offset_styles((n00b_str_t *)joiner,
                                                  result,
                                                  style_ix,
                                                  txt_ix);
            txt_ix += joinlen;
        }
    }

    return result;
}

n00b_utf8_t *
n00b_to_utf8(const n00b_utf32_t *inp)
{
    if (inp == NULL || n00b_str_codepoint_len(inp) == 0) {
        return n00b_empty_string();
    }

    if (!n00b_str_is_u32(inp)) {
        return (n00b_utf8_t *)inp;
    }

    // Allocates 4 bytes per codepoint; this is an overestimate in
    // cases where UTF8 codepoints are above U+00ff. But nbd.

    n00b_utf8_t *res = n00b_new(n00b_type_utf8(),
                              n00b_kw("length", n00b_ka(inp->byte_len + 1)));

    n00b_codepoint_t *p      = (n00b_codepoint_t *)inp->data;
    uint8_t         *outloc = (uint8_t *)res->data;
    int              l;

    res->codepoints = n00b_str_codepoint_len(inp);

    for (int i = 0; i < res->codepoints; i++) {
        l = utf8proc_encode_char(p[i], outloc);
        outloc += l;
    }

    res->byte_len = (int32_t)(outloc - (uint8_t *)res->data);

    n00b_copy_style_info(inp, res);

    return res;
}

n00b_utf32_t *
n00b_to_utf32(const n00b_utf8_t *instr)
{
    if (!instr || instr->byte_len == 0) {
        n00b_utf32_t *outstr = n00b_new(n00b_type_utf32(),
                                      n00b_kw("length", n00b_ka(0)));
        return outstr;
    }

    if (n00b_str_is_u32(instr)) {
        return (n00b_utf32_t *)instr;
    }

    int64_t len = (int64_t)n00b_str_codepoint_len(instr);

    if (!len) {
        return (n00b_utf32_t *)instr;
    }

    n00b_utf32_t     *outstr = n00b_new(n00b_type_utf32(),
                                  n00b_kw("length", n00b_ka(len)));
    uint8_t         *inp    = (uint8_t *)(instr->data);
    n00b_codepoint_t *outp   = (n00b_codepoint_t *)(outstr->data);

    for (int i = 0; i < len; i++) {
        int val = utf8proc_iterate(inp, 4, outp + i);
        if (val < 0) {
            N00B_CRAISE("Invalid utf8 in string when convering to utf32.");
        }
        inp += val;
    }

    outstr->codepoints = len;

    n00b_copy_style_info(instr, outstr);

    return outstr;
}

static void
utf8_init(n00b_utf8_t *s, va_list args)
{
    int64_t     length        = 0; // BYTE length.
    int64_t     start         = 0;
    char       *cstring       = NULL;
    n00b_style_t style         = N00B_STY_BAD;
    bool        replace_style = true;
    char       *tag           = NULL;
    bool        share_cstring = false;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);
    n00b_kw_int64("start", start);
    n00b_kw_ptr("cstring", cstring);
    n00b_kw_uint64("style", style);
    n00b_kw_bool("replace_style", replace_style);
    n00b_kw_ptr("tag", tag);

    if (length == 0 && cstring == NULL) {
        return;
    }

    if (cstring != NULL) {
        if (length <= 0) {
            length = strlen(cstring);
        }

        if (start > length) {
            N00B_CRAISE(
                "Invalid string constructor call: "
                "len(cstring) is less than the start index");
        }

        if (share_cstring == true && ((char *)s->data)[length] == 0) {
            s->data = cstring;
        }
        else {
            s->data = n00b_gc_raw_alloc(length + 1, NULL);
        }
        s->byte_len = length;
        memcpy(s->data, cstring, length);
        s->data[length] = 0;
        n00b_internal_utf8_set_codepoint_count(s);
    }
    else {
        if (length <= 0) {
            s->data = 0;
            N00B_CRAISE("length cannot be < 0 for string initialization");
        }
        s->data = n00b_gc_raw_alloc(length + 1, NULL);
    }

    if (style != N00B_STY_BAD) {
        n00b_str_apply_style(s, style, replace_style);
    }

    if (tag != NULL) {
        n00b_render_style_t *rs = n00b_lookup_cell_style(n00b_new_utf8(tag));
        if (rs != NULL) {
            n00b_str_apply_style(s, rs->base_style, replace_style);
        }
    }
}

static void
utf32_init(n00b_utf32_t *s, va_list args)
{
    int64_t          length        = 0; // NUMBER OF CODEPOINTS.
    int64_t          start         = 0;
    char            *cstring       = NULL;
    n00b_codepoint_t *codepoints    = NULL;
    n00b_style_t      style         = N00B_STY_BAD;
    bool             replace_style = true;
    char            *tag           = NULL;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", length);
    n00b_kw_int64("start", start);
    n00b_kw_ptr("cstring", cstring);
    n00b_kw_uint64("style", style);
    n00b_kw_ptr("codepoints", codepoints);
    n00b_kw_bool("replace_style", replace_style);
    n00b_kw_ptr("tag", tag);

    s->utf32 = 1;

    assert(length >= 0);

    if (length == 0 && cstring == NULL) {
        return;
    }

    if (codepoints != NULL && cstring != NULL) {
        N00B_CRAISE("Cannot specify both 'codepoints' and 'cstring' keywords.");
    }

    if (codepoints != NULL) {
        if (length == 0) {
            N00B_CRAISE(
                "When specifying 'codepoints', must provide a valid "
                "'length' containing the number of codepoints.");
        }
        s->byte_len = length * 4;
        s->data     = n00b_gc_raw_alloc((length + 1) * 4, NULL);

        n00b_codepoint_t *local = (n00b_codepoint_t *)s->data;

        for (int i = 0; i < length; i++) {
            local[i] = codepoints[i];
        }

        s->codepoints = length;
    }
    else {
        if (cstring != NULL) {
            if (length == 0) {
                length = strlen(cstring);
            }

            if (start > length) {
                N00B_CRAISE(
                    "Invalid string constructor call: "
                    "len(cstring) is less than the start index");
            }
            s->byte_len = length * 4;
            s->data     = n00b_gc_raw_alloc((length + 1) * 4, NULL);

            for (int64_t i = 0; i < length; i++) {
                ((uint32_t *)s->data)[i] = (uint32_t)(cstring[i]);
            }
            s->codepoints = length;
        }
        else {
            if (length == 0) {
                N00B_CRAISE(
                    "Must specify a valid length if not initializing "
                    "with a null-terminated cstring.");
            }
            s->byte_len = length * 4;
            s->data     = n00b_gc_raw_alloc((length + 1) * 8, NULL);
        }
    }

    if (style != N00B_STY_BAD) {
        n00b_str_apply_style(s, style, replace_style);
    }

    if (tag != NULL) {
        n00b_render_style_t *rs = n00b_lookup_cell_style(n00b_new_utf8(tag));
        if (rs != NULL) {
            n00b_str_apply_style(s, rs->base_style, replace_style);
        }
    }
}

n00b_utf8_t *
n00b_str_from_int(int64_t n)
{
    char buf[21] = {
        0,
    };
    char *p = &buf[20];

    if (!n) {
        return n00b_utf8_repeat('0', 1);
    }

    int64_t i = n;

    while (i) {
        *--p = '0' + (i % 10);
        i /= 10;
    }

    if (n < 0) {
        *--p = '-';
    }

    return n00b_new(n00b_type_utf8(), n00b_kw("cstring", n00b_ka(p)));
}

// For repeat, we leave an extra alloc'd character to ensure we
// can easily drop in a newline.
n00b_utf8_t *
n00b_utf8_repeat(n00b_codepoint_t cp, int64_t num)
{
    uint8_t buf[4] = {
        0,
    };
    int         buf_ix = 0;
    int         l      = utf8proc_encode_char(cp, &buf[0]);
    int         blen   = l * num;
    n00b_utf8_t *res    = n00b_new(n00b_type_utf8(),
                              n00b_kw("length", n00b_ka(blen + 1)));
    char       *p      = res->data;

    res->codepoints = l;
    res->byte_len   = blen;

    for (int i = 0; i < blen; i++) {
        p[i] = buf[buf_ix++];
        buf_ix %= l;
    }

    return res;
}

n00b_utf32_t *
n00b_utf32_repeat(n00b_codepoint_t cp, int64_t num)
{
    if (num <= 0) {
        return n00b_empty_string();
    }

    n00b_utf32_t     *res = n00b_new(n00b_type_utf32(),
                               n00b_kw("length", n00b_ka(num)));
    n00b_codepoint_t *p   = (n00b_codepoint_t *)res->data;

    res->codepoints = num;

    for (int i = 0; i < num; i++) {
        *p++ = cp;
    }

    return res;
}

int64_t
n00b_str_render_len(const n00b_str_t *s)
{
    int64_t result = 0;
    int64_t n      = n00b_str_codepoint_len(s);

    if (n00b_str_is_u32(s)) {
        n00b_codepoint_t *p = (n00b_codepoint_t *)s->data;

        for (int i = 0; i < n; i++) {
            result += n00b_codepoint_width(p[i]);
        }
    }
    else {
        uint8_t        *p = (uint8_t *)s->data;
        n00b_codepoint_t cp;

        for (int i = 0; i < n; i++) {
            int val = utf8proc_iterate(p, 4, &cp);

            if (val < 0) {
                return result;
            }

            p += val;

            result += n00b_codepoint_width(cp);
        }
    }
    return result;
}

n00b_str_t *
_n00b_str_truncate(const n00b_str_t *s, int64_t len, ...)
{
    n00b_karg_only_init(len);

    bool use_render_width = false;

    n00b_kw_bool("use_render_width", use_render_width);

    int64_t n = n00b_str_codepoint_len(s);
    int64_t c = 0;

    if (n00b_str_is_u32(s)) {
        n00b_codepoint_t *p = (n00b_codepoint_t *)s->data;

        if (use_render_width) {
            for (int i = 0; i < n; i++) {
                int w = n00b_codepoint_width(p[i]);
                if (c + w > len) {
                    return n00b_str_slice(s, 0, i);
                }
            }
            return (n00b_str_t *)s; // Didn't need to truncate.
        }
        if (n <= len) {
            return (n00b_str_t *)s;
        }
        return n00b_str_slice(s, 0, len);
    }
    else {
        uint8_t        *p = (uint8_t *)s->data;
        uint8_t        *next;
        n00b_codepoint_t cp;
        int64_t         num_cp = 0;

        if (use_render_width) {
            for (int i = 0; i < n; i++) {
                int val = utf8proc_iterate(p, 4, &cp);
                if (val < 0) {
                    break;
                }
                next  = p + val;
                int w = n00b_codepoint_width(cp);
                if (c + w > len) {
                    goto u8_slice;
                }
                num_cp++;
                p = next;
            }
            return (n00b_str_t *)s;
        }

        else {
            num_cp = len;

            for (int i = 0; i < n; i++) {
                if (c++ == len) {
u8_slice:
                    // Since we don't have a full u8 slice yet...
                    {
                        uint8_t    *start = (uint8_t *)s->data;
                        int64_t     blen  = p - start;
                        n00b_utf8_t *res   = n00b_new(n00b_type_utf8(),
                                                  n00b_kw("length",
                                                         n00b_ka(blen)));

                        memcpy(res->data, start, blen);
                        n00b_copy_style_info(s, res);

                        if (res->styling != NULL) {
                            for (int i = 0; i < res->styling->num_entries; i++) {
                                n00b_style_entry_t e = res->styling->styles[i];
                                if (e.start > num_cp) {
                                    res->styling->num_entries = i;
                                    return res;
                                }
                                if (e.end > num_cp) {
                                    res->styling->styles[i].end = num_cp;
                                    res->styling->num_entries   = i + 1;
                                    return res;
                                }
                            }
                        }
                        return res;
                    }
                }
                int val = utf8proc_iterate(p, 4, &cp);
                if (val < 0) {
                    break;
                }
                p += val;
            }
            return (n00b_str_t *)s;
        }
    }
}

static bool
u8_starts_with(const n00b_utf8_t *s1, const n00b_utf8_t *s2)
{
    int len = n00b_str_codepoint_len(s2);

    if (len > n00b_str_codepoint_len(s1)) {
        return false;
    }

    for (int i = 0; i < len; i++) {
        if (s1->data[i] != s2->data[i]) {
            return false;
        }
    }

    return true;
}

static bool
u32_starts_with(const n00b_utf32_t *s1, const n00b_utf32_t *s2)
{
    int len = n00b_str_codepoint_len(s2);

    if (len > n00b_str_codepoint_len(s1)) {
        return false;
    }

    if (len == 0) {
        return true;
    }

    n00b_codepoint_t *cp1 = (n00b_codepoint_t *)s1->data;
    n00b_codepoint_t *cp2 = (n00b_codepoint_t *)s2->data;

    for (int i = 0; i < len; i++) {
        if (cp1[i] != cp2[i]) {
            return false;
        }
    }

    return true;
}

bool
n00b_str_starts_with(const n00b_str_t *s1, const n00b_str_t *s2)
{
    if (n00b_str_is_u8(s1)) {
        return u8_starts_with(s1, n00b_to_utf8(s2));
    }

    return u32_starts_with(s1, n00b_to_utf32(s2));
}

bool
n00b_str_ends_with(const n00b_str_t *s1, const n00b_str_t *s2)
{
    int l1 = n00b_str_codepoint_len(s1);
    int l2 = n00b_str_codepoint_len(s2);

    if (l2 > l1) {
        return false;
    }

    n00b_utf32_t *u1 = n00b_to_utf32(s1);
    n00b_utf32_t *u2 = n00b_to_utf32(s2);

    u1 = n00b_str_slice(u1, l1 - l2, l1);

    return n00b_str_eq(u1, u2);
}

n00b_utf8_t *
n00b_from_file(const n00b_str_t *name, int *err)
{
    n00b_utf32_t *n = n00b_to_utf32(name);

    if (!n00b_str_codepoint_len(n)) {
        N00B_CRAISE("Cannot accept an empty string for a file name.");
    }

    // Assumes file is UTF-8.
    //
    // On BSDs, we might add O_EXLOCK. Should do similar on Linux too.
    int fd = open(n->data, O_RDONLY);
    if (fd == -1) {
        *err = errno;
        return NULL;
    }

    off_t len = lseek(fd, 0, SEEK_END);

    if (len == -1) {
err:
        *err = errno;
        close(fd);
        return NULL;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        goto err;
    }

    n00b_utf8_t *result = n00b_new(n00b_type_utf8(), n00b_kw("length", n00b_ka(len)));
    char       *p      = result->data;

    while (1) {
        ssize_t num_read = read(fd, p, len);

        if (num_read == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            goto err;
        }

        if (num_read == len) {
            n00b_internal_utf8_set_codepoint_count(result);
            return result;
        }

        p += num_read;
        len -= num_read;
    }
}

int64_t
_n00b_str_find(n00b_str_t *str, n00b_str_t *sub, ...)
{
    int64_t start = 0;
    int64_t end   = -1;

    n00b_karg_only_init(sub);

    n00b_kw_int64("start", start);
    n00b_kw_int64("end", end);

    str = n00b_to_utf32(str);
    sub = n00b_to_utf32(sub);

    uint64_t strcp = n00b_str_codepoint_len(str);
    uint64_t subcp = n00b_str_codepoint_len(sub);

    if (start < 0) {
        start += strcp;
    }
    if (start < 0) {
        return -1;
    }
    if (end < 0) {
        end += strcp + 1;
    }
    if (end <= start) {
        return -1;
    }
    if ((uint64_t)end > strcp) {
        end = strcp;
    }
    if (subcp == 0) {
        return start;
    }

    uint32_t *strp = (uint32_t *)str->data;
    uint32_t *endp = &strp[end - subcp + 1];
    uint32_t *subp;
    uint32_t *p;

    strp += start;
    while (strp < endp) {
        p    = strp;
        subp = (uint32_t *)sub->data;
        for (uint64_t i = 0; i < subcp; i++) {
            if (*p++ != *subp++) {
                goto next_start;
            }
        }
        return start;
next_start:
        strp++;
        start++;
    }
    return -1;
}

int64_t
_n00b_str_rfind(n00b_str_t *str, n00b_str_t *sub, ...)
{
    int64_t stop  = 0;
    int64_t start = -1;

    n00b_karg_only_init(sub);

    n00b_kw_int64("stop", stop);
    n00b_kw_int64("start", start);

    str = n00b_to_utf32(str);
    sub = n00b_to_utf32(sub);

    uint64_t strcp = n00b_str_codepoint_len(str);
    uint64_t subcp = n00b_str_codepoint_len(sub);

    if (stop < 0) {
        stop += strcp;
    }
    if (stop < 0) {
        stop = 0;
    }
    if (start < 0) {
        start += strcp + 1;
    }

    if (start <= stop) {
        return -1;
    }
    if ((uint64_t)stop > strcp) {
        stop = strcp;
    }

    if (subcp == 0) {
        return start - 1;
    }

    uint32_t *strp   = (uint32_t *)str->data;
    uint32_t *startp = (strp + start) - subcp;
    uint32_t *endp   = strp + stop;
    uint32_t *p;
    uint32_t *subp;

    while (startp >= endp) {
        p    = startp;
        subp = (uint32_t *)sub->data;
        for (uint64_t i = 0; i < subcp; i++) {
            if (*p++ != *subp++) {
                goto next_start;
            }
        }
        return startp - strp;
next_start:
        startp--;
    }
    return -1;
}

flexarray_t *
n00b_str_fsplit(n00b_str_t *str, n00b_str_t *sub)
{
    str            = n00b_to_utf32(str);
    sub            = n00b_to_utf32(sub);
    uint64_t strcp = n00b_str_codepoint_len(str);
    uint64_t subcp = n00b_str_codepoint_len(sub);

    flexarray_t *result = n00b_new(n00b_type_list(n00b_type_utf32()),
                                  n00b_kw("length", n00b_ka(strcp)));

    if (!subcp) {
        for (uint64_t i = 0; i < strcp; i++) {
            flexarray_set(result, i, n00b_str_slice(str, i, i + 1));
        }
        return result;
    }

    int64_t start = 0;
    int64_t ix    = n00b_str_find(str, sub, n00b_kw("start", n00b_ka(start)));
    int     n     = 0;

    while (ix != -1) {
        flexarray_set(result, n++, n00b_str_slice(str, start, ix));
        start = ix + subcp;
        ix    = n00b_str_find(str, sub, n00b_kw("start", n00b_ka(start)));
    }

    if ((uint64_t)start != strcp) {
        flexarray_set(result, n++, n00b_str_slice(str, start, strcp));
    }

    flexarray_shrink(result, n);

    return result;
}

n00b_list_t *
n00b_str_split(n00b_str_t *str, n00b_str_t *sub)
{
    str            = n00b_to_utf32(str);
    sub            = n00b_to_utf32(sub);
    uint64_t strcp = n00b_str_codepoint_len(str);
    uint64_t subcp = n00b_str_codepoint_len(sub);

    n00b_list_t *result = n00b_new(n00b_type_list(n00b_type_utf32()));

    if (!subcp) {
        for (uint64_t i = 0; i < strcp; i++) {
            n00b_list_append(result, n00b_str_slice(str, i, i + 1));
        }
        return result;
    }

    int64_t start = 0;
    int64_t ix    = n00b_str_find(str, sub, n00b_kw("start", n00b_ka(start)));

    while (ix != -1) {
        n00b_list_append(result, n00b_str_slice(str, start, ix));
        start = ix + subcp;
        ix    = n00b_str_find(str, sub, n00b_kw("start", n00b_ka(start)));
    }

    if ((uint64_t)start != strcp) {
        n00b_list_append(result, n00b_str_slice(str, start, strcp));
    }

    return result;
}

n00b_utf8_t *
n00b_cstring(char *s, int64_t len)
{
    return n00b_new(n00b_type_utf8(),
                   n00b_kw("cstring", n00b_ka(s), "length", n00b_ka(len)));
}

n00b_utf8_t *
n00b_rich(n00b_utf8_t *to_copy, n00b_utf8_t *style)
{
    n00b_utf8_t         *res = n00b_str_copy(to_copy);
    n00b_render_style_t *rs  = n00b_lookup_cell_style(n00b_new_utf8(style->data));

    if (rs != NULL) {
        n00b_str_apply_style(res, rs->base_style, 0);
    }

    return res;
}

static n00b_str_t *
n00b_str_repr(n00b_str_t *str)
{
    // TODO: actually implement string quoting.
    n00b_utf32_t *q = n00b_new(n00b_type_utf32(),
                             n00b_kw("cstring", n00b_ka("\"")));

    return n00b_str_concat(n00b_str_concat(q, str), q);
}

static n00b_str_t *
n00b_str_to_str(n00b_str_t *str)
{
    return str;
}

bool
n00b_str_can_coerce_to(n00b_type_t *my_type, n00b_type_t *target_type)
{
    // clang-format off
    if (n00b_types_are_compat(target_type, n00b_type_utf8(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_utf32(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_buffer(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_bool(), NULL)) {
        return true;
    }
    // clang-format on

    return false;
}

n00b_obj_t
n00b_str_coerce_to(const n00b_str_t *s, n00b_type_t *target_type)
{
    if (n00b_types_are_compat(target_type, n00b_type_utf8(), NULL)) {
        return n00b_to_utf8(s);
    }
    if (n00b_types_are_compat(target_type, n00b_type_utf32(), NULL)) {
        return n00b_to_utf32(s);
    }
    if (n00b_types_are_compat(target_type, n00b_type_buffer(), NULL)) {
        // We can't just point into the UTF8 string, since buffers
        // are mutable but strings are not.

        s              = n00b_to_utf8(s);
        n00b_buf_t *res = n00b_new(target_type, s->byte_len);
        memcpy(res->data, s->data, s->byte_len);

        return res;
    }
    if (n00b_types_are_compat(target_type, n00b_type_bool(), NULL)) {
        if (!s || !n00b_str_codepoint_len(s)) {
            return (n00b_obj_t) false;
        }
        else {
            return (n00b_obj_t) true;
        }
    }

    N00B_CRAISE("Invalid coersion.");
}

static n00b_obj_t
n00b_str_lit(n00b_utf8_t          *s_u8,
            n00b_lit_syntax_t     st,
            n00b_utf8_t          *lit_u8,
            n00b_compile_error_t *err)
{
    char *s      = s_u8->data;
    char *litmod = lit_u8->data;

    // clang-format off
    if (!litmod || *litmod == 0 ||
	!strcmp(litmod, "u8") || !strcmp(litmod, "utf8")) {
        return s_u8;
    }
    // clang-format on

    if (!strcmp(litmod, "u32") || !strcmp(litmod, "utf32")) {
        return n00b_to_utf32(s_u8);
    }

    if (!strcmp(litmod, "r") || !strcmp(litmod, "rich")) {
        return n00b_rich_lit(s);
    }

    if (!strcmp(litmod, "url")) {
        if (!n00b_validate_url(s_u8)) {
            *err = n00b_err_malformed_url;
            return NULL;
        }
        return s_u8;
    }

    if (n00b_str_codepoint_len(lit_u8) != 0) {
        *err = n00b_err_parse_no_lit_mod_match;
        return NULL;
    }

    return s_u8;
}

bool
n00b_str_eq(n00b_str_t *s1, n00b_str_t *s2)
{
    if (!s1) {
        if (!s2) {
            return true;
        }
        return false;
    }
    if (!s2) {
        return false;
    }

    bool s1_is_u32 = n00b_str_is_u32(s1);
    bool s2_is_u32 = n00b_str_is_u32(s2);

    if (s1_is_u32 ^ s2_is_u32) {
        if (s1_is_u32) {
            s2 = n00b_to_utf32(s2);
        }
        else {
            s1 = n00b_to_utf32(s1);
        }
    }

    if (s1->byte_len != s2->byte_len) {
        return false;
    }

    return memcmp(s1->data, s2->data, s1->byte_len) == 0;
}

static n00b_str_t *
n00b_string_format(n00b_str_t *obj, n00b_fmt_spec_t *spec)
{
    // For now, just do nothing.
    return obj;
}

n00b_list_t *
n00b_str_wrap(const n00b_str_t *s, int64_t width, int64_t hang)
{
    n00b_list_t *result = n00b_list(n00b_type_utf32());

    if (!s || !n00b_str_codepoint_len(s)) {
        return result;
    }

    n00b_utf32_t *as_u32 = n00b_to_utf32(s);
    int32_t      i;

    if (width <= 0) {
        width = n00b_terminal_width();
        if (!width) {
            width = N00B_MIN_RENDER_WIDTH;
        }
    }

    n00b_break_info_t *line_starts = n00b_wrap_text(as_u32, width, hang);

    for (i = 0; i < line_starts->num_breaks - 1; i++) {
        n00b_utf32_t *slice = n00b_str_slice(as_u32,
                                           line_starts->breaks[i],
                                           line_starts->breaks[i + 1]);
        n00b_list_append(result, slice);

        if (!n00b_str_ends_with(slice, n00b_get_newline_const())) {
            // Add a newline if we wrapped somewhere else.
            n00b_list_append(result, n00b_get_newline_const());
        }
    }

    if (i == line_starts->num_breaks - 1) {
        n00b_utf32_t *slice = n00b_str_slice(as_u32,
                                           line_starts->breaks[i],
                                           n00b_str_codepoint_len(as_u32));
        n00b_list_append(result, slice);
    }

    return result;
}

n00b_utf32_t *
n00b_str_upper(n00b_str_t *s)
{
    n00b_utf32_t *result;
    if (n00b_str_is_u8(s)) {
        result = n00b_to_utf32(s);
    }
    else {
        result = n00b_str_copy(s);
    }

    n00b_codepoint_t *p = (n00b_codepoint_t *)result->data;
    int64_t          n = n00b_str_codepoint_len(s);

    for (int i = 0; i < n; i++) {
        *p = utf8proc_toupper(*p);
        p++;
    }

    result->codepoints = n;
    return n00b_to_utf8(result);
}

n00b_utf32_t *
n00b_str_lower(n00b_str_t *s)
{
    n00b_utf32_t *result;
    if (n00b_str_is_u8(s)) {
        result = n00b_to_utf32(s);
    }
    else {
        result = n00b_str_copy(s);
    }

    n00b_codepoint_t *p = (n00b_codepoint_t *)result->data;
    int64_t          n = n00b_str_codepoint_len(s);

    for (int i = 0; i < n; i++) {
        *p = utf8proc_tolower(*p);
        p++;
    }

    result->codepoints = n;
    return n00b_to_utf8(result);
}

n00b_utf32_t *
n00b_str_title_case(n00b_str_t *s)
{
    n00b_utf32_t *result;
    if (n00b_str_is_u8(s)) {
        result = n00b_to_utf32(s);
    }
    else {
        result = n00b_str_copy(s);
    }

    n00b_codepoint_t *p = (n00b_codepoint_t *)result->data;
    int64_t          n = n00b_str_codepoint_len(s);

    for (int i = 0; i < n; i++) {
        *p = utf8proc_totitle(*p);
        p++;
    }

    result->codepoints = n;
    return n00b_to_utf8(result);
}

n00b_str_t *
n00b_str_pad(n00b_str_t *s, int64_t to_len)
{
    int64_t n = n00b_str_codepoint_len(s);

    if (n >= to_len) {
        return s;
    }

    s = n00b_to_utf32(s);

    n00b_utf32_t     *result = n00b_new(n00b_type_utf32(),
                                  n00b_kw("length", n00b_ka(to_len)));
    n00b_codepoint_t *from   = (n00b_codepoint_t *)s->data;
    n00b_codepoint_t *to     = (n00b_codepoint_t *)result->data;
    int              i;

    for (i = 0; i < n; i++) {
        *to++ = *from++;
    }

    for (; i < to_len; i++) {
        *to++ = ' ';
    }

    result->codepoints = to_len;
    return result;
}

n00b_list_t *
n00b_u8_map(n00b_list_t *inlist)
{
    int len = n00b_list_len(inlist);

    n00b_list_t *result = n00b_list(n00b_type_utf8());

    for (int i = 0; i < len; i++) {
        n00b_list_append(result, n00b_to_utf8(n00b_list_get(inlist, i, NULL)));
    }

    return result;
}

n00b_list_t *
_n00b_c_map(char *s, ...)
{
    n00b_list_t *result = n00b_list(n00b_type_utf8());
    va_list     args;

    va_start(args, s);

    while (s != NULL) {
        n00b_list_append(result, n00b_new_utf8(s));
        s = va_arg(args, char *);
    }

    return result;
}

static inline n00b_type_t *
n00b_str_item_type(n00b_obj_t ignore)
{
    return n00b_type_char();
}

static void *
n00b_str_view(n00b_str_t *s, uint64_t *n)
{
    n00b_utf32_t *as_u32 = n00b_to_utf32(s);
    *n                  = n00b_str_codepoint_len(as_u32);

    return as_u32->data;
}

void
n00b_str_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_str_t *s = (n00b_str_t *)alloc;

    n00b_mark_raw_to_addr(bitfield, alloc, &s->styling);
}

n00b_str_t *
n00b_str_copy(const n00b_str_t *s)
{
    n00b_str_t *res;

    if (s->utf32) {
        res = n00b_new(n00b_type_utf32(),
                      n00b_kw("length",
                             n00b_ka(s->codepoints),
                             "codepoints",
                             n00b_ka(s->data)));
    }
    else {
        res = n00b_new(n00b_type_utf8(),
                      n00b_kw("length",
                             n00b_ka(s->byte_len),
                             "cstring",
                             n00b_ka(s->data)));
    }

    n00b_copy_style_info(s, res);

    return res;
}

const n00b_vtable_t n00b_u8str_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)utf8_init,
        [N00B_BI_REPR]         = (n00b_vtable_entry)n00b_str_repr,
        [N00B_BI_TO_STR]       = (n00b_vtable_entry)n00b_str_to_str,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)n00b_string_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)n00b_str_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)n00b_str_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)n00b_str_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)n00b_str_copy,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_str_concat,
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_str_codepoint_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)n00b_utf8_index,
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)n00b_str_slice,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)n00b_str_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)n00b_str_view,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)n00b_str_set_gc_bits,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `n00b_stream_t` on my mac).
        [N00B_BI_FINALIZER]    = NULL,
    },
};

const n00b_vtable_t n00b_u32str_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)utf32_init,
        [N00B_BI_REPR]         = (n00b_vtable_entry)n00b_str_repr,
        [N00B_BI_TO_STR]       = (n00b_vtable_entry)n00b_str_to_str,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)n00b_string_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)n00b_str_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)n00b_str_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)n00b_str_lit,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_str_concat,
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_str_codepoint_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)n00b_utf32_index,
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)n00b_str_slice,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)n00b_str_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)n00b_str_view,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)n00b_str_set_gc_bits,
        [N00B_BI_FINALIZER]    = NULL,
    },
};
