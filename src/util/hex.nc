#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define MIN_DUMP_WIDTH 36

static int
calculate_size_prefix(uint64_t len, uint64_t start)
{
    // We're going to keep the size prefix even; every 8 bits of max size
    // results in 2 characters printing.

    int log2 = 64 - __builtin_clzll(start + len);
    int ret  = (log2 / 8) * 2;

    if (log2 % 8) {
        ret += 2;
    }
    if (ret == 0) {
        ret = 2;
    }
    return ret;
}

static void
add_offset(char   **optr,
           uint64_t start_offset,
           uint64_t offset_len,
           uint64_t line,
           uint64_t cpl)
{
    /*
    ** To not have to worry much about padding, we're going to add
    ** offset_len zeros and the two spaces. Then, we'll set hex
    ** offset digits from the *right* until the whole offset is written.
    */
    uint8_t  chr;
    char    *p     = *optr;
    uint64_t value = start_offset + (uint64_t)(line * cpl);
    uint64_t ix    = offset_len;
    char     buf[offset_len];

    for (uint64_t i = 0; i < offset_len; i++) {
        buf[i] = '0';
    }

    while (value) {
        chr       = (uint8_t)value & 0x0f;
        value     = value >> 4;
        buf[--ix] = n00b_hex_map_lower[chr];
    }

    for (ix = 0; ix < offset_len; ix++) {
        *p++ = buf[ix];
    }
    *p++ = ' ';
    *p++ = ' ';

    *optr = p;
}

#define ASCIICHAR()                        \
    if (*lineptr < 32 || *lineptr > 126) { \
        *outptr++ = '.';                   \
        lineptr++;                         \
    }                                      \
    else {                                 \
        *outptr++ = *lineptr++;            \
    }

typedef struct {
    int32_t cpl;
    int32_t cols;
    int32_t width;
    int32_t left_margin;
    int32_t ascii_start;
} n00b_hex_fmt_info_t;

