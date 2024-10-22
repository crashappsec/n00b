#include "n00b.h"

// This function is a little overly defensive on bounds, since the caller
// does validate the presence of { and }.
static void
parse_one_format_spec(const n00b_utf32_t *s, n00b_fmt_info_t *cur)
{
    int             pos  = cur->start;
    n00b_fmt_spec_t *info = &cur->spec;

    // The actual Python-style format specifier is much easier to
    // parse backward. So once we get to the colon, that is what we
    // will do.

    memset(info, 0, sizeof(n00b_fmt_spec_t));
    int              saved_position;
    int64_t          l = n00b_str_codepoint_len(s);
    n00b_codepoint_t *p = (n00b_codepoint_t *)s->data;
    n00b_codepoint_t  c;

    c = p[pos++];

    // Assumes we've advanced past a leading '{'
    if (c == '}') {
        info->empty = 1;
        return;
    }

    if (c == ':') {
        goto at_colon;
    }

    if (c >= '0' && c <= '9') {
        __uint128_t last = 0;
        __uint128_t n    = 0;
        n                = c - '0';

        while (pos < l) {
            last = n;
            c    = p[pos];

            if (c < '0' || c > '9') {
                break;
            }
            n *= 10;
            n += c - '0';

            if (((int64_t)n) < (int64_t)last) {
                N00B_CRAISE("Integer overflow for argument number.");
            }
        }
        cur->reference.position = (int64_t)n;
        info->kind              = N00B_FMT_NUMBERED;
    }
    else {
        if (!n00b_codepoint_is_n00b_id_start(c)) {
            N00B_CRAISE("Invalid start for format specifier.");
        }
        while (pos < l && n00b_codepoint_is_n00b_id_continue(p[pos])) {
            pos++;
        }
        info->type = N00B_FMT_NAMED;
    }

    if (pos >= l) {
        N00B_CRAISE("Missing } to end format specifier.");
    }
    if (p[pos] == '}') {
        info->empty = 1;
        return;
    }

    if (p[pos++] != ':') {
        N00B_CRAISE("Expected ':' for format info or '}' here");
    }

at_colon:
    saved_position = pos - 1;

    while (true) {
        if (pos == l) {
            N00B_CRAISE("Missing } to end format specifier.");
        }

        c = p[pos];

        if (c == '}') {
            break;
        }

        pos++;
    }

    // Meaning, we got {whatever:}
    if (saved_position + 1 == pos) {
        return;
    }

    c = p[--pos];

    switch (c) {
        //clang-format off
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case ' ':
    case ',':
    case '_':
    case '^':
        break;
    default:
        info->type = p[pos];
        c          = p[--pos];
        // clang-format on
    }

    if (c >= '0' && c <= '9') {
        int         multiplier = 10;
        __uint128_t last       = 0;
        __uint128_t n          = c - '0';

        while (pos != saved_position) {
            c = p[--pos];
            if (c < '0' || c > '9') {
                break;
            }

            last = n;

            n          = n + multiplier * (c - '0');
            multiplier = multiplier * 10;

            if (((int64_t)n) < (int64_t)last) {
                N00B_CRAISE("Integer overflow for argument number.");
            }
        }

        if (c == '.') {
            info->precision = (int64_t)n;
            pos--;
            if (pos == saved_position) {
                return;
            }
        }
        else {
            info->width = (int64_t)n;
            if (pos == saved_position) {
                return;
            }
            goto look_for_sign;
        }
    }

    switch (p[pos]) {
    case '_':
        info->sep = N00B_FMT_SEP_USCORE;
        --pos;
        break;
    case ',':
        info->sep = N00B_FMT_SEP_COMMA;
        --pos;
        break;
    default:
        break;
    }

    if (pos == saved_position) {
        return;
    }

    c = p[pos];

    if (c >= '0' && c <= '9') {
        int         multiplier = 10;
        __uint128_t last       = 0;
        __uint128_t n          = c - '0';

        while (pos != saved_position) {
            c = p[--pos];
            if (c < '0' || c > '9') {
                break;
            }

            last = n;

            n          = n + multiplier * (c - '0');
            multiplier = multiplier * 10;

            if (((int64_t)n) < (int64_t)last) {
                N00B_CRAISE("Integer overflow for argument number.");
            }
        }
        info->width = n;
    }

look_for_sign:
    if (pos == saved_position) {
        return;
    }

    switch (p[pos]) {
    case '+':
        info->sign = N00B_FMT_SIGN_DEFAULT;
        pos--;
        break;
    case '-':
        info->sign = N00B_FMT_SIGN_ALWAYS;
        pos--;
        break;
    case ' ':
        info->sign = N00B_FMT_SIGN_POS_SPACE;
        pos--;
        break;
    default:
        break;
    }

    if (pos == saved_position) {
        return;
    }

    switch (p[pos]) {
    case '<':
        info->align = N00B_FMT_ALIGN_LEFT;
        break;
    case '>':
        info->align = N00B_FMT_ALIGN_RIGHT;
        break;
    case '^':
        info->align = N00B_FMT_ALIGN_CENTER;
        break;
    default:
        N00B_CRAISE("Invalid format specifier.");
    }

    pos--;

    if (pos == saved_position) {
        return;
    }

    info->fill = p[pos--];

    if (pos != saved_position) {
        N00B_CRAISE("Invalid format specifier.");
    }
}

