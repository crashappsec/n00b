#define N00B_USE_INTERNAL_API
#include "n00b.h"
// We *always* store a u8 representation, and optionally keep a u32
// one, if some API call requires it.

n00b_string_t *n00b_common_string_cache[N00B_STRCACHE_BUILTIN_MAX];

const char n00b_hex_map_lower[16] = "0123456789abcdef";
const char n00b_hex_map_upper[16] = "0123456789ABCDEF";

#define cache_init(x, y) \
    n00b_common_string_cache[N00B_STRCACHE_##x] = y
#define cache_cp_init(x, y) \
    cache_init(x, n00b_string_from_codepoint(y))
#define cache_cstr_init(x, y) \
    cache_init(x, n00b_cstring(y))
void
n00b_init_common_string_cache(void)
{
    if (!n00b_startup_complete) {
        n00b_gc_register_root(n00b_common_string_cache,
                              N00B_STRCACHE_BUILTIN_MAX);
        n00b_string_t *s = n00b_new(n00b_type_string(), NULL, false, 0, NULL);
        // A single null for the empty string constant.
        s->data          = n00b_gc_value_alloc(char);
        cache_init(EMPTY_STR, s);
        cache_cp_init(NEWLINE, '\n');
        cache_cp_init(CR, '\r');
        cache_cp_init(COMMA, ',');
        cache_cp_init(LBRACKET, '[');
        cache_cp_init(RBRACKET, ']');
        cache_cp_init(LBRACE, '{');
        cache_cp_init(RBRACE, '}');
        cache_cp_init(LPAREN, '(');
        cache_cp_init(RPAREN, ')');
        cache_cp_init(BACKTICK, '`');
        cache_cp_init(1QUOTE, '\'');
        cache_cp_init(2QUOTE, '"');
        cache_cp_init(STAR, '*');
        cache_cp_init(SPACE, ' ');
        cache_cp_init(SLASH, '/');
        cache_cp_init(BACKSLASH, '\\');
        cache_cp_init(PERIOD, '.');
        cache_cp_init(COLON, ':');
        cache_cp_init(SEMICOLON, ';');
        cache_cp_init(BANG, '!');
        cache_cp_init(AT, '@');
        cache_cp_init(HASH, '#');
        cache_cp_init(DOLLAR, '$');
        cache_cp_init(PERCENT, '%');
        cache_cp_init(CARAT, '^');
        cache_cp_init(AMPERSAND, '&');
        cache_cp_init(MINUS, '&');
        cache_cp_init(UNDERSCORE, '_');
        cache_cp_init(PLUS, '+');
        cache_cp_init(EQUALS, '=');
        cache_cp_init(PIPE, '=');
        cache_cp_init(GT, '=');
        cache_cp_init(LT, '=');
        cache_cp_init(ESC, '\e');
        cache_cp_init(QUESTION_MARK, '?');
        cache_cp_init(TILDE, '~');
        cache_cstr_init(ARROW, " -> ");
        cache_cstr_init(COMMA_PADDED, ", ");
        cache_cstr_init(COLON_PADDED, " : ");
        cache_cp_init(ELLIPSIS, 0x2026);
    }
}

bool
n00b_valid_codepoint(n00b_codepoint_t cp)
{
    if (cp < 0 || cp > 0x10ffff) {
        return false;
    }
    if (cp >= 0xd800 && cp <= 0xdfff) {
        return false;
    }

    return true;
}

// Returns true if valid, and if so, sets the byte length and # of codepoints.

int
n00b_string_set_codepoint_count(n00b_string_t *s)
{
    char            *p   = s->data;
    char            *end = s->data + s->u8_bytes;
    n00b_codepoint_t cp;
    int              count = 0;

    while (p < end) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        count++;
        n00b_assert(l > 0);
        n00b_assert(cp);
        p += l;
    }

    return count;
}

static inline bool
n00b_cstring_validate_u8(char *s, int *num_cp, int *num_bytes, int max_bytes)
{
    if (!max_bytes) {
        max_bytes = strlen(s);
    }

    int              cp_count = 0;
    char            *p        = s;
    char            *end      = p + max_bytes;
    n00b_codepoint_t cp;

    while (p < end) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (l < 0) {
            return false;
        }
        p += l;
        ++cp_count;
    }

    *num_cp    = cp_count;
    *num_bytes = p - s;

    return true;
}

void
n00b_string_sanity_check(n00b_string_t *s)
{
    int num_cp;
    int num_bytes;

    if (!s) {
        return;
    }

    if (!s->data) {
        s->data = "";
    }

    if (!s->u8_bytes && !s->codepoints) {
        return;
    }

    char *check = s->data;
    for (int i = 0; i < s->u8_bytes; i++) {
        if (!*check++) {
            printf("Null in string.");
            abort();
        }
    }

    if (!n00b_cstring_validate_u8(s->data, &num_cp, &num_bytes, s->u8_bytes)) {
        printf("String validation failed.");
        abort();
    }

    if (num_bytes != s->u8_bytes) {
        printf("Bad byte count.");
        abort();
    }

    if (num_cp != s->codepoints) {
        printf("Bad codepoint count.");
        abort();
    }
}

static inline void
n00b_string_initialize_from_cstring(n00b_string_t *s, char *p)
{
    int num_cp    = 0;
    int num_bytes = 0;

    if (!n00b_cstring_validate_u8(p, &num_cp, &num_bytes, 0)) {
        N00B_CRAISE("Invalid UTF-8 in C string.");
    }

    if (!n00b_in_heap(p)) {
        char *copy = n00b_gc_raw_alloc(num_bytes + 1, N00B_GC_SCAN_NONE);
        memcpy(copy, p, num_bytes);
        p = copy;
    }

    s->data       = p;
    s->codepoints = num_cp;
    s->u8_bytes   = num_bytes;

    n00b_string_sanity_check(s);
}

static inline void
n00b_string_init_from_cstring_with_len(n00b_string_t *s,
                                       char          *p,
                                       int            len)
{
    int num_cp    = 0;
    int num_bytes = 0;

    if (len <= 0) {
        memcpy(s, n00b_cached_empty_string(), sizeof(n00b_string_t));
        return;
    }

    if (!n00b_cstring_validate_u8(p, &num_cp, &num_bytes, len)
        || len != num_bytes) {
        n00b_cstring_validate_u8(p, &num_cp, &num_bytes, len);
        N00B_CRAISE("Invalid UTF-8 in C string.");
    }

    if (!n00b_in_heap(p) || p[len]) {
        char *copy = n00b_gc_raw_alloc(num_bytes + 1, N00B_GC_SCAN_NONE);
        memcpy(copy, p, num_bytes);
        p = copy;
    }

    s->data       = p;
    s->codepoints = num_cp;
    s->u8_bytes   = num_bytes;
}