static char *
n00b_hexl(void                *ptr,
          int32_t              len,
          uint64_t             start_offset,
          int32_t              width,
          char                *prefix,
          n00b_hex_fmt_info_t *info)
{
    uint64_t offset_len = calculate_size_prefix(len, start_offset);
    uint64_t chars_per_line;
    uint64_t used_width;
    uint64_t num_lines;
    uint64_t alloc_len;
    uint64_t remainder;
    char    *inptr   = (char *)ptr;
    char    *lineptr = inptr;
    char    *outptr;
    char    *ret;
    uint8_t  c;
    size_t   prefix_len = strlen(prefix);

    // Right now, the render width is wider and both are static. But
    // the render width might move to runtime.
    width = n00b_max(n00b_calculate_render_width(width), MIN_DUMP_WIDTH);

    /*
    ** Calculate how many characters we have room to print per line.
    **
    ** Every byte will have its two nibbles printed together, and will
    ** be separated by a space. But, Every FOURTH byte, we add an
    ** extra space. And each byte will have an ascii representation off on
    ** the end. So each byte requires 4.25 spaces.
    **
    ** But we'll only print out groups of 4 bytes, so each group of 4
    ** requires 17 columns.
    **
    ** Additionally, we have the overhead of an extra two spaces
    ** between the offset and the first byte, and we should leave at
    ** least a 1 char margin on the right, so from the width we remove
    ** the `offset_len` we calculated, along w/ 3 more overhead
    ** cols. This will never possibly be more than 19 columns (offset
    ** length of a 64-bit address would be 16 bytes).
    **
    ** This explains the below equation, but also why the minimum
    ** width is 36; the size we need for one group of 4 chars.
    */

    chars_per_line = ((width - offset_len - 3) / 17) * 4;
    used_width     = (chars_per_line / 4) * 17 + offset_len + 2;

    if (info) {
        info->cpl         = chars_per_line;
        info->cols        = chars_per_line / 4;
        info->width       = width + 1; // +1 for the newline.
        info->left_margin = offset_len;
        info->ascii_start = info->cols * 13 + info->left_margin;
    }

    /*
    ** To figure out how many lines we need, we add chars_per_line - 1
    ** to the len, then divide by chars_per_line; this makes sure to
    ** count any short line, but does not overcount if we already are
    ** at an exact multiple.
    */
    num_lines = (len + chars_per_line - 1) / chars_per_line;

    /*
     * We need to keep track of how many leftover characters
     * we have to print on the final line.
     */
    remainder = len % chars_per_line;

    /*
    ** When allocing the result, we need to add another character per
    ** line for the ending newline. Plus, we need to add one more byte
    ** for the null terminator.
    */

    alloc_len = (width + 1) * (num_lines + 1) + prefix_len + 1;
    ret       = n00b_gc_raw_alloc(alloc_len, NULL);

    if (prefix_len) {
        strcpy(ret, prefix);
        outptr = ret + prefix_len;
    }
    else {
        outptr = ret;
    }
    /*
     * Now that we've done our allocation, let's have num_lines
     * represent the number of FULL lines by subtracting one if
     * the remainder is non-zero.
     */
    if (remainder != 0) {
        num_lines -= 1;
    }

    for (uint64_t i = 0; i < num_lines; i++) {
        add_offset(&outptr, start_offset, offset_len, i, chars_per_line);

        // The inner loop is for quads.
        int n = chars_per_line / 4;
        for (int j = 0; j < n; j++) {
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';

            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';

            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';

            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
            *outptr++ = ' ';
        }
        // Now for any ASCII-printable stuff, we emit it, or a '.' if not.
        // lineptr is pointing at the first char we need to show.
        for (uint64_t j = 0; j < chars_per_line; j++) {
            ASCIICHAR();
        }

        for (int64_t j = used_width; j < width; j++) {
            *outptr++ = ' ';
        }

        *outptr++ = '\n';
    }

    if (remainder != 0) {
        // First, print the offset.
        add_offset(&outptr, start_offset, offset_len, num_lines, chars_per_line);

        // Next, we need to know the position where the ASCII
        // representation starts. We've skipped the offset plus pad,
        // but we need to calculate 13 bytes for each group of 4,
        // if this were a full line.
        char *stopptr = outptr + (chars_per_line / 4) * 13;

        // Now, print any full groups of 4.
        for (uint64_t i = 0; i < remainder / 4; i++) {
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
            *outptr++ = ' ';
        }
        // Now, print any leftover chars.
        for (uint64_t i = 0; i < remainder % 4; i++) {
            c         = *inptr++;
            *outptr++ = n00b_hex_map_lower[(c >> 4)];
            *outptr++ = n00b_hex_map_lower[c & 0x0f];
            *outptr++ = ' ';
        }

        // Pad with spaces until we get to where the ASCII bits start.
        while (outptr < stopptr) {
            *outptr++ = ' ';
        }
        for (uint64_t i = 0; i < remainder; i++) {
            ASCIICHAR();
        }
        // Now, pad the rest of the line w/ spaces;
        for (int64_t i = remainder; i < width; i++) {
            *outptr++ = ' ';
        }

        *outptr++ = '\n';
    }

    *outptr = 0;
    return ret;
}

// Line and column when we wrap the original buffer by the number of
// chars per line. Does not directly translate to the new position.
static inline void
line_and_index(int orig_pos, n00b_hex_fmt_info_t *info, int *lptr, int *iptr)
{
    *lptr = orig_pos / info->cpl;
    *iptr = orig_pos % info->cpl;
}

static inline int
new_cstart(int orig_pos, n00b_hex_fmt_info_t *info)
{
    int line;
    int index;
    int col;
    int colpos;

    line_and_index(orig_pos, info, &line, &index);

    col    = index / 4;
    colpos = index % 4;

    int res = line * info->width;

    res += info->left_margin + 2;
    res += col * 13;
    res += colpos * 3;

    return res;
}

static inline int
new_cend(int orig_pos, n00b_hex_fmt_info_t *info)
{
    return new_cstart(orig_pos - 1, info) + 2;
}