void
n00b_fmt_gc_bits(uint64_t *bitmap, n00b_fmt_info_t *fi)
{
    // If the union is an int, we live dangerously.
    n00b_mark_raw_to_addr(bitmap, fi, &fi->next);
}

static n00b_fmt_info_t *
n00b_extract_format_specifiers(const n00b_str_t *fmt)
{
    n00b_fmt_info_t  *top  = NULL;
    n00b_fmt_info_t  *last = NULL;
    n00b_fmt_info_t  *cur  = NULL;
    n00b_codepoint_t *p;
    int              l;
    int              n;

    fmt = n00b_to_utf32(fmt);
    p   = (n00b_codepoint_t *)fmt->data;
    l   = n00b_str_codepoint_len(fmt);

    for (int i = 0; i < l; i++) {
        switch (p[i]) {
        case '\\':
            i++; // Skip whatever is next;
            continue;
        case '{':
            n = i + 1;
            while (n < l) {
                if (p[n] == '}') {
                    break;
                }
                else {
                    n++;
                }
            }
            if (n == l) {
                N00B_CRAISE("Missing } to end format specifier.");
            }
            cur = n00b_gc_alloc_mapped(n00b_fmt_info_t, n00b_fmt_gc_bits);
            assert(cur);
            cur->start = i + 1;
            cur->end   = n;
            if (last != NULL) {
                last->next = cur;
                last       = cur;
            }
            else {
                top  = cur;
                last = cur;
            }

            parse_one_format_spec(fmt, cur);
        }
    }

    return top;
}

static inline n00b_utf8_t *
apply_padding_and_alignment(n00b_utf8_t *instr, n00b_fmt_spec_t *spec)
{
    int64_t tofill = spec->width - n00b_str_codepoint_len(instr);

    if (tofill <= 0) {
        return instr;
    }

    n00b_utf8_t     *pad;
    n00b_utf8_t     *outstr;
    n00b_codepoint_t cp;
    int             len;

    if (spec->fill != 0) {
        cp = spec->fill;
    }
    else {
        cp = ' ';
    }

    switch (spec->align) {
    case N00B_FMT_ALIGN_CENTER:
        len    = tofill >> 1;
        pad    = n00b_utf8_repeat(cp, len);
        outstr = n00b_str_concat(pad, instr);
        outstr = n00b_str_concat(outstr, pad);
        if (tofill & 1) {
            pad    = n00b_utf8_repeat(cp, 1);
            outstr = n00b_str_concat(outstr, pad);
        }

        return outstr;
    case N00B_FMT_ALIGN_RIGHT:
        pad = n00b_utf8_repeat(cp, tofill);
        return n00b_str_concat(pad, instr);
    default:
        pad = n00b_utf8_repeat(cp, tofill);
        return n00b_str_concat(instr, pad);
    }
}

