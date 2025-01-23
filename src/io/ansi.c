#include "n00b.h"

static inline bool
ignore_for_printing(n00b_codepoint_t cp)
{
    // This prevents things like the terminal ANSI code escape
    // indicator from being printed when processing user data for
    // terminal outputs.  If the user does add in ANSI sequences, the
    // contents of the sequence minus the escape get treated as actual
    // text. This de-fanging seems best, so that the API can fully
    // control rendering as intended.

    switch (cp) {
    case UTF8PROC_CATEGORY_CN:
    case UTF8PROC_CATEGORY_CC:
    case UTF8PROC_CATEGORY_CF:
    case UTF8PROC_CATEGORY_CS:
    case UTF8PROC_CATEGORY_CO:
        if (cp == '\n') {
            return false;
        }
        return true;
    default:
        return false;
    }
}

static inline int
internal_char_render_width(n00b_codepoint_t cp)
{
    if (ignore_for_printing(cp)) {
        return 0;
    }
    return utf8proc_charwidth(cp);
}

static void
ansi_render_style_start(uint64_t info, n00b_stream_t *outstream)
{
    uint64_t remaining = (~N00B_STY_CLEAR_FLAGS) & info;

    if (!info) {
        return;
    }

    n00b_putc(outstream, '\e');
    n00b_putc(outstream, '[');
    if (info & N00B_STY_BOLD) {
        remaining &= ~N00B_STY_BOLD;
        n00b_putc(outstream, '1');
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }
    if (info & N00B_STY_REV) {
        remaining &= ~N00B_STY_REV;
        n00b_putc(outstream, '7');
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }
    if (info & N00B_STY_ST) {
        remaining &= ~N00B_STY_ST;
        n00b_putc(outstream, '9');
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }
    if (info & N00B_STY_ITALIC) {
        remaining &= ~N00B_STY_ITALIC;
        n00b_putc(outstream, '3');
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }
    if (info & N00B_STY_UL) {
        remaining &= ~N00B_STY_UL;
        n00b_putc(outstream, '4');
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }
    if (info & N00B_STY_UUL) {
        remaining &= ~N00B_STY_UUL;
        n00b_puts(outstream, "21");
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }

    if (info & N00B_STY_FG) {
        remaining &= ~N00B_STY_FG;

        uint64_t tmp = info & N00B_STY_FG_BITS;

        if (n00b_use_truecolor()) {
            uint8_t r = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_RED) & 0xff;
            uint8_t g = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_GREEN) & 0xff;
            uint8_t b = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_BLUE) & 0xff;
            n00b_puts(outstream, "38;2;");
            n00b_puti(outstream, r);
            n00b_putc(outstream, ';');
            n00b_puti(outstream, g);
            n00b_putc(outstream, ';');
            n00b_puti(outstream, b);
        }
        else {
            n00b_puts(outstream, "38;5;");
            int32_t color = (int32_t)(tmp & ~(N00B_STY_CLEAR_FG));
            n00b_puti(outstream, n00b_to_vga(color));
        }
        if (remaining) {
            n00b_putc(outstream, ';');
        }
    }

    if (info & N00B_STY_BG) {
        remaining &= ~N00B_STY_BG;

        uint64_t tmp = info & N00B_STY_BG_BITS;

        if (n00b_use_truecolor()) {
            uint8_t r = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_RED) & 0xff;
            uint8_t g = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_GREEN) & 0xff;
            uint8_t b = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_BLUE) & 0xff;
            n00b_puts(outstream, "48;2;");
            n00b_puti(outstream, r);
            n00b_putc(outstream, ';');
            n00b_puti(outstream, g);
            n00b_putc(outstream, ';');
            n00b_puti(outstream, b);
        }
        else {
            n00b_puts(outstream, "38;5;");
            int32_t toand = (int32_t)((uint64_t)N00B_STY_BG_BITS >> N00B_OFFSET_BG_BLUE);
            n00b_puti(outstream, n00b_to_vga(tmp & toand));
        }
    }
    n00b_putc(outstream, 'm');
}