static inline void
apply_highlights(n00b_string_t       *s,
                 n00b_list_t         *inarr,
                 n00b_hex_fmt_info_t *info)
{
    int          n = n00b_list_len(inarr);
    n00b_list_t *l = n00b_list(n00b_type_ref());

    for (int i = 0; i < n; i++) {
        n00b_style_record_t *src_style = n00b_list_get(inarr, i, NULL);
        n00b_style_record_t *hex_style;
        // n00b_style_record_t *asc_style;

        int sline;
        int sindex;
        int eline;
        int eindex;

        line_and_index(src_style->start,
                       info,
                       &sline,
                       &sindex);
        line_and_index(src_style->end,
                       info,
                       &eline,
                       &eindex);

        int new_start_pos = new_cstart(src_style->start, info);
        int new_end_pos   = new_cend(src_style->end, info);
        // int ascii_start   = sline * info->width + info->ascii_start + 2;

        if (sline == eline) {
            hex_style        = n00b_gc_alloc_mapped(n00b_style_record_t,
                                             N00B_GC_SCAN_NONE);
            hex_style->start = new_start_pos;
            hex_style->end   = new_end_pos;
            hex_style->info  = src_style->info;
            n00b_list_append(l, hex_style);
            continue;
        }

        // Highlight the rest of first line.
        hex_style        = n00b_gc_alloc_mapped(n00b_style_record_t,
                                         N00B_GC_SCAN_NONE);
        hex_style->start = new_start_pos;
        hex_style->end   = sline * info->width + info->ascii_start;
        hex_style->info  = src_style->info;
        n00b_list_append(l, hex_style);

        // Highlight entire lines.
        while (++sline < eline) {
            hex_style        = n00b_gc_alloc_mapped(n00b_style_record_t,
                                             N00B_GC_SCAN_NONE);
            hex_style->start = sline * info->width + info->left_margin + 2;
            hex_style->end   = sline * info->width + info->ascii_start;
            hex_style->info  = src_style->info;
            n00b_list_append(l, hex_style);
        }

        if (!eindex) {
            continue;
        }
        hex_style        = n00b_gc_alloc_mapped(n00b_style_record_t,
                                         N00B_GC_SCAN_NONE);
        hex_style->start = eline * info->width + info->left_margin + 2;
        hex_style->end   = new_end_pos;
        hex_style->info  = src_style->info;
        n00b_list_append(l, hex_style);
    }

    n = n00b_list_len(l);

    s->styling             = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                    n00b_style_record_t,
                                    n,
                                    N00B_GC_SCAN_ALL);
    s->styling->num_styles = n;

    for (int i = 0; i < n; i++) {
        n00b_style_record_t *e = n00b_list_get(l, i, NULL);
        s->styling->styles[i]  = *e;
    }
}

n00b_string_t *
_n00b_hex_dump(void *ptr, uint32_t len, ...)
{
    keywords
    {
        int64_t      start_offset = 0;
        char        *prefix       = "";
        n00b_list_t *highlights   = NULL;
        int32_t      width        = -1;
    }
    n00b_string_t *res;
    char          *dump;
    // int32_t   addr_width;
    // int32_t   bpl;

    if (highlights) {
        n00b_buf_t         *b = n00b_new(n00b_type_buffer(),
                                 n00b_kw("ptr", ptr, "length", n00b_ka(len)));
        //        n00b_list_t        *hl_styles = n00b_apply_highlights(b, highlights);
        n00b_hex_fmt_info_t info;

        dump = n00b_hexl(b->data, len, start_offset, width, prefix, &info);

        res = n00b_cstring(dump);
        apply_highlights(res, highlights /*hl_styles*/, &info);
    }
    else {
        dump = n00b_hexl(ptr, len, start_offset, width, prefix, NULL);
        res  = n00b_cstring(dump);
    }

    return res;
}

n00b_string_t *
n00b_bytes_to_hex(char *bytes, int n)
{
    n00b_string_t *result = n00b_alloc_utf8_to_copy(n * 2);
    char          *dst    = result->data;
    uint8_t       *src    = (uint8_t *)bytes;

    for (int i = 0; i < n; i++) {
        uint8_t b = *src++;
        *dst++    = n00b_hex_map_lower[b >> 4];
        *dst++    = n00b_hex_map_lower[b & 0xf];
    }

    return result;
}