n00b_codepoint_t *
n00b_string_to_codepoint_array(n00b_string_t *s)
{
    if (s->u32_data) {
        return s->u32_data;
    }

    n00b_codepoint_t *res = n00b_gc_raw_alloc(s->codepoints * 4,
                                              N00B_GC_SCAN_NONE);

    char            *p = s->data;
    int              n = s->codepoints;
    n00b_codepoint_t cp;

    for (int i = 0; i < n; i++) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (l <= 0) {
            N00B_CRAISE("Corrupted UTF-8 in C string.");
        }
        p += l;
        res[i] = cp;
    }

    s->u8_bytes = p - s->data;

    return res;
}

// This assumes you've validated you need it.
static bool
u32_required(n00b_string_t *s)
{
    if (s->u32_data) {
        return true;
    }
    if (s->codepoints == s->u8_bytes) {
        return false;
    }

    s->u32_data = n00b_string_to_codepoint_array(s);
    return true;
}

static inline void
n00b_string_initialize_from_codepoint_array(n00b_string_t    *s,
                                            n00b_codepoint_t *p,
                                            int64_t           len)
{
    n00b_assert(p);
    n00b_assert(len);

    if (!n00b_in_heap(p)) {
        n00b_codepoint_t *p2 = n00b_gc_array_value_alloc(n00b_codepoint_t, len);
        memcpy(p2, p, len * 4);
        p = p2;
    }

    s->u32_data   = p;
    s->codepoints = len;

    // This runs through the string once to figure out how many cp to alloc.
    int u8_len = 0;

    for (int64_t i = 0; i < len; i++) {
        n00b_codepoint_t cp = p[i];

        if (!(cp & 0xffffff80)) {
            u8_len += 1;
            continue;
        }
        if (cp & 0xffff0000) {
            u8_len += 4;
            continue;
        }
        if (cp & 0xfffff800) {
            u8_len += 3;
            continue;
        }
        u8_len += 2;
    }

    s->data     = n00b_gc_array_value_alloc(char, u8_len + 1);
    s->u8_bytes = u8_len;

    char *new = s->data;

    for (int64_t i = 0; i < len; i++) {
        int l = utf8proc_encode_char(p[i], (uint8_t *)new);

        if (l <= 0) {
            N00B_CRAISE("Invalid UTF-32.");
        }
        new += l;
    }
}

static void
n00b_string_init(n00b_string_t *s, va_list args)
{
    void   *p    = va_arg(args, void *);
    bool    cstr = va_arg(args, int);
    int64_t len  = va_arg(args, int64_t);

    if (!len) {
        if (!p) {
            return; // Totally empty.
        }
        n00b_assert(cstr);
        n00b_string_initialize_from_cstring(s, p);
        return;
    }
    if (cstr) {
        if (p) {
            n00b_string_init_from_cstring_with_len(s, p, len);
            return;
        }

        s->data       = n00b_gc_array_value_alloc(char, len + 1);
        s->codepoints = len;
        s->u8_bytes   = len;
        return;
    }

    n00b_string_initialize_from_codepoint_array(s, p, len);
}

static n00b_string_t *
slice_styles(n00b_string_t *dst, n00b_string_t *src, int64_t start, int64_t end)
{
    if (!src->styling) {
        return dst;
    }

    int64_t first = -1;
    int64_t last  = 0;
    int64_t i     = 0;

    for (; i < src->styling->num_styles; i++) {
        int ss = n00b_style_start(src, i);
        int se = n00b_style_end(src, i);

        if (se <= start) {
            continue;
        }
        if (ss >= end) {
            last = i;
            break;
        }
        if (first == -1) {
            first = i;
        }
        last = i + 1;
    }

    if (first == -1) {
        n00b_string_sanity_check(dst);
        return dst;
    }

    dst->styling = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                      n00b_style_record_t,
                                      last - first,
                                      N00B_GC_SCAN_ALL);

    int n = 0;

    for (i = first; i < last; i++) {
        int64_t sold = n00b_style_start(src, i);
        int64_t eold = n00b_style_end(src, i);
        int64_t snew = n00b_max(sold - start, 0);
        int64_t enew = n00b_min(eold, end) - start;

        if (snew >= enew) {
            break;
        }

        if (n00b_style_extends_front(&src->styling->styles[i])) {
            snew = N00B_STYLE_EXTEND;
        }

        if (n00b_style_extends_end(&src->styling->styles[i])) {
            enew = N00B_STYLE_EXTEND;
        }

        dst->styling->styles[n].start = snew;
        dst->styling->styles[n].end   = enew;
        dst->styling->styles[n].info  = src->styling->styles[i].info;
        n++;
    }

    dst->styling->num_styles = n;
    n00b_string_sanity_check(dst);

    return dst;
}

static n00b_string_t *
copy_styles(n00b_string_t *dst, n00b_string_t *src)
{
    if (!src->styling || !src->styling->num_styles) {
        n00b_string_sanity_check(dst);

        return dst;
    }

    dst->styling = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                      n00b_style_record_t,
                                      src->styling->num_styles,
                                      N00B_GC_SCAN_ALL);

    dst->styling->num_styles = src->styling->num_styles;

    for (int i = 0; i < src->styling->num_styles; i++) {
        dst->styling->styles[i].info  = src->styling->styles[i].info;
        dst->styling->styles[i].start = src->styling->styles[i].start;
        dst->styling->styles[i].end   = src->styling->styles[i].end;
        if (i + 1 == src->styling->num_styles) {
            if (src->styling->styles[i].end >= src->codepoints) {
                dst->styling->styles[i].end = dst->codepoints;
            }
        }
    }

    n00b_string_sanity_check(dst);
    return dst;
}

static inline n00b_string_t *
copy_styles_with_offset(n00b_string_t *dst, n00b_string_t *src, int offset)
{
    if (!src->styling || !src->styling->num_styles) {
        n00b_string_sanity_check(dst);
        return dst;
    }

    copy_styles(dst, src);
    int n = src->styling->num_styles;

    // Offset all the copied styles.

    for (int i = 0; i < n; i++) {
        dst->styling->styles[i].start += offset;

        if (!n00b_style_extends_end(&dst->styling->styles[i])) {
            dst->styling->styles[i].end += offset;
        }
    }

    n00b_string_sanity_check(dst);
    return dst;
}

// This Assumes that s1 + s2 resulted in dst, and dst needs its styles set.
static inline n00b_string_t *
concat_styles(n00b_string_t *dst, n00b_string_t *s1, n00b_string_t *s2)
{
    int                  cp1        = n00b_string_codepoint_len(s1);
    int                  n1         = n00b_style_count(s1);
    int                  n2         = n00b_style_count(s2);
    int                  num_styles = n1 + n2;
    int                  x          = 0;
    int                  extend     = false;
    n00b_style_record_t *transition = NULL;

    if (!num_styles) {
        n00b_string_sanity_check(dst);
        return dst;
    }

    dst->styling             = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                      n00b_style_record_t,
                                      num_styles,
                                      N00B_GC_SCAN_ALL);
    dst->styling->num_styles = num_styles;

    for (int i = 0; i < n1; i++) {
        dst->styling->styles[x].info  = s1->styling->styles[i].info;
        dst->styling->styles[x].start = n00b_style_start(s1, i);
        dst->styling->styles[x].end   = n00b_style_end(s1, i);
        if (i + 1 == n1) {
            transition = &dst->styling->styles[x];
            extend     = n00b_style_extends_end(&s1->styling->styles[i]);
        }
        x++;
    }

    for (int i = 0; i < n2; i++) {
        dst->styling->styles[x].info  = s2->styling->styles[i].info;
        dst->styling->styles[x].start = n00b_style_start(s2, i) + cp1;
        dst->styling->styles[x].end   = n00b_style_end(s2, i) + cp1;

        if (!i) {
            bool pre_extend = n00b_style_extends_front(&s2->styling->styles[0]);

            // If we extended and have pre-extend, we do 0, because
            // the transition style would have gotten cp1 above.
            if (extend && !pre_extend) {
                transition->end = dst->styling->styles[x].start;
            }
            if (pre_extend) {
                dst->styling->styles[x].start = transition->end;
            }
        }

        if (i + 1 == n2 && n00b_style_extends_end(&s2->styling->styles[i])) {
            dst->styling->styles[x].end = N00B_STYLE_EXTEND;
        }
        x++;
    }

    n00b_string_sanity_check(dst);
    return dst;
}