static inline void
ansi_render_style_end(n00b_stream_t *outstream)
{
    n00b_putc(outstream, '\e');
    n00b_putc(outstream, '[');
    n00b_putc(outstream, '0');
    n00b_putc(outstream, 'm');
}

static inline void
ansi_render_style_final(n00b_stream_t *outstream)
{
    n00b_puts(outstream, "\e[0m\e[K");
    // n00b_flush(outstream);
}

static inline n00b_utf8_t *
ansi_get_style_end(void)
{
    return n00b_new_utf8("\e[0m");
}

static inline void
ansi_render_one_codepoint_plain(n00b_codepoint_t cp, n00b_stream_t *outstream)
{
    uint8_t tmp[4];
    int     len;

    if (ignore_for_printing(cp)) {
        return;
    }

    len = utf8proc_encode_char(cp, tmp);
    n00b_stream_write_memory(outstream, (char *)tmp, len);
}

static void
ansi_add_one_codepoint_plain(char **outp, n00b_codepoint_t cp)
{
    if (ignore_for_printing(cp)) {
        return;
    }

    char *tmp = *outp;
    int   len = utf8proc_encode_char(cp, (void *)tmp);
    if (len > 0) {
        tmp += len;
        *outp = tmp;
    }
}

static inline void
ansi_render_one_codepoint_upper(n00b_codepoint_t cp, n00b_stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_toupper(cp), outstream);
}

static inline void
ansi_add_one_codepoint_upper(char **outp, n00b_codepoint_t cp)
{
    ansi_add_one_codepoint_plain(outp, utf8proc_toupper(cp));
}

static inline void
ansi_render_one_codepoint_lower(n00b_codepoint_t cp, n00b_stream_t *outstream)
{
    ansi_render_one_codepoint_plain(utf8proc_tolower(cp), outstream);
}

static inline void
ansi_add_one_codepoint_lower(char **outp, n00b_codepoint_t cp)
{
    ansi_add_one_codepoint_plain(outp, utf8proc_tolower(cp));
}

static inline bool
ansi_render_one_n00b_codepoint_title(n00b_codepoint_t cp, bool go_up, n00b_stream_t *outstream)
{
    if (go_up) {
        ansi_render_one_codepoint_upper(cp, outstream);
        return 0;
    }
    ansi_render_one_codepoint_plain(cp, outstream);
    return n00b_codepoint_is_space(cp);
}

static inline bool
ansi_add_one_codepoint_title(char **outp, n00b_codepoint_t cp, bool go_up)
{
    if (go_up) {
        ansi_add_one_codepoint_upper(outp, cp);
        return false;
    }
    else {
        ansi_add_one_codepoint_plain(outp, cp);
        return n00b_codepoint_is_space(cp);
    }
}