static inline n00b_list_t *
lookup_arg_strings(n00b_fmt_info_t *specs, n00b_dict_t *args)
{
    n00b_list_t     *result = n00b_list(n00b_type_utf8());
    n00b_fmt_info_t *info   = specs;
    int             i      = 0;
    n00b_utf8_t     *key;
    n00b_obj_t       obj;

    while (info != NULL) {
        switch (info->spec.kind) {
        case N00B_FMT_FMT_ONLY:
            key = n00b_str_from_int(i);
            break;
        case N00B_FMT_NUMBERED:
            key = n00b_str_from_int(info->reference.position);
            break;
        default:
            key = n00b_new(n00b_type_utf8(),
                          n00b_kw("cstring", n00b_ka(info->reference.name)));
            break;
        }

        i++;

        bool found = false;

        obj = hatrack_dict_get(args, key, &found);

        if (!found) {
            n00b_utf8_t *err = n00b_new(
                n00b_type_utf8(),
                n00b_kw("cstring", n00b_ka("Format parameter not found: ")));

            err = n00b_to_utf8(n00b_str_concat(err, key));

            N00B_RAISE(err);
        }

        n00b_vtable_t *vtable = n00b_vtable(obj);
        n00b_format_fn fn     = (n00b_format_fn)vtable->methods[N00B_BI_FORMAT];
        n00b_utf8_t   *s;

        if (fn != NULL) {
            s = n00b_to_utf8(fn(obj, &info->spec));
        }
        else {
            s = n00b_to_utf8(n00b_to_str(obj, n00b_get_my_type(obj)));
        }

        s = apply_padding_and_alignment(s, &info->spec);

        n00b_list_append(result, s);

        info = info->next;
    }

    return result;
}

static inline void
style_adjustment(n00b_utf32_t *s, int64_t start, int64_t offset)
{
    n00b_style_info_t *styles = s->styling;

    if (styles == NULL) {
        return;
    }

    for (int64_t i = 0; i < styles->num_entries; i++) {
        if (styles->styles[i].end <= start) {
            continue;
        }

        if (styles->styles[i].start > start) {
            styles->styles[i].start += offset;
        }

        styles->styles[i].end += offset;
    }
}

static inline n00b_utf8_t *
assemble_formatted_result(const n00b_str_t *fmt, n00b_list_t *arg_strings)
{
    // We need to re-parse the {}'s, and also copy stuff (including
    // style info) into the final string.

    fmt = n00b_to_utf32(fmt);

    int64_t          to_alloc = n00b_str_codepoint_len(fmt);
    int64_t          list_len = n00b_list_len(arg_strings);
    int64_t          arg_ix   = 0;
    int64_t          out_ix   = 0;
    int64_t          alen; // length of one argument being substituted in.
    int64_t          fmt_len = n00b_str_codepoint_len(fmt);
    n00b_codepoint_t *fmtp    = (n00b_codepoint_t *)fmt->data;
    n00b_codepoint_t *outp;
    n00b_codepoint_t *argp;
    n00b_codepoint_t  cp;
    n00b_str_t       *result;
    n00b_utf8_t      *arg;

    for (int64_t i = 0; i < list_len; i++) {
        n00b_str_t *s = (n00b_str_t *)n00b_list_get(arg_strings, i, NULL);
        to_alloc += n00b_str_codepoint_len(s);
    }

    result = n00b_new(n00b_type_utf32(),
                     n00b_kw("length", n00b_ka(to_alloc)));
    outp   = (n00b_codepoint_t *)result->data;

    n00b_copy_style_info(fmt, result);

    for (int64_t i = 0; i < fmt_len; i++) {
        // This is not yet handling hex or unicode etc in format strings.
        switch (fmtp[i]) {
        case '\\':
            style_adjustment(result, out_ix, -1);
            cp = fmtp[++i];
            switch (cp) {
            case 'n':
                outp[out_ix++] = '\n';
                break;
            case 't':
                outp[out_ix++] = '\t';
                break;
            case 'f':
                outp[out_ix++] = '\f';
                break;
            case 'v':
                outp[out_ix++] = '\v';
                break;
            default:
                outp[out_ix++] = cp;
                break;
            }
            continue;
        case '}':
            continue;
        case '{':
            // For now, we will not copy over styles from the format call.
            // Might do that later.
            arg  = n00b_list_get(arg_strings, arg_ix++, NULL);
            alen = n00b_str_codepoint_len(arg);
            arg  = n00b_to_utf32(arg);
            argp = (n00b_codepoint_t *)arg->data;

            style_adjustment(result, out_ix, alen - 2);

            for (int64_t j = 0; j < alen; j++) {
                if (argp[j]) {
                    outp[out_ix++] = argp[j];
                }
            }

            i++; // Skip the } too.
            continue;
        case '\0':
            outp[out_ix] = 0;
            style_adjustment(result, out_ix + 1, -1);
            continue;
        default:
            outp[out_ix++] = fmtp[i];
            continue;
        }
    }

    while (out_ix != 0) {
        if (outp[out_ix - 1] == 0) {
            --out_ix;
        }
        else {
            break;
        }
    }

    result->codepoints = out_ix;

    return n00b_to_utf8(result);
}