n00b_string_t *
n00b_string_slice(n00b_string_t *s, int64_t start, int64_t end)
{
    if (!s || s->codepoints == 0) {
        return slice_styles(n00b_string_empty(), s, 0, 0);
    }

    if (end < 0) {
        end += s->codepoints + 1;
    }
    else {
        if (end > s->codepoints) {
            end = s->codepoints;
        }
    }
    if (start < 0) {
        start += s->codepoints;
    }
    else {
        if (start >= s->codepoints) {
            return slice_styles(n00b_string_empty(), s, start, end);
        }
    }

    if ((start | end) < 0 || start >= end) {
        return slice_styles(n00b_string_empty(), s, start, end);
    }

    int64_t len = end - start;

    if (!u32_required(s)) {
        // First 'true' is c-string; if it's got a number passed after
        // for the length, we know it was pre-checked.
        n00b_string_t *new;
        new = n00b_new(n00b_type_string(), s->data + start, true, len);

        return slice_styles(new, s, start, end);
    }

    n00b_codepoint_t *u32 = &s->u32_data[start];

    n00b_string_t *copy = n00b_new(n00b_type_string(), u32, false, len);
    return slice_styles(copy, s, start, end);
}

n00b_codepoint_t
n00b_string_index(n00b_string_t *s, int64_t n)
{
    if (n < 0) {
        n += s->codepoints;
    }
    if (n < 0) {
        N00B_CRAISE("String is too short for the negative index.");
    }
    if (n >= s->codepoints) {
        N00B_CRAISE("Index out of bounds.");
    }

    if (!u32_required(s)) {
        return s->data[n];
    }

    return s->u32_data[n];
}

n00b_string_t *
n00b_string_to_hex(n00b_string_t *s, bool upper)
{
    n00b_string_t *result = n00b_new(n00b_type_string(),
                                     NULL,
                                     true,
                                     s->u8_bytes * 2);
    char          *map    = (char *)n00b_hex_map_lower;

    if (upper) {
        map = (char *)n00b_hex_map_upper;
    }

    char *src = s->data;
    char *dst = result->data;
    for (int i = 0; i < s->u8_bytes; i++) {
        char c = *src++;
        *dst++ = map[c >> 4];
        *dst++ = map[c & 0x0f];
    }

    return result;
}

n00b_string_t *
_n00b_string_strip(n00b_string_t *s, ...)
{
    bool front = true;
    bool back  = true;

    n00b_karg_only_init(s);
    n00b_kw_bool("front", front);
    n00b_kw_bool("back", back);

    char *start = s->data;
    char *end   = start + s->u8_bytes;

    if (front) {
        while (start < end && n00b_codepoint_is_space(*start)) {
            start++;
        }
    }
    if (back && end > start) {
        while (--end != start) {
            if (!n00b_codepoint_is_space(*end)) {
                break;
            }
        }
        end++;
    }

    if (start == s->data && end == start + s->u8_bytes) {
        return s;
    }

    int            shaved_from_front = start - s->data;
    int            shaved_from_back  = s->data + s->u8_bytes - end;
    int            shaved            = shaved_from_front + shaved_from_back;
    n00b_string_t *result            = n00b_string_empty();

    result->data       = start;
    result->codepoints = s->codepoints - shaved;
    result->u8_bytes   = s->u8_bytes - shaved;

    if (s->u32_data) {
        result->u32_data = s->u32_data + shaved_from_front;
    }

    return slice_styles(result,
                        s,
                        shaved_from_front,
                        s->codepoints - shaved_from_back);
}

n00b_string_t *
n00b_string_concat(n00b_string_t *s1, n00b_string_t *s2)
{
    n00b_string_sanity_check(s1);
    n00b_string_sanity_check(s2);

    if (!s1 || !s1->u8_bytes) {
        return s2;
    }
    if (!s2 || !s2->u8_bytes) {
        return s1;
    }

    n00b_string_t *result = n00b_string_empty();
    result->codepoints    = s1->codepoints + s2->codepoints;
    result->u8_bytes      = s1->u8_bytes + s2->u8_bytes;

    result->data = n00b_gc_array_value_alloc(char, result->u8_bytes + 1);
    memcpy(result->data, s1->data, s1->u8_bytes);
    memcpy(result->data + s1->u8_bytes, s2->data, s2->u8_bytes);

    n00b_string_sanity_check(result);

    return concat_styles(result, s1, s2);
}

n00b_string_t *
_n00b_string_join(n00b_list_t *l, n00b_string_t *joiner, ...)
{
    n00b_karg_only_init(joiner);

    bool           add_trailing = false;
    n00b_string_t *result;
    n00b_string_t *tmp;

    n00b_kw_bool("add_trailing", add_trailing);
    // This would be more efficient overall to alloc one string
    // instead of doing a bunch of concatenations. But since I'm
    // rewriting the string library, and expect it'll take some time
    // to work out subtle bugs, I'm going to revert to the much
    // simpler approach of leveraging concat() and then come back to
    // the 'better' approach when it seems appropriate (probably
    // after grids/tables are re-done and working).

    bool use_joiner = joiner && joiner->codepoints;
    n00b_lock_list_read(l);
    int n = n00b_list_len(l);

    if (!n) {
        n00b_unlock_list(l);
        return n00b_string_empty();
    }

    result = n00b_private_list_get(l, 0, NULL);

    for (int i = 1; i < n; i++) {
        if (use_joiner) {
            result = n00b_string_concat(result, joiner);
        }
        tmp    = n00b_private_list_get(l, i, NULL);
        result = n00b_string_concat(result, tmp);
    }

    n00b_unlock_list(l);

    if (add_trailing) {
        result = n00b_string_concat(result, joiner);
    }

    return result;
}

n00b_string_t *
n00b_string_from_int(int64_t n)
{
    char buf[21] = {
        0,
    };
    char *p = &buf[20];

    if (!n) {
        return n00b_string_repeat('0', 1);
    }

    int64_t i = n;

    while (i) {
        *--p = '0' + (i % 10);
        i /= 10;
    }

    if (n < 0) {
        *--p = '-';
    }

    return n00b_new(n00b_type_string(), p, true, 0);
}