void
n00b_utf8_ansi_render(const n00b_utf8_t *s, n00b_stream_t *outstream)
{
    if (!s) {
        return;
    }

    n00b_style_t        default_style = n00b_get_default_style();
    n00b_style_t        current_style = default_style;
    uint64_t            casing        = current_style & N00B_STY_TITLE;
    int32_t             cp_ix         = 0;
    int32_t             cp_stop       = 0;
    uint32_t            style_ix      = 0;
    n00b_u8_state_t     style_state   = N00B_U8_STATE_START_DEFAULT;
    uint8_t            *p             = (uint8_t *)s->data;
    uint8_t            *end           = p + s->codepoints;
    n00b_style_entry_t *entry         = NULL;
    bool                case_up       = true;
    n00b_codepoint_t    codepoint;

    style_state = N00B_U8_STATE_START_DEFAULT;

    if (s->styling != NULL && ((uint64_t)s->styling != N00B_STY_BAD) && s->styling->num_entries != 0) {
        entry = &s->styling->styles[0];
        if (entry->start == 0) {
            style_state = N00B_U8_STATE_START_STYLE;
        }
        else {
            cp_stop = entry->start;
        }
    }
    else {
        cp_stop = s->codepoints;
    }

    while (p < end) {
        switch (style_state) {
        case N00B_U8_STATE_START_DEFAULT:
            if (current_style != 0) {
                ansi_render_style_end(outstream);
            }

            current_style = default_style;
            casing        = current_style & N00B_STY_TITLE;
            case_up       = true;

            if (entry != NULL) {
                cp_stop = entry->start;
            }
            else {
                cp_stop = s->codepoints;
            }

            ansi_render_style_start(current_style, outstream);

            style_state = N00B_U8_STATE_DEFAULT_STYLE;
            continue;

        case N00B_U8_STATE_START_STYLE:
            current_style = entry->info;
            casing        = current_style & N00B_STY_TITLE;
            cp_stop       = entry->end;
            case_up       = true;

            ansi_render_style_start(current_style, outstream);
            style_state = N00B_U8_STATE_IN_STYLE;
            continue;

        case N00B_U8_STATE_DEFAULT_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                if (entry != NULL) {
                    style_state = N00B_U8_STATE_START_STYLE;
                }
                else {
                    break;
                }
                continue;
            }
            break;

        case N00B_U8_STATE_IN_STYLE:
            if (cp_ix == cp_stop) {
                if (current_style != 0) {
                    ansi_render_style_end(outstream);
                }
                style_ix += 1;
                if (style_ix == s->styling->num_entries) {
                    entry       = NULL;
                    style_state = N00B_U8_STATE_START_DEFAULT;
                }
                else {
                    entry = &s->styling->styles[style_ix];
                    if (cp_ix == entry->start) {
                        style_state = N00B_U8_STATE_START_STYLE;
                    }
                    else {
                        style_state = N00B_U8_STATE_START_DEFAULT;
                    }
                }
                continue;
            }
            break;
        }

        int tmp = utf8proc_iterate(p, 4, &codepoint);
        assert(tmp > 0);
        p += tmp;
        cp_ix += 1;

        switch (casing) {
        case N00B_STY_UPPER:
            ansi_render_one_codepoint_upper(codepoint, outstream);
            break;
        case N00B_STY_LOWER:
            ansi_render_one_codepoint_lower(codepoint, outstream);
            break;
        case N00B_STY_TITLE:
            case_up = ansi_render_one_n00b_codepoint_title(codepoint, case_up, outstream);
            break;
        default:
            ansi_render_one_codepoint_plain(codepoint, outstream);
            break;
        }
    }

    ansi_render_style_final(outstream);
}

// This will have to convert characters to utf-8, since terminals
// generally do not support other encodings.
static void
ansi_render_u32_region(const n00b_utf32_t *s,
                       int32_t             from,
                       int32_t             to,
                       n00b_style_t        style,
                       n00b_stream_t      *outstream)
{
    n00b_codepoint_t *p   = (n00b_codepoint_t *)(s->data);
    bool              cap = true;

    if (style != 0) {
        ansi_render_style_start(style, outstream);
    }

    switch (style & N00B_STY_TITLE) {
    case N00B_STY_UPPER:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_upper(p[i], outstream);
        }
        break;
    case N00B_STY_LOWER:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_lower(p[i], outstream);
        }
        break;
    case N00B_STY_TITLE:
        for (int32_t i = from; i < to; i++) {
            cap = ansi_render_one_n00b_codepoint_title(p[i], cap, outstream);
        }
        break;
    default:
        for (int32_t i = from; i < to; i++) {
            ansi_render_one_codepoint_plain(p[i], outstream);
        }
        break;
    }

    ansi_render_style_final(outstream);
}

#define ansi_puti(var)               \
    if (!var) {                      \
        buf[i++] = '0';              \
    }                                \
    else {                           \
        if (var < 0) {               \
            buf[i++] = '-';          \
            var *= -1;               \
        }                            \
        memset(convert, 0, 21);      \
        p = convert + 20;            \
        while (var != 0) {           \
            *--p = (var % 10) + '0'; \
            var /= 10;               \
        }                            \
        while (p < cend) {           \
            buf[i++] = *p++;         \
        }                            \
    }