n00b_utf8_t *
n00b_str_vformat(const n00b_str_t *fmt, n00b_dict_t *args)
{
    // Positional items are looked up via their ASCII string.
    // Keys are expected to be utf8.
    //
    // Note that the input fmt string is treated as a rich literal,
    // meaning that if there is attached style info it will be 100%
    // stripped.
    n00b_fmt_info_t *info = n00b_extract_format_specifiers(fmt);

    if (info == NULL) {
        return n00b_rich_lit(n00b_to_utf8(fmt)->data);
    }

    // We're going to create a version of the format string where all
    // the parameters are replaced with just {}; we are going to then
    // pass this to n00b_rich_lit to do any formatting we've been asked
    // to do, before we do object substitutions.
    n00b_list_t *segments = n00b_new(n00b_type_list(n00b_type_utf32()));

    int             cur_pos = 0;
    n00b_fmt_info_t *cur_arg = info;

    while (cur_arg != NULL) {
        n00b_utf32_t *s = n00b_str_slice(fmt, cur_pos, cur_arg->start);
        n00b_list_append(segments, s);
        cur_pos = cur_arg->end;
        cur_arg = cur_arg->next;
    }

    if (cur_pos != 0) {
        int l = n00b_str_codepoint_len(fmt);

        if (cur_pos != l) {
            n00b_utf32_t *s = n00b_str_slice(fmt, cur_pos, l);
            n00b_list_append(segments, s);
        }

        fmt = n00b_str_join(segments, n00b_empty_string());
    }

    // After this point, we will potentially have a formatted string,
    // so the locations for the {} may not be where we might compute
    // them to be, so we will just reparse them.
    fmt                     = n00b_rich_lit(n00b_to_utf8(fmt)->data);
    n00b_list_t *arg_strings = lookup_arg_strings(info, args);

    return assemble_formatted_result(fmt, arg_strings);
}

n00b_utf8_t *
n00b_base_format(const n00b_str_t *fmt, int nargs, va_list args)
{
    n00b_obj_t   one;
    n00b_dict_t *dict = n00b_dict(n00b_type_utf8(), n00b_type_ref());

    for (int i = 0; i < nargs; i++) {
        n00b_mem_ptr p;

        p.v = va_arg(args, n00b_obj_t);
        one = n00b_autobox(p.v);

        n00b_utf8_t *key = n00b_str_from_int(i);
        hatrack_dict_add(dict, key, one);
    }
    return n00b_str_vformat(fmt, dict);
}

n00b_utf8_t *
_n00b_str_format(const n00b_str_t *fmt, int nargs, ...)
{
    va_list     args;
    n00b_utf8_t *result;

    va_start(args, nargs);
    result = n00b_base_format(fmt, nargs, args);
    va_end(args);

    return result;
}

n00b_utf8_t *
_n00b_cstr_format(char *fmt, int nargs, ...)
{
    va_list     args;
    n00b_utf8_t *utf8fmt;
    n00b_utf8_t *result;

    va_start(args, nargs);
    utf8fmt = n00b_new_utf8(fmt);
    result  = n00b_base_format((const n00b_str_t *)utf8fmt, nargs, args);
    va_end(args);

    return result;
}

n00b_utf8_t *
n00b_cstr_array_format(char *fmt, int num_args, n00b_utf8_t **params)
{
    n00b_utf8_t *one;
    n00b_dict_t *dict = n00b_dict(n00b_type_utf8(), n00b_type_ref());

    for (int i = 0; i < num_args; i++) {
        one             = params[i];
        n00b_utf8_t *key = n00b_str_from_int(i);
        hatrack_dict_add(dict, key, one);
    }

    return n00b_str_vformat((const n00b_str_t *)n00b_new_utf8(fmt), dict);
}