n00b_string_t *
n00b_string_repeat(n00b_codepoint_t cp, int64_t num)
{
    int            u8_len;
    int            alloc_len;
    n00b_string_t *result;
    char           encoded[4];

    u8_len = utf8proc_encode_char(cp, (uint8_t *)&encoded[0]);

    if (u8_len <= 0) {
        N00B_CRAISE("Invalid codepoint.");
    }

    alloc_len = num * u8_len;
    result    = n00b_new(n00b_type_string(), NULL, true, alloc_len);
    char *p   = result->data;

    if (u8_len == 1) {
        for (int i = 0; i < num; i++) {
            *p++ = cp;
        }
    }
    else {
        for (int i = 0; i < num; i++) {
            for (int j = 0; j < u8_len; j++) {
                *p++ = encoded[j];
            }
        }
    }

    result->codepoints = num;

    return result;
}

int64_t
n00b_string_render_len(n00b_string_t *s)
{
    int64_t  result = 0;
    uint8_t *p      = (uint8_t *)s->data;

    if (s->codepoints == s->u8_bytes) {
        for (int i = 0; i < s->u8_bytes; i++) {
            result += n00b_codepoint_width(*p++);
        }

        return result;
    }

    n00b_codepoint_t cp;
    int              n = s->u8_bytes;

    while (n) {
        int l = utf8proc_iterate(p, 4, &cp);

        if (l < 0) {
            N00B_CRAISE("UTF-8 is corrupt.");
        }
        p += l;
        n -= l;
        result += n00b_codepoint_width(cp);
    }

    return result;
}

n00b_string_t *
n00b_string_truncate(n00b_string_t *s, int64_t len)
{
    // This truncates to the *render width.* Use n00b_string_slice to
    // truncate to the codepoint.
    int64_t          n      = s->u8_bytes;
    uint8_t         *p      = (uint8_t *)s->data;
    int64_t          width  = 0;
    int64_t          num_cp = 0;
    n00b_codepoint_t cp;

    while (n) {
        int val = utf8proc_iterate(p, 4, &cp);
        if (val <= 0) {
            N00B_CRAISE("Corrupt UTF-8.");
        }
        n -= val;
        int w = n00b_codepoint_width(cp);
        if (width + w > len) {
            break;
        }
        width += w;
        num_cp++;
    }

    return n00b_string_slice(s, 0, num_cp);
}

bool
n00b_string_starts_with(n00b_string_t *s1, n00b_string_t *s2)
{
    if (s2->codepoints > s1->codepoints) {
        return false;
    }

    int   n = s2->u8_bytes;
    char *p = s1->data;
    char *q = s2->data;

    for (int i = 0; i < n; i++) {
        if (*p++ != *q++) {
            return false;
        }
    }

    return true;
}

bool
n00b_string_ends_with(n00b_string_t *s1, n00b_string_t *s2)
{
    if (s2->codepoints > s1->codepoints) {
        return false;
    }

    int   n = s2->u8_bytes;
    char *p = s1->data + s1->u8_bytes;
    char *q = s2->data + n;

    for (int i = 0; i < n; i++) {
        if (*--p != *--q) {
            return false;
        }
    }

    return true;
}

static n00b_string_t *
count_codepoints(n00b_string_t *s)
{
    int              remaining = s->u8_bytes;
    n00b_codepoint_t cp;
    uint8_t         *p = (uint8_t *)s->data;

    s->codepoints = 0;

    while (remaining) {
        int n = utf8proc_iterate(p, 4, &cp);
        if (n < 0) {
            N00B_CRAISE("Corrupt UTF-8.");
        }
        p += n;
        remaining -= n;
        s->codepoints++;
    }

    return s;
}

bool
n00b_string_from_file(n00b_string_t *name, int *err)
{
    if (!name || !name->codepoints) {
        N00B_CRAISE("Cannot accept an empty string for a file name.");
    }

    // Assumes file is UTF-8.
    //
    // On BSDs, we might add O_EXLOCK. Should do similar on Linux too.
    int fd = open(name->data, O_RDONLY);
    if (fd == -1) {
        *err = errno;
        return NULL;
    }

    off_t len = lseek(fd, 0, SEEK_END);

    if (len == -1) {
        *err = errno;
        n00b_raw_fd_close(fd);
        return NULL;
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        n00b_raise_errno();
    }

    n00b_string_t *result     = n00b_new(n00b_type_string(), NULL, true, len);
    char          *p          = result->data;
    int64_t        total_read = 0;

    while (total_read < len) {
        ssize_t num_read = read(fd, p, len - total_read);

        if (num_read == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            n00b_raw_fd_close(fd);
            *err = errno;
            return NULL;
        }

        p += num_read;
        total_read += num_read;

        if (total_read == len) {
            break;
        }
        n00b_assert(total_read > len);
    }
    return count_codepoints(result);
}

static int64_t
n00b_find_base(n00b_string_t *s, n00b_string_t *sub, int64_t start, int64_t end)
{
    n00b_codepoint_t cp;

    uint64_t strcp = s->codepoints;
    uint64_t subcp = sub->codepoints;

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

    char *next = s->data;
    char *endp;

    if (s->codepoints == s->u8_bytes) {
        next += start;
        endp = s->data + end - subcp + 1;
    }
    else {
        if (start != 0) {
            for (int i = 0; i < start; i++) {
                int n = utf8proc_iterate((uint8_t *)next, 4, &cp);
                if (n < 0) {
                    N00B_CRAISE("Corrupt UTF-8.");
                }
                next += n;
            }
        }
        endp = next;

        for (int i = 0; i < end - start + 1; i++) {
            int n = utf8proc_iterate((uint8_t *)endp, 4, &cp);

            if (n < 0) {
                N00B_CRAISE("Corrupt UTF-8.");
            }
            endp += n;
        }
    }

    int pos = start;

    // This is just very naive for now. There's a lot of literature
    // here, can do much better.
    while (next < endp) {
        if (!memcmp(next, sub->data, sub->u8_bytes)) {
            return pos;
        }
        int l = utf8proc_iterate((uint8_t *)next, 4, &cp);
        n00b_assert(l > 0);
        next += l;
        pos++;
    }

    return -1;
}

int64_t
_n00b_string_find(n00b_string_t *s, n00b_string_t *sub, ...)
{
    int64_t start = 0;
    int64_t end   = -1;

    n00b_karg_only_init(sub);

    n00b_kw_int64("start", start);
    n00b_kw_int64("end", end);

    return n00b_find_base(s, sub, start, end);
}