#define ansi_puts(str)     \
    p   = str;             \
    end = p + strlen(str); \
    while (p < end) {      \
        buf[i++] = *p++;   \
    }

#define ansi_putc(ch) buf[i++] = ch

#define ansi_sep()      \
    if (remaining) {    \
        ansi_putc(';'); \
    }

static n00b_utf8_t *
ansi_get_style_start(uint64_t info)
{
    uint64_t remaining = (~N00B_STY_CLEAR_FLAGS) & info;
    int32_t  i         = 2;
    char     buf[1024] = {
        '\e',
        '[',
        0,
    };
    char  convert[21];
    char *p;
    char *cend = convert + 20;
    char *end;

    if (info & N00B_STY_BOLD) {
        remaining &= ~N00B_STY_BOLD;
        ansi_putc('1');
        ansi_sep();
    }
    if (info & N00B_STY_REV) {
        remaining &= ~N00B_STY_REV;
        ansi_putc('7');
        ansi_sep();
    }
    if (info & N00B_STY_ST) {
        remaining &= ~N00B_STY_ST;
        ansi_putc('9');
        ansi_sep();
    }
    if (info & N00B_STY_ITALIC) {
        remaining &= ~N00B_STY_ITALIC;
        ansi_putc('3');
        ansi_sep();
    }
    if (info & N00B_STY_UL) {
        remaining &= ~N00B_STY_UL;
        ansi_putc('4');
        ansi_sep();
    }
    if (info & N00B_STY_UUL) {
        remaining &= ~N00B_STY_UUL;
        ansi_puts("21");
        ansi_sep();
    }

    if (info & N00B_STY_FG) {
        remaining &= ~N00B_STY_FG;

        uint64_t tmp = info & N00B_STY_FG_BITS;

        if (n00b_use_truecolor()) {
            uint8_t r = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_RED) & 0xff;
            uint8_t g = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_GREEN) & 0xff;
            uint8_t b = (uint8_t)((tmp & ~N00B_STY_CLEAR_FG) >> N00B_OFFSET_FG_BLUE) & 0xff;
            ansi_puts("38;2;");
            ansi_puti(r);
            ansi_putc(';');
            ansi_puti(g);
            ansi_putc(';');
            ansi_puti(b);
        }
        else {
            ansi_puts("38;5;");
            int32_t color = (int32_t)(tmp & ~(N00B_STY_CLEAR_FG));
            uint8_t c     = n00b_to_vga(color);
            ansi_puti(c);
        }
        ansi_sep();
    }

    if (info & N00B_STY_BG) {
        remaining &= ~N00B_STY_BG;

        uint64_t tmp = info & N00B_STY_BG_BITS;

        if (n00b_use_truecolor()) {
            uint8_t r = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_RED) & 0xff;
            uint8_t g = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_GREEN) & 0xff;
            uint8_t b = (uint8_t)((tmp & ~N00B_STY_CLEAR_BG) >> N00B_OFFSET_BG_BLUE) & 0xff;
            ansi_puts("48;2;");
            ansi_puti(r);
            ansi_putc(';');
            ansi_puti(g);
            ansi_putc(';');
            ansi_puti(b);
        }
        else {
            ansi_puts("38;5;");
            int32_t toand = (int32_t)((uint64_t)N00B_STY_BG_BITS >> N00B_OFFSET_BG_BLUE);
            uint8_t c     = n00b_to_vga(tmp & toand);
            ansi_puti(c);
        }
    }
    ansi_putc('m');

    return n00b_new_utf8(buf);
}

static n00b_utf8_t *
ansify_region(const n00b_utf32_t *s,
              int32_t             from,
              int32_t             to,
              n00b_style_t        style)
{
    n00b_codepoint_t *p        = (n00b_codepoint_t *)(s->data);
    n00b_utf8_t      *result   = NULL;
    bool              cap      = true;
    // Overallocate by a lot.
    int               to_alloc = (to - from) * 16;
    char             *buf      = n00b_gc_raw_alloc(to_alloc, N00B_GC_SCAN_NONE);
    char             *outp     = buf;
    n00b_utf8_t      *tmp;

    if (from == to) {
        return NULL;
    }

    if (style != N00B_UNSTYLED) {
        result = ansi_get_style_start(style);
    }

    switch (style & N00B_STY_TITLE) {
    case N00B_STY_UPPER:
        for (int32_t i = from; i < to; i++) {
            ansi_add_one_codepoint_upper(&outp, p[i]);
        }
        break;
    case N00B_STY_LOWER:
        for (int32_t i = from; i < to; i++) {
            ansi_add_one_codepoint_lower(&outp, p[i]);
        }
        break;
    case N00B_STY_TITLE:
        for (int32_t i = from; i < to; i++) {
            cap = ansi_add_one_codepoint_title(&outp, p[i], cap);
        }
        break;
    default:
        for (int32_t i = from; i < to; i++) {
            ansi_add_one_codepoint_plain(&outp, p[i]);
        }
        break;
    }

    *outp = 0;
    if (outp == buf) {
        return ansi_get_style_end();
    }
    tmp    = n00b_new_utf8(buf);
    result = n00b_str_concat(result, tmp);
    tmp    = ansi_get_style_end();

    return n00b_str_concat(result, tmp);
}

void
n00b_utf32_ansi_render(const n00b_utf32_t *s,
                       int32_t             start_ix,
                       int32_t             end_ix,
                       n00b_stream_t      *outstream)
{
    if (!s) {
        return;
    }

    int32_t      len    = n00b_str_codepoint_len(s);
    n00b_style_t style0 = n00b_get_default_style();

    if (start_ix < 0) {
        start_ix += len;
    }
    if (end_ix <= 0) {
        end_ix += len;
    }

    if (s->styling == NULL || (uint64_t)s->styling == N00B_STY_BAD) {
        ansi_render_u32_region(s, start_ix, end_ix, style0, outstream);
        return;
    }

    int32_t cur        = start_ix;
    int     num_styles = s->styling->num_entries;

    for (int i = 0; i < num_styles; i++) {
        int32_t ss = s->styling->styles[i].start;
        int32_t se = s->styling->styles[i].end;

        if (se <= cur) {
            continue;
        }

        if (ss > cur) {
            ansi_render_u32_region(s, cur, n00b_min(ss, end_ix), style0, outstream);
            cur = ss;
        }

        if (ss <= cur && se >= cur) {
            int32_t stopat = n00b_min(se, end_ix);
            ansi_render_u32_region(s, cur, stopat, s->styling->styles[i].info, outstream);
            cur = stopat;
        }

        if (cur == end_ix) {
            return;
        }
    }

    if (cur != end_ix) {
        ansi_render_u32_region(s, cur, end_ix, style0, outstream);
    }
}

n00b_utf8_t *
n00b_ansify_line(const n00b_utf32_t *s)
{
    int          l     = n00b_str_codepoint_len(s);
    n00b_style_t sbase = n00b_get_default_style();

    if (s->styling == NULL || (uint64_t)s->styling == N00B_STY_BAD) {
        return ansify_region(s, 0, l, sbase);
    }

    int           cur        = 0;
    int           num_styles = s->styling->num_entries;
    n00b_utf32_t *result     = NULL;
    n00b_utf32_t *tmp;

    for (int i = 0; i < num_styles; i++) {
        int32_t ss = s->styling->styles[i].start;
        int32_t se = s->styling->styles[i].end;

        if (se <= cur) {
            continue;
        }

        if (ss > cur) {
            tmp = ansify_region(s, cur, n00b_min(ss, l), sbase);
            cur = ss;
            if (tmp) {
                result = n00b_str_concat(result, tmp);
            }
        }

        if (ss <= cur && se >= cur) {
            int32_t      stopat = n00b_min(se, l);
            n00b_style_t sty    = s->styling->styles[i].info;
            tmp                 = ansify_region(s, cur, stopat, sty);
            if (tmp) {
                result = n00b_str_concat(result, tmp);
            }
            cur = stopat;
        }

        if (cur == l) {
            return result;
        }
    }

    if (cur != l) {
        if (tmp) {
            tmp = ansify_region(s, cur, l, sbase);
        }
        result = n00b_str_concat(result, tmp);
    }

    return n00b_to_utf8(result);
}