int64_t
_n00b_string_rfind(n00b_string_t *s, n00b_string_t *sub, ...)
{
    // For now, this is just porting the old utf32-only algorithm.
    int stop  = 0;
    int start = -1;

    n00b_karg_only_init(sub);

    n00b_kw_int64("stop", stop);
    n00b_kw_int64("start", start);

    if (!s->u32_data) {
        s->u32_data = n00b_string_to_codepoint_array(s);
    }
    if (!sub->u32_data) {
        sub->u32_data = n00b_string_to_codepoint_array(sub);
    }

    int strcp = s->codepoints;
    int subcp = sub->codepoints;

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
    if (stop > strcp) {
        stop = strcp;
    }

    if (subcp == 0) {
        return start - 1;
    }

    uint32_t *strp   = (uint32_t *)s->u32_data;
    uint32_t *startp = (strp + start) - subcp;
    uint32_t *endp   = strp + stop;
    uint32_t *p;
    uint32_t *subp;

    while (startp >= endp) {
        p    = startp;
        subp = (uint32_t *)sub->u32_data;
        for (int i = 0; i < subcp; i++) {
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

n00b_list_t *
n00b_string_split(n00b_string_t *str, n00b_string_t *sub)
{
    n00b_list_t *result   = n00b_list(n00b_type_string());
    int          last_end = 0;
    int          str_len  = str->codepoints;
    int          sub_len  = sub->codepoints;
    int64_t      ix;
    bool         done = false;

    if (!sub_len) {
        for (int i = 0; i < str_len; i++) {
            n00b_string_t *s = n00b_string_slice(str, i, i + 1);
            n00b_private_list_append(result, s);
        }
        return result;
    }

    while (!done) {
        ix = n00b_find_base(str, sub, last_end, str_len);
        if (ix == -1) {
            ix   = str_len;
            done = true;
        }

        n00b_string_t *s = n00b_string_slice(str, last_end, ix);
        n00b_private_list_append(result, s);

        last_end = ix + sub_len;

        if (last_end == str_len) {
            // The final substring ended at the end of the parent string,
            // and we need to add an empty string to the end.
            n00b_private_list_append(result, n00b_string_empty());
            done = true;
        }
    }

    return result;
}

n00b_list_t *
_n00b_string_split_words(n00b_string_t *str, ...)
{
    n00b_list_t       *result    = n00b_list(n00b_type_string());
    char              *p         = str->data;
    char              *cur       = p;
    n00b_break_info_t *binfo     = n00b_word_breaks(str);
    int                cur_break = 0;
    n00b_codepoint_t   cp;

    bool spaces      = false;
    bool punctuation = true;

    n00b_karg_only_init(str);
    n00b_kw_bool("spaces", spaces);
    n00b_kw_bool("punctuation", punctuation);

    if (!binfo->num_breaks) {
        return result;
    }

    for (int i = 1; i < binfo->num_breaks; i++) {
        int segment_len = binfo->breaks[i] - cur_break;
        cur_break       = binfo->breaks[i];
        for (int j = 0; j < segment_len; j++) {
            cur += utf8proc_iterate((uint8_t *)cur, 4, &cp);
        }
        n00b_string_t *s = n00b_utf8(p, cur - p);

        if (!s->codepoints) {
            continue;
        }

        if (!spaces || !punctuation) {
            utf8proc_iterate((uint8_t *)s->data, 4, &cp);

            if (!spaces) {
                if (cp == ' ' || cp == '\n' || cp == 'r') {
                    p = cur;
                    continue;
                }

                switch (n00b_codepoint_category(cp)) {
                case UTF8PROC_CATEGORY_ZS:
                case UTF8PROC_CATEGORY_ZL:
                case UTF8PROC_CATEGORY_ZP:
                    p = cur;
                    continue;
                default:
                    break;
                }
            }
            if (!punctuation) {
                if (!n00b_codepoint_is_id_continue(cp)) {
                    p = cur;
                    continue;
                }
            }
        }

        n00b_list_append(result, s);
        p = cur;
    }

    return result;
}

n00b_list_t *
n00b_string_split_and_crop(n00b_string_t *str, n00b_string_t *sub, int64_t w)
{
    n00b_list_t *result   = n00b_list(n00b_type_string());
    int          last_end = 0;
    int          str_len  = str->codepoints;
    int          sub_len  = sub->codepoints;
    int64_t      ix;
    bool         done = false;

    if (!sub->codepoints) {
        for (int i = 0; i < str_len; i++) {
            n00b_string_t *s = n00b_string_slice(str, i, i + 1);
            n00b_list_append(result, n00b_string_truncate(s, w));
        }
        return result;
    }

    while (!done) {
        ix = n00b_find_base(str, sub, last_end, str_len);
        if (ix == -1) {
            ix   = str_len;
            done = true;
        }
        n00b_string_t *s = n00b_string_slice(str, last_end, ix);
        n00b_list_append(result, n00b_string_truncate(s, w));
        last_end += sub_len;

        if (last_end == str_len) {
            // The final substring ended at the end of the parent string,
            // and we need to add an empty string to the end.
            n00b_list_append(result, n00b_string_empty());
            done = true;
        }
    }

    return result;
}

static n00b_string_t *
n00b_string_to_literal(n00b_string_t *str)
{
    n00b_string_t *q = n00b_cached_2quote();

    return n00b_string_concat(n00b_string_concat(q, str), q);
}

static n00b_string_t *
n00b_string_to_str(n00b_string_t *str)
{
    return str;
}

static bool
n00b_string_can_coerce_to(n00b_type_t *my_type, n00b_type_t *target_type)
{
    switch (target_type->base_index) {
    case N00B_T_STRING:
    case N00B_T_BOOL:
    case N00B_T_BUFFER:
    case N00B_T_BYTERING:
        return true;
    default:
        return false;
    }
}
static n00b_obj_t
n00b_string_coerce_to(n00b_string_t *s, n00b_type_t *t)
{
    switch (t->base_index) {
    case N00B_T_STRING:
        return s;
    case N00B_T_BOOL:
        return (n00b_obj_t)(int64_t)(s && s->codepoints);
    case N00B_T_BUFFER:
        return n00b_new(t,
                        n00b_kw("length",
                                n00b_ka(s->u8_bytes),
                                "raw",
                                s->data));
    case N00B_T_BYTERING:
        return n00b_new(t, n00b_kw("string", s));
    default:
        N00B_CRAISE("Invalid coersion.");
    }
}

static n00b_obj_t
n00b_string_lit(n00b_string_t        *s,
                n00b_lit_syntax_t     st,
                n00b_string_t        *litmod,
                n00b_compile_error_t *err)
{
    if (!litmod || litmod->data[0] == 0) {
        return s;
    }

    switch (litmod->data[0]) {
    case 'r':
        if (!strcmp(litmod->data, "rich") || !strcmp(litmod->data, "r")) {
            return n00b_crich(s->data);
        }
        break;
    case 'u':

        if (!strcmp(litmod->data, "u8")) {
            return s;
        }

        if (!strcmp(litmod->data, "url")) {
            if (!n00b_validate_url(s)) {
                *err = n00b_err_malformed_url;
                return NULL;
            }
        }
        break;
    default:
        // TODO / FIXME: hook up the errors.
        return n00b_string_unescape(s, (int *)err);
    }
    *err = n00b_err_parse_no_lit_mod_match;
    return NULL;
}

bool
n00b_string_eq(n00b_string_t *s1, n00b_string_t *s2)
{
    if (s1->u8_bytes != s2->u8_bytes) {
        return false;
    }

    return !memcmp(s1->data, s2->data, s1->u8_bytes);
}

static n00b_string_t *
n00b_string_format(n00b_string_t *obj, n00b_string_t *fmt)
{
    // TODO: Add hex formatting.
    return obj;
}

n00b_list_t *
n00b_string_wrap(n00b_string_t *s, int64_t width, int64_t hang)
{
    n00b_list_t *result = n00b_list(n00b_type_string());
    int          start, end, i = 0;

    if (!s || !s->codepoints) {
        return result;
    }

    width = n00b_calculate_render_width(width);

    n00b_break_info_t *line_starts = n00b_break_wrap(s, width, hang);

    for (; i < line_starts->num_breaks - 1; i++) {
        start                = line_starts->breaks[i];
        end                  = line_starts->breaks[i + 1] - 1;
        n00b_string_t *slice = n00b_string_slice(s, start, end);
        n00b_private_list_append(result, slice);
    }

    if (i == line_starts->num_breaks - 1) {
        start                = line_starts->breaks[i];
        end                  = s->codepoints;
        n00b_string_t *slice = n00b_string_slice(s, start, end);

        // I think this might not be possible, but just in case.
        if (slice->codepoints && slice->data[slice->u8_bytes - 1] == '\n') {
            slice->codepoints--;
            slice->u8_bytes--;
        }
        n00b_private_list_append(result, slice);
    }

    return result;
}

n00b_string_t *
n00b_string_all_caps(n00b_string_t *s)
{
    // This assumes the UTF-8 encoding sizes of all characters are the
    // same. This is my understanding, but if I'm wrong, this will
    // certainly need to change.
    //
    // This uses the upper-case mapping only, and applies it to all
    // characters; string_title_caps converts only the first letter of
    // words, and does so with Unicode title case characters.
    //
    // This also does not yet handle special mappings (e.g., locale).

    n00b_string_t *result;

    result       = n00b_new(n00b_type_string(), NULL, true, s->codepoints);
    uint8_t *src = (uint8_t *)s->data;
    uint8_t *dst = (uint8_t *)result->data;

    n00b_codepoint_t cp;

    for (int i = 0; i < s->codepoints; i++) {
        int dsz = utf8proc_iterate(src, 4, &cp);

        src += dsz;

        if (dsz <= 0) {
            N00B_CRAISE("Corrupt UTF-8.");
        }
        cp = utf8proc_toupper(cp);

        dst += utf8proc_encode_char(cp, dst);
    }

    return copy_styles(result, s);
}

n00b_string_t *
n00b_string_lower(n00b_string_t *s)
{
    // Same note as above on not handling mappings.
    n00b_string_t *result;

    result       = n00b_new(n00b_type_string(), NULL, true, s->codepoints);
    uint8_t *src = (uint8_t *)s->data;
    uint8_t *dst = (uint8_t *)result->data;

    n00b_codepoint_t cp;

    for (int i = 0; i < s->codepoints; i++) {
        int dsz = utf8proc_iterate(src, 4, &cp);

        src += dsz;

        if (dsz <= 0) {
            N00B_CRAISE("Corrupt UTF-8.");
        }
        cp = utf8proc_tolower(cp);

        dst += utf8proc_encode_char(cp, dst);
    }

    return copy_styles(result, s);
}

n00b_codepoint_t
n00b_codepoint_title_case(n00b_codepoint_t cp, bool *prev_cased)
{
    bool             cased = *prev_cased;
    n00b_codepoint_t result;

    if (cased) {
        result = utf8proc_tolower(cp);
    }
    else {
        result = utf8proc_totitle(cp);
    }
    switch (utf8proc_category(cp)) {
    case UTF8PROC_CATEGORY_LU:
    case UTF8PROC_CATEGORY_LL:
    case UTF8PROC_CATEGORY_LT:
    case UTF8PROC_CATEGORY_LM:
    case UTF8PROC_CATEGORY_LO:
        *prev_cased = true;
        break;
    default:
        *prev_cased = false;
        break;
    }

    return result;
}

n00b_string_t *
n00b_string_upper(n00b_string_t *s)
{
    bool           prev_was_cased = false;
    n00b_string_t *result         = n00b_new(n00b_type_string(),
                                     NULL,
                                     true,
                                     s->codepoints);
    uint8_t       *src            = (uint8_t *)s->data;
    uint8_t       *dst            = (uint8_t *)result->data;

    n00b_codepoint_t cp;

    for (int i = 0; i < s->codepoints; i++) {
        int dsz = utf8proc_iterate(src, 4, &cp);

        src += dsz;

        if (dsz <= 0) {
            N00B_CRAISE("Corrupt UTF-8.");
        }

        if (prev_was_cased) {
            cp = utf8proc_tolower(cp);
        }
        else {
            cp = utf8proc_totitle(cp);
        }

        switch (utf8proc_category(cp)) {
        case UTF8PROC_CATEGORY_LU:
        case UTF8PROC_CATEGORY_LL:
        case UTF8PROC_CATEGORY_LT:
        case UTF8PROC_CATEGORY_LM:
        case UTF8PROC_CATEGORY_LO:
            prev_was_cased = true;
            break;
        default:
            prev_was_cased = false;
            break;
        }

        dst += utf8proc_encode_char(cp, dst);
    }

    return copy_styles(result, s);
}

n00b_string_t *
n00b_string_pad(n00b_string_t *s, int64_t to_len)
{
    // This RIGHT-pads strings.

    int diff = to_len - s->codepoints;

    if (diff <= 0) {
        return s;
    }

    int len = s->codepoints + diff;

    n00b_string_t *result = n00b_new(n00b_type_string(),
                                     NULL,
                                     true,
                                     len);
    memcpy(result->data, s->data, s->u8_bytes);
    for (int i = s->u8_bytes; i < len; i++) {
        result->data[i] = ' ';
    }

    copy_styles(result, s);

    if (result->styling && result->styling->num_styles) {
        int n = result->styling->num_styles - 1;

        // If the style goes to the end of the original string,
        // extend it out.
        if (result->styling->styles[n].end == s->codepoints) {
            result->styling->styles[n].end = len;
        }
    }

    return result;
}

static void *
n00b_string_view(n00b_string_t *s, uint64_t *n)
{
    // TODO: consider returning an array of styled 1 char strings.
    *n = s->codepoints;
    return n00b_string_to_codepoint_array(s);
}

n00b_string_t *
n00b_string_reuse_text(n00b_string_t *s)
{
    // Copies the string data by reference, since the bytes are fully
    // immutable.
    //
    // This does NOT copy styles.  n00b_string_copy() does.

    n00b_string_t *result = n00b_new(n00b_type_string(), NULL, false, 0);
    result->data          = s->data;
    result->u32_data      = s->u32_data;
    result->codepoints    = s->codepoints;
    result->u8_bytes      = s->u8_bytes;

    return result;
}

#ifdef N00B_DESIRED_BUT_BROKEN
n00b_string_t *
n00b_string_copy(n00b_string_t *s)
{
    return copy_styles(n00b_string_reuse_text(s), s);
}
#else
n00b_string_t *
n00b_string_copy(n00b_string_t *s)
{
    return copy_styles(n00b_utf8(s->data, s->u8_bytes), s);
}
#endif

n00b_string_t *
n00b_string_align_left(n00b_string_t *s, int w)
{
    int l = n00b_string_render_len(s);

    if (l >= w) {
        return s;
    }

    int            diff   = w - l;
    int            n      = s->u8_bytes + diff;
    n00b_string_t *result = n00b_new(n00b_type_string(), NULL, true, n);

    result->codepoints = s->codepoints + diff;

    memcpy(result->data, s->data, s->u8_bytes);
    char *p = result->data + s->u8_bytes;

    for (int i = 0; i < diff; i++) {
        *p++ = ' ';
    }

    return copy_styles(result, s);
}

n00b_string_t *
n00b_string_align_right(n00b_string_t *s, int w)
{
    int l = n00b_string_render_len(s);

    if (l >= w) {
        return s;
    }

    int            diff   = w - l;
    int            n      = s->u8_bytes + diff;
    n00b_string_t *result = n00b_new(n00b_type_string(), NULL, true, n);

    result->codepoints = s->codepoints + diff;

    char *p = result->data;

    for (int i = 0; i < diff; i++) {
        *p++ = ' ';
    }

    memcpy(p, s->data, s->u8_bytes);

    return copy_styles_with_offset(result, s, diff);
}

n00b_string_t *
n00b_string_align_center(n00b_string_t *s, int w)
{
    int l = n00b_string_render_len(s);

    if (l >= w) {
        return s;
    }
    int            diff     = w - l;
    int            n        = s->u8_bytes + diff;
    int            lpad_len = diff / 2;
    int            rpad_len = lpad_len + diff % 2;
    n00b_string_t *result   = n00b_new(n00b_type_string(), NULL, true, n);

    result->codepoints = s->codepoints + diff;

    char *p = result->data;

    for (int i = 0; i < lpad_len; i++) {
        *p++ = ' ';
    }

    memcpy(p, s->data, s->u8_bytes);

    p += s->u8_bytes;

    for (int i = 0; i < rpad_len; i++) {
        *p++ = ' ';
    }

    return copy_styles_with_offset(result, s, lpad_len);
}

char *
n00b_string_to_cstr(n00b_string_t *s)
{
    char *result = n00b_gc_array_value_alloc(char, s->u8_bytes + 1);
    memcpy(result, s->data, s->u8_bytes);

    return result;
}

static inline char *
n00b_copy_buffer_contents(n00b_buf_t *b)
{
    defer_on();
    n00b_buffer_acquire_r(b);
    char *result = n00b_gc_raw_alloc(b->byte_len + 1, N00B_GC_SCAN_ALL);

    memcpy(result, b->data, b->byte_len);

    Return result;
    defer_func_end();
}

char *
n00b_to_cstr(void *v)
{
    n00b_type_t *t = n00b_get_my_type(v);

    if (n00b_type_is_buffer(t)) {
        return n00b_copy_buffer_contents(v);
    }
    else {
        if (!n00b_type_is_string(t)) {
            N00B_CRAISE("Expected a string or a buffer.");
        }

        return n00b_string_to_cstr(v);
    }
}

char **
n00b_make_cstr_array(n00b_list_t *l, int *lenp)
{
    n00b_lock_list(l);

    int    len    = n00b_list_len(l);
    char **result = n00b_gc_array_alloc(char *, len + 1);

    for (int i = 0; i < len; i++) {
        void *v   = n00b_private_list_get(l, i, NULL);
        result[i] = n00b_to_cstr(v);
    }

    result[len] = NULL;

    n00b_unlock_list(l);

    if (lenp) {
        *lenp = len;
    }

    n00b_assert(n00b_in_heap(result));
    return result;
}

n00b_string_t *
n00b_to_literal_provided_type(void *item, n00b_type_t *t)
{
    uint64_t                x = t->base_index;
    n00b_string_convertor_t p;

    p = (void *)n00b_base_type_info[x].vtable->methods[N00B_BI_TO_LITERAL];

    if (!p) {
        return NULL;
    }

    return (*p)(item);
}

n00b_string_t *
n00b_to_string_provided_type(void *item, n00b_type_t *t)
{
    uint64_t                x = t->base_index;
    n00b_string_convertor_t p;

    // TODO: Remove this after adding the method.
    if (x == N00B_T_STRING) {
        return item;
    }

    p = (void *)n00b_base_type_info[x].vtable->methods[N00B_BI_TO_STRING];

    if (!p) {
        return n00b_to_literal_provided_type(item, t);
    }

    return (*p)(item);
}

n00b_string_t *
n00b_to_string(void *item)
{
    if (!n00b_in_heap(item)) {
        return n00b_to_string_provided_type(item, n00b_type_i64());
    }

    n00b_type_t *t = n00b_get_my_type(item);

    if (!t) {
        return n00b_to_string_provided_type(item, n00b_type_i64());
    }

    return n00b_to_string_provided_type(item, t);
}

n00b_string_t *
n00b_to_literal(void *item)
{
    if (!n00b_in_heap(item)) {
        return NULL;
    }

    n00b_type_t *t = n00b_get_my_type(item);

    if (!t) {
        return NULL;
    }

    return n00b_to_literal_provided_type(item, t);
}

static inline n00b_type_t *
n00b_string_item_type(n00b_obj_t ignore)
{
    return n00b_type_char();
}

n00b_string_t *
n00b_cstring_copy(char *s)
{
    int n = strlen(s);

    n00b_string_t *result = n00b_alloc_utf8_to_copy(n);
    memcpy(result->data, s, n);

    return result;
}

// For now, I'm storing the masked byte so that we can avoid the
// ugliness of the full array of 256 entries; the default 0'd out
// entires will give 0xff when XOR'd when 0xff.

const uint8_t n00b_inverse_hex[256] = {
    ['0'] = 0xff,
    ['1'] = 0xfe,
    ['2'] = 0xfd,
    ['3'] = 0xfc,
    ['4'] = 0xfb,
    ['5'] = 0xfa,
    ['6'] = 0xf9,
    ['7'] = 0xf8,
    ['8'] = 0xf7,
    ['9'] = 0xf6,
    ['a'] = 0xf5,
    ['b'] = 0xf4,
    ['c'] = 0xf3,
    ['d'] = 0xf2,
    ['e'] = 0xf1,
    ['f'] = 0xf0,
    ['A'] = 0xf5,
    ['B'] = 0xf4,
    ['C'] = 0xf3,
    ['D'] = 0xf2,
    ['E'] = 0xf1,
    ['F'] = 0xf0,
};

static inline uint8_t
one_hex_digit(uint8_t c)
{
    return n00b_inverse_hex[c] ^ 0xff;
}

static inline bool
should_escape(n00b_codepoint_t cp)
{
    if (!n00b_codepoint_is_printable(cp)) {
        return true;
    }
    switch (cp) {
    case '"':
    case '\\':
    case '\'':
        return true;
    default:
        return false;
    }
}

n00b_string_t *
n00b_string_escape(n00b_string_t *s)
{
    // We don't know up-front how many codepoints we'll need to escape,
    // so we'll take two passes through the source string.
    // We will just go ahead and allocate (very conservatively) 8 extra
    // bytes per escape needed.

    n00b_codepoint_t cp;
    uint8_t         *p           = (uint8_t *)s->data;
    uint8_t         *end         = (uint8_t *)s->data + s->u8_bytes;
    int              num_escapes = 0;

    while (p < end) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (l <= 0) {
            N00B_CRAISE("Invalid UTF-8 string");
        }
        p += l;
        if (should_escape(cp)) {
            num_escapes++;
        }
    }

    if (!num_escapes) {
        return s;
    }

    p = (uint8_t *)s->data;

    int            num_cp = 0;
    int            alen   = s->u8_bytes + 8 * num_escapes;
    n00b_string_t *result = n00b_alloc_utf8_to_copy(alen);
    uint8_t       *q      = (uint8_t *)result->data;

    while (p < end) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (!should_escape(cp)) {
            num_cp++;
            for (int i = 0; i < l; i++) {
                *q++ = *p++;
            }
        }
        else {
            p += l;
            *q++ = '\\';
            num_cp += 2; // Assume 2; add more as needed.

            switch (cp) {
            case '\\':
                *q++ = '\\';
                continue;
            case '\a':
                *q++ = 'a';
                continue;
            case '\b':
                *q++ = 'b';
                continue;
            case '\e':
                *q++ = 'e';
                continue;
            case '\f':
                *q++ = 'f';
                continue;
            case '\n':
                *q++ = 'n';
                continue;
            case '\r':
                *q++ = 'r';
                continue;
            case '\t':
                *q++ = 't';
                continue;
            case '\v':
                *q++ = 'v';
                continue;
            default:
                num_cp += 2;
                if (cp <= 0x7f) {
                    *q++ = 'x';
                    *q++ = n00b_hex_map_lower[cp >> 4];
                    *q++ = n00b_hex_map_lower[cp & 0x0f];
                    continue;
                }
                num_cp += 2;
                if (cp < 0x10000) {
                    *q++ = 'u';
                    *q++ = n00b_hex_map_lower[cp >> 12];
                    *q++ = n00b_hex_map_lower[(cp >> 8) & 0x0f];
                    *q++ = n00b_hex_map_lower[(cp >> 4) & 0x0f];
                    *q++ = n00b_hex_map_lower[cp & 0x0f];
                    continue;
                }
                num_cp += 2;
                *q++ = 'U';
                *q++ = n00b_hex_map_lower[cp >> 20];
                *q++ = n00b_hex_map_lower[(cp >> 16) & 0x0f];
                *q++ = n00b_hex_map_lower[(cp >> 12) & 0x0f];
                *q++ = n00b_hex_map_lower[(cp >> 8) & 0x0f];
                *q++ = n00b_hex_map_lower[(cp >> 4) & 0x0f];
                *q++ = n00b_hex_map_lower[cp & 0x0f];
                continue;
            }
        }
    }

    result->u8_bytes   = ((char *)q) - result->data;
    result->codepoints = num_cp;

    return result;
}

n00b_string_t *
n00b_string_unescape(n00b_string_t *s, int *error)
{
    n00b_compile_error_t internal_errno = n00b_err_no_error;
    uint8_t             *p              = (uint8_t *)s->data;
    uint8_t             *end            = (uint8_t *)p + s->u8_bytes;
    n00b_codepoint_t     cp;

    if (*(end)) {
        if (n00b_string_find(s, n00b_string_from_codepoint('\\')) == -1) {
            return s;
        }
    }
    else {
        if (!index((char *)p, '\\')) {
            return s;
        }
    }

    n00b_string_t *res   = n00b_alloc_utf8_to_copy(s->u8_bytes);
    uint8_t       *q     = (uint8_t *)res->data;
    int            count = 0;

    while (p < end) {
        int l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        int n;

        count++;
        if (l <= 0) {
            return NULL;
        }
        if (cp != '\\') {
            while (l--) {
                *q++ = *p++;
            }
            continue;
        }
        p++;
        l = utf8proc_iterate((uint8_t *)p, 4, &cp);
        if (l <= 0) {
            return NULL;
        }
        switch (cp) {
        case 'a':
            *q++ = '\a';
            p++;
            continue;
        case 'b':
            *q++ = '\b';
            p++;
            continue;
        case 'e':
            *q++ = '\e';
            p++;
            continue;
        case 'f':
            *q++ = '\f';
            p++;
            continue;
        case 'n':
            *q++ = '\n';
            p++;
            continue;
        case 'r':
            *q++ = '\r';
            p++;
            continue;
        case 't':
            *q++ = '\t';
            p++;
            continue;
        case 'v':
            *q++ = '\v';
            p++;
            continue;
        case 'u':
            p++;
            if (*p == '+') {
                p++;
            }
            if (end - p < 4) {
                internal_errno = n00b_err_hex_eos;
                goto err;
            }
            cp = one_hex_digit(*p++);
            if (cp == 0xff) {
                internal_errno = n00b_err_hex_missing;
                goto err;
            }
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_u;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_u;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_u;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            q += utf8proc_encode_char(cp, q);
            continue;
        case 'U':
            p++;
            if (*p == '+') {
                p++;
            }
            if (end - p < 6) {
                internal_errno = n00b_err_hex_eos;
                goto err;
            }
            cp = one_hex_digit(*p++);
            if (cp == 0xff) {
                internal_errno = n00b_err_hex_missing;
                goto err;
            }
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_U;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_U;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_U;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_U;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_U;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            q += utf8proc_encode_char(cp, q);
            continue;
        case 'x':
        case 'X':
            p++;
            if (end - p < 2) {
                internal_errno = n00b_err_hex_eos;
                goto err;
            }
            cp = one_hex_digit(*p++);
            if (cp == 0xff) {
                internal_errno = n00b_err_hex_missing;
                goto err;
            }
            n = one_hex_digit(*p++);
            if (n == 0xff) {
                internal_errno = n00b_err_hex_x;
                goto err;
            }
            cp <<= 4;
            cp |= n;
            q += utf8proc_encode_char(cp, q);
            continue;

        default:
            while (l--) {
                *q++ = *p++;
            }
            continue;
        }
    }

    *q = 0;

    res->codepoints = count;
    res->u8_bytes   = ((char *)q) - res->data;

    return res;

err:
    if (error) {
        *error = (int)internal_errno;
        return NULL;
    }
    N00B_CRAISE("Improper escape sequence in string.");
}

const n00b_vtable_t n00b_string_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)n00b_string_init,
        [N00B_BI_TO_LITERAL]   = (n00b_vtable_entry)n00b_string_to_literal,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)n00b_string_to_str,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)n00b_string_format,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)n00b_string_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)n00b_string_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)n00b_string_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)n00b_string_copy,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_string_concat,
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_string_codepoint_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)n00b_string_index,
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)n00b_string_slice,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)n00b_string_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)n00b_string_view,
        [N00B_BI_RENDER]       = NULL,
    },
};