void
n00b_ansi_render(const n00b_str_t *s, n00b_stream_t *out)
{
    if (!s) {
        return;
    }

    if (n00b_str_is_u32(s)) {
        n00b_utf32_ansi_render(s, 0, 0, out);
    }
    else {
        n00b_utf8_ansi_render(s, out);
    }
}

static int32_t
internal_truncate_to_width(n00b_utf32_t *s,
                           int32_t       end,
                           int32_t       width)
{
    int i = 0;

    n00b_codepoint_t *cp = (n00b_codepoint_t *)s->data;

    for (i = 0; i < end; i++) {
        width -= n00b_codepoint_width(*cp++);

        if (width < 0) {
            return i;
        }
    }

    return end;
}

void
n00b_ansi_render_to_width(const n00b_str_t *s,
                          int32_t           width,
                          int32_t           hang,
                          n00b_stream_t    *out)
{
    if (!s) {
        return;
    }

    n00b_utf32_t *as_u32 = n00b_to_utf32(s);
    int32_t       i;

    n00b_list_t *lines = n00b_str_split(as_u32, n00b_get_newline_const());
    int          n     = n00b_list_len(lines);

    if (width <= 0) {
        width = N00B_MIN_RENDER_WIDTH;
    }

    for (i = 0; i < n; i++) {
        n00b_utf32_t *line = n00b_list_get(lines, i, NULL);
        int32_t       end  = n00b_str_codepoint_len(line);

        if (end > width) {
            end = internal_truncate_to_width(line, end, width);
        }
        n00b_utf32_ansi_render(line, 0, end, out);
        if (i + 1 == n) {
            break;
        }
        n00b_putcp(out, '\n');
    }
}

typedef struct {
    int32_t       width;
    int32_t       hang;
    n00b_utf32_t *s;
    bool          truncate;
} ansi_filter_ctx;

static void *
n00b_filter_flush_ansi(n00b_stream_t *party, ansi_filter_ctx *ctx, void *ignore)
{
    n00b_str_t *result = ctx->s;
    ctx->s             = NULL;

    return result;
}

static void *
n00b_filter_merge_ansi(n00b_stream_t *party, ansi_filter_ctx *ctx, void *msg)
{
    n00b_type_t  *t            = n00b_get_my_type(msg);
    n00b_utf8_t  *nl           = n00b_get_newline_const();
    bool          partial_line = false;
    n00b_utf32_t *result       = NULL;
    n00b_list_t  *lines;
    n00b_utf32_t *s;
    int           n;

    if (!n00b_type_is_string(t)) {
        // Then it's a synchronous write.

        n00b_list_t *l = n00b_list(n00b_type_ref());
        if (ctx->s) {
            n00b_list_append(l, n00b_filter_flush_ansi(party, ctx, NULL));
            ctx->s = NULL;
        }

        n00b_list_append(l, msg);

        return l;
    }

    if (n00b_str_codepoint_len(msg) == 0) {
        return NULL;
    }

    s = n00b_to_utf32(msg);

    if (ctx->s) {
        s      = n00b_str_concat(ctx->s, s);
        ctx->s = NULL;
    }

    if (!n00b_str_ends_with(s, n00b_get_newline_const())
        && !n00b_str_ends_with(s, n00b_utf32_repeat('\r', 1))) {
        partial_line = true;
    }

    if (!ctx->truncate) {
        lines = n00b_str_wrap(s, ctx->width, ctx->hang);
    }
    else {
        lines = n00b_str_split(s, nl);
    }

    n = n00b_list_len(lines);

    if (partial_line) {
        n      = n - 1;
        ctx->s = n00b_list_get(lines, n, NULL);
    }

    for (int i = 0; i < n; i++) {
        n00b_utf32_t *line = n00b_list_get(lines, i, NULL);
        int32_t       end  = n00b_str_codepoint_len(line);

        if (end > ctx->width && ctx->truncate) {
            internal_truncate_to_width(line, end, ctx->width);
            n00b_utf8_t *line8 = n00b_ansify_line(line);
            if (line8) {
                result = n00b_str_concat(result, line8);
            }
            else {
                result = n00b_str_concat(result, n00b_get_newline_const());
            }
        }
        else {
            n00b_utf8_t *part = n00b_ansify_line(line);

            if (result && part) {
                result = n00b_cstr_format("{}{}", result, part);
            }
            else {
                if (part) {
                    result = part;
                }
            }
        }
    }

    n00b_list_t *l = n00b_list(n00b_type_utf8());

    if (!result) {
        return l;
    }

    n00b_list_append(l, n00b_to_utf8(result));

    return l;
}

n00b_utf8_t *
n00b_ansify(n00b_str_t *s, bool truncate, int hang)
{
    s = n00b_to_utf32(s);

    n00b_list_t  *lines;
    n00b_utf32_t *processed = NULL;
    int           width     = n00b_max(n00b_terminal_width(),
                         N00B_MIN_RENDER_WIDTH);

    if (!n00b_str_ends_with(s, n00b_get_newline_const())) {
        s = n00b_str_concat(s, n00b_get_newline_const());
    }
    if (truncate) {
        lines = n00b_str_split(s, n00b_get_newline_const());
    }
    else {
        lines = n00b_str_wrap(s, width, hang);
    }

    int n = n00b_list_len(lines);

    for (int i = 0; i < n; i++) {
        n00b_utf32_t *line = n00b_list_get(lines, i, NULL);
        int32_t       end  = n00b_str_codepoint_len(line);

        if (truncate && end > width) {
            internal_truncate_to_width(line, end, width);
        }

        processed = n00b_str_concat(processed, n00b_ansify_line(line));
    }

    return n00b_to_utf8(processed);
}

// Note that there's currently no way to resize via API.
void
n00b_merge_ansi(n00b_stream_t *party,
                size_t         width,
                size_t         hang,
                bool           truncate)
{
    ansi_filter_ctx *ctx = n00b_gc_alloc_mapped(ansi_filter_ctx,
                                                N00B_GC_SCAN_ALL);

    if (width <= 0) {
        width = n00b_max(n00b_terminal_width(), N00B_MIN_RENDER_WIDTH);
    }

    ctx->width    = width;
    ctx->hang     = hang;
    ctx->truncate = truncate;

    n00b_stream_filter_t *f = n00b_new_filter((void *)n00b_filter_merge_ansi,
                                              (void *)n00b_filter_flush_ansi,
                                              n00b_new_utf8("ansi"),
                                              sizeof(ansi_filter_ctx));

    ansi_filter_ctx *cookie = f->state;

    cookie->width    = width;
    cookie->hang     = hang;
    cookie->truncate = truncate;

    n00b_add_filter(party, f, false);
}

static inline size_t
internal_render_len_u32(const n00b_utf32_t *s)
{
    n00b_codepoint_t *p     = (n00b_codepoint_t *)s->data;
    int32_t           len   = n00b_str_codepoint_len(s);
    size_t            count = 0;

    for (int i = 0; i < len; i++) {
        count += internal_char_render_width(p[i]);
    }

    return count;
}

static inline size_t
internal_render_len_u8(const n00b_utf8_t *s)
{
    uint8_t         *p   = (uint8_t *)s->data;
    uint8_t         *end = p + s->byte_len;
    n00b_codepoint_t cp;
    size_t           count = 0;

    while (p < end) {
        p += utf8proc_iterate(p, 4, &cp);
        count += internal_char_render_width(cp);
    }

    return count;
}

size_t
n00b_ansi_render_len(const n00b_str_t *s)
{
    if (!s) {
        return 0;
    }

    if (n00b_str_is_u32(s)) {
        return internal_render_len_u32(s);
    }
    else {
        return internal_render_len_u8(s);
    }
}
