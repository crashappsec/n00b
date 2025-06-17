#define N00B_USE_INTERNAL_API
#include "n00b.h"

enum {
    PREF_DEFAULT,
    PREF_TO_STRING,
    PREF_LITERAL,
};

enum {
    BOOL_TF,
    BOOL_TF_FULL,
    BOOL_TF_UPPER,
    BOOL_TF_FULL_UPPER,
    BOOL_YN,
    BOOL_YN_FULL,
    BOOL_YN_UPPER,
    BOOL_YN_FULL_UPPER,
};

typedef enum {
    RICH_STYLE_ON,      // Turns on a style.
    RICH_STYLE_OFF,     // Specific style is off; properties apply.
    RICH_PROP_ON,       // Turns on a property.
    RICH_PROP_OFF,      // Named property is being turned off.
    RICH_STYLE_RESET,   // [/]
    RICH_PAYLOAD,       // A substitution argument w/ a payload.
    RICH_ORIGINAL_TEXT, // Lexed as text.
    RICH_AS_TEXT,       // A control field demoted to text.
    RICH_STYLE_TBD,
} rich_ctl_type;

typedef enum {
    RICH_NONPROP = 0,
    RICH_B       = 1,
    RICH_I       = 2,
    RICH_U       = 4,
    RICH_UU      = 8,
    RICH_REV     = 16,
    RICH_ST      = 32,
    RICH_UPPER   = 64,
    RICH_LOWER   = 128,
    RICH_ALLCAPS = 256,
} rich_prop_t;

typedef union {
    n00b_text_element_t *style;
    rich_prop_t          prop;
    n00b_string_t       *repr;
} rich_tsi;

typedef struct {
    int           start_cp;
    int           end_cp;
    char         *start;
    char         *end;
    rich_tsi      tsi;
    rich_ctl_type type;
} rich_ctrl_t;

static inline void
add_span(n00b_break_info_t **binfo, int s, int cur, bool zero_ok)
{
    if (s == cur && !zero_ok) {
        return;
    }

    n00b_break_add(binfo, s);
    n00b_break_add(binfo, cur);
}

static inline void
advance(uint8_t **p, n00b_codepoint_t *cp, int *count)
{
    int l = utf8proc_iterate(*p, 4, cp);
    n00b_assert(l > 0);
    *p     = *p + l;
    *count = *count + 1;
}

static inline void
lookahead(uint8_t *p, n00b_codepoint_t *cp)
{
    int l = utf8proc_iterate(p, 4, cp);
    n00b_assert(l > 0);
}

// This function breaks up the input string into two and only two
// token types, 'text' sections, and 'control' sections. The 'text'
// sections are non-zero lengthed pieces of text outside of a control
// section.
//
// Control sections are returned without the start and end tag
// delimeter.
//
// Currently, we allow [|tag|] and «tag»
//
// This algorithm is designed to have very simple parsing rules, and
// to always successfully tokenize no matter what. Basically:
//
// 1. A backslash always escapes.
// 2. Start and end delimeters must be of a matching kind.
// 3. There's no nesting of delimeters allowed anywhere at all,.
// 4. Matches are always as short as possible.
// 5. Everything up until the matching end delimeter is part of the tag,
//    whatever it happens to be. The tag itself will be contextually parsed.
//
// Any delimiter that isn't part of a tag is text.

static inline void
rich_raw_tokenize(n00b_string_t      *s,
                  n00b_break_info_t **text,
                  n00b_break_info_t **ctrl)
{
    int              i;
    int              next_i     = 0;
    int              next_start = 0;
    uint8_t         *p          = (uint8_t *)s->data;
    uint8_t         *end        = (uint8_t *)s->data + s->u8_bytes;
    n00b_codepoint_t cp;
    n00b_codepoint_t end_start        = 0;
    int              mono_start_index = -1;
    int              bi_start_index   = -1;

    *text = n00b_break_alloc(s, 4);
    *ctrl = n00b_break_alloc(s, 4);

    while (p < end) {
        i = next_i;

        advance(&p, &cp, &next_i);

        if (cp == '\\' && p < end) {
            advance(&p, &cp, &next_i);
            continue;
        }

        switch (cp) {
        case 0x00ab:
            mono_start_index = next_i;
            break;
        case '[':
            lookahead(p, &cp);
            if (cp == '=' || cp == '|') {
                end_start = cp;
                advance(&p, &cp, &next_i);
                bi_start_index = next_i;
            }
            break;
        case 0x00bb:
            if (mono_start_index != -1) {
                add_span(text, next_start, mono_start_index - 1, false);
                add_span(ctrl, mono_start_index, i, true);
                mono_start_index = -1;
                bi_start_index   = -1;
                next_start       = next_i;
            }
            break;
        case '=':
        case '|':
            if (cp != end_start) {
                break;
            }
            if (bi_start_index != -1) {
                lookahead(p, &cp);
                if (cp == ']') {
                    advance(&p, &cp, &next_i);
                    add_span(text, next_start, bi_start_index - 2, false);
                    add_span(ctrl, bi_start_index, i, true);
                    mono_start_index = -1;
                    bi_start_index   = -1;
                    next_start       = next_i;
                }
                break;
            }
        default:
            break;
        }
    }

    if (next_start != next_i) {
        add_span(text, next_start, next_i, false);
    }
}

// Rich tags consist of one of the following:

// 1) Any valid, registered text tag, as long as the tag does not
//    start with a slash, tilde, hash mark or digit.
// 2) A slash, followed by a single valid text tag. (closes the tag)
// 3) A slash by itself (resets styling to the default)
// 4) A hash mark, optionally followed argument number, indicating a
//    text substitution from the ordered list of
//    parameters. Importantly, all parameters 64 bits in size.
//
//    Arg #s are zero-indexed.
//
//    If a mix of arg #s and omitted #s are specified, the omitted
//    numbers are treated the same as if no arg #s were
//    specified. That means, omitted numbers are assigned numbers
//    left-to-right, always counting from 0.
//
// 5) Anything else gets treated as if it was not supposed to be a tag
//    at all (as literal text). This is also true if the specification in
//    option #4 is not valid for *any* reason.
//
// Tags in category 1 generally immediately replace the current
// styling with the passed style, unless it is one of the following:
//
// - b or bold; layers bold onto the existing style.
// - i or italic; layers italics onto the existing style.
// - r or reverse; layers reverse onto the existing style.
// - u or underline; layers underline onto the existing style (potentially
//       turning off double-underline)
// - uu; layers double-underline onto the existing style (potentially turning
//       off single underline).
//
// Individual fg / bg colors can't get set directly via rich, unless
// they've been registered as tags, in which case they're treated as
// STYLES. Ideally, you should not use color names for styles. While
// themes do support instances of common colors, this is meant for
// applications that have reasonable fixed ties to colors, like
// traffic light colors.
//
// Tags in category #4 can also be followed by a colon; anything to
// the right of the colon is considered a 'specifier'. The specifier's
// behavior is based on the first letter after the colon; the rest of
// the contents are context-dependent.
//
// Any time context-dependent arguments are not understood, if the
// associated object has a format function, whole argument gets
// passed. If that formatting routine does not accept the argument,
// then the entire tag gets treated as text.
//
// Built-in specifiers only take arguments if explicitly mentioned.
//
// a) If there is no corresponding argument passed, then the entire
// lhs and rhs are considered text, along with the delimeters.
//
// b) If there is no colon, or nothing to the right of the colon, then
//    the *default* representation for an object is selected:
//
//    - If the value is not in the heap, or if the value is in the
//      heap, but does not point to the start of an object, it is
//      formatted as if one explicitly requested pointer formatting
//      via 'p' (see below).
//
//    - If the value is an object in the heap, we attempt to get a
//      string representation, first by calling the object's `format`
//      method, then trying it's `to_string` method, then trying its
//      `to_literal` method.
//
//      If none of those methods are defined, we format it as if 'o'
//      was specified.
//
// c) '!' uses the default representation, but strips it of any found
//    formatting.
//
// d) 'p' indicates that the value should be treated as a
//        pointer. It's printed with a leading @, along with a full 64
//        bit value in hex, where the hex digits are lower-cased.
//
//    'P' works like 'p', but the hex is upper case.
//
// e) 'o' object-formats. That means this prints type@addr, where the
//    addr is formatted like 'p'. If the value is not an object, the
//    part before the `@` will be one of, 'alloc', 'offset',
//    'non-heap', 'header', or 'invalid'.
//
//    'alloc' indicates the start of a user addressable heap
//    allocation record (i.e., the part returned by
//    n00b_gc_alloc...()).
//
//    'header' indicates the value points to the start of an
//    allocation header.
//
//    'offset' means the pointer points into a valid allocation, but
//    does not point to either the user-returned start, or the
//    allocation record.
//
//    Eventually, we will add, AFTER the address, we add '+' or '-', and
//    then a second hex value, which is the number of bytes before or
//    after the user-defined address. But for now, this isn't done.
//
//    'non-heap' indicates that the pointer isn't pointing into a heap
//    at all.
//
//    'O' works the same, except hex digits are upper-cased.
//
// f) 's' Indicates that the object's 'to_string' method should be
//    preferred. Otherwise, objects get formatted per it's
//    default. HOWEVER, if the associated value is *not* an object,
//    then it is treated like a C string. This may be followed by '!'
//    to strip any formatting found in the argument.
//
// g) 'l' Indicates a preference for 'to_literal', otherwise follows
//    the rules of default formats. This may be followed by '!'
//    to strip any formatting found in the argument.
//
// h) 'c' formats the item as a Unicode code point. If it's a
//    non-printable codepoint, it will be represented as U+...
//
//    If it is not a codepoint, the default formatting rules will be
//    applied.
//
// i) 'b' formats the value as a boolean 'true' or 'false'. 'B' is the
//    same but upper-cases the value ('True' / 'False').
//
// j) 't' (and 'T') are the same as 'b', but produce 't' / 'f' (or 'T' / 'F').
//
// k) 'q' (and 'Q)' is also for booleans, producing 'Yes' or 'No' (the
//     Q stands for question)
//
// l) 'y' (and 'Y') produce 'y' / 'n' (or 'Y' / 'N')
//
// ** NOTE **
//
//    The above specifiers always get used, no matter what the
//    matching data is. The remaining specifier are fallbacks. That
//    means, if the associated data is an object that defined a format
//    routine, and if that format routine accepts the format string,
//    it gets first crack at the representation.
//
// m) 'x' Hex-formats the value as a number (not a pointer). It
//    currently does not take arguments, but we expect to add the
//    ability to pad and to zero-fill. It does NOT add a leading 0x;
//    you must do it yourself.
//
//    'X' is the same, except with upper-cased hex digits.
//
// n) 'i' formats the value as a signed integer. This will eventually
//    get arguments for padding and zfill, etc.
//
// o) 'u' formats the value as an unsigned integer.
//
// p) 'f' formats the value as a floating point number. Currently,
//    there are no modifiers here, but we do plan to change that as
//    well.

static inline void
assign_slots(rich_ctrl_t       *info,
             n00b_break_info_t *txt,
             n00b_break_info_t *ctrl,
             int                total)
{
    int txt_ix  = 0;
    int ctrl_ix = 0;
    int i       = 0;

    for (; i < total; i++) {
        if (txt_ix == txt->num_breaks || ctrl_ix == ctrl->num_breaks) {
            break;
        }
        if (txt->breaks[txt_ix] < ctrl->breaks[ctrl_ix]) {
            info[i].start_cp = txt->breaks[txt_ix++];
            info[i].end_cp   = txt->breaks[txt_ix++];
            info[i].type     = RICH_ORIGINAL_TEXT;
        }
        else {
            info[i].start_cp = ctrl->breaks[ctrl_ix++];
            info[i].end_cp   = ctrl->breaks[ctrl_ix++];
            info[i].type     = RICH_STYLE_TBD;
        }
    }

    n00b_break_info_t *rest;
    rich_ctl_type      ct;
    int                last_ix;

    if (txt_ix == txt->num_breaks) {
        rest    = ctrl;
        ct      = RICH_STYLE_TBD;
        last_ix = ctrl_ix;
    }
    else {
        rest    = txt;
        ct      = RICH_ORIGINAL_TEXT;
        last_ix = txt_ix;
    }

    for (; i < total; i++) {
        info[i].start_cp = rest->breaks[last_ix++];
        info[i].end_cp   = rest->breaks[last_ix++];
        info[i].type     = ct;
    }
}

static inline void
set_cheap_slices(n00b_string_t *s, rich_ctrl_t *info, int n)
{
    for (int i = 0; i < n; i++) {
        info[i].start = s->data + info[i].start_cp;
        info[i].end   = s->data + info[i].end_cp;
    }
}

static inline void
set_slice_info(n00b_string_t *s, rich_ctrl_t *info, int n)
{
    // This goes through the string at most once to assign to each
    // segment pointers to the start and end of that segment.
    if (s->codepoints == s->u8_bytes) {
        set_cheap_slices(s, info, n);
        return;
    }

    int              info_ix = 0;
    int              cp      = 0;
    char            *p       = s->data;
    n00b_codepoint_t unused;

    for (int i = 0; i < s->codepoints; i++) {
        if (info_ix == n) {
            break;
        }
        if (cp == info[info_ix].start_cp) {
            info[info_ix].start = p;
        }
        else {
            if (cp == info[info_ix].end_cp) {
                info[info_ix].end = p;
                info_ix++;
            }
        }
        advance((uint8_t **)&p, &unused, &cp);
    }

    if (info_ix != n) {
        info[info_ix].end = s->data + s->u8_bytes;
    }
}

static inline void
pointer_format(rich_ctrl_t *info, void *object, bool caps)
{
    info->type     = RICH_PAYLOAD;
    info->tsi.repr = n00b_fmt_pointer(object, caps);
}

static void
object_generic_repr(rich_ctrl_t *info, void *obj, n00b_alloc_hdr *h, bool caps)
{
    pointer_format(info, obj, caps);

    if (!h) {
        n00b_string_t *s = n00b_cstring("non-heap");
        info->tsi.repr   = n00b_string_concat(s, info->tsi.repr);
        return;
    }

    if (h == obj) {
        n00b_string_t *s = n00b_cstring("header");
        info->tsi.repr   = n00b_string_concat(s, info->tsi.repr);
        return;
    }

    if (h->data != obj) {
        n00b_string_t *s = n00b_cstring("offset");
        info->tsi.repr   = n00b_string_concat(s, info->tsi.repr);
        return;
    }

    if (!h->type) {
        n00b_string_t *s = n00b_cstring("alloc");
        info->tsi.repr   = n00b_string_concat(s, info->tsi.repr);
        return;
    }

    n00b_string_t *s = n00b_to_string(h->type);
    info->tsi.repr   = n00b_string_concat(s, info->tsi.repr);
    return;
}

static inline char *
get_arg_string_start(char *info)
{
    switch (*info) {
    case '#':
        if (*++info == '!') {
            info++;
        }
        break;
    case '!':
        info++;
        break;
    default:
        break;
    }
    return info;
}

static void
object_self_repr(rich_ctrl_t *info,
                 void        *object,
                 char        *start,
                 char        *end,
                 bool         strip)
{
    if (!n00b_in_any_heap(object)) {
        info->type     = RICH_PAYLOAD;
        info->tsi.repr = n00b_fmt_int((__int128_t)(int64_t)object, false);
        return;
    }

    n00b_alloc_hdr *h = n00b_find_allocation_record(object);

    if (!h) {
        pointer_format(info, object, false);
        return;
    }

    if (h->data != object || !h->type) {
        // If there's an argument, but no object, fall back to as-text.
        if (start == end) {
            object_generic_repr(info, object, h, false);
        }
        return;
    }

    int ti = h->type->base_index;

    if (ti == N00B_T_TYPESPEC) {
        info->type     = RICH_PAYLOAD;
        info->tsi.repr = n00b_to_string(object);
        return;
    }

    if (ti >= 0 && ti < N00B_NUM_BUILTIN_DTS) {
        n00b_format_fn fn;
        char          *pos  = get_arg_string_start(info->start);
        int            diff = info->end - pos;
        void          *res;

        fn = (void *)n00b_base_type_info[ti].vtable->methods[N00B_BI_FORMAT];
        if (fn) {
            n00b_string_t *in;

            if (diff <= 0) {
                in = n00b_cached_empty_string();
            }
            else {
                in = n00b_utf8(pos, diff);
            }

            res = fn(object, (void *)in);
            if (res) {
                goto successful_callback;
            }
            goto after_callback;
        }

        if (diff) {
            goto after_callback;
        }

        n00b_repr_fn repr = (void *)n00b_base_type_info[ti].vtable->methods[N00B_BI_TO_STRING];

        if (!repr) {
            goto after_callback;
        }

        res = repr(object);

successful_callback:

        if (res) {
            info->type = RICH_PAYLOAD;

            if (strip) {
                info->tsi.repr = n00b_string_reuse_text(res);
            }
            else {
                info->tsi.repr = res;
            }
            return;
        }
    }
after_callback:
    if (start == end) {
        object_generic_repr(info, object, h, false);
    }
}

static void
object_lit_repr(rich_ctrl_t *info, void *obj, bool strip)
{
    // If there's a to_literal call it, otherwise fall back to self_repr.
    n00b_string_t *s = n00b_to_literal(obj);

    if (!s) {
        object_self_repr(info, obj, NULL, NULL, strip);
        return;
    }

    info->type     = RICH_PAYLOAD;
    info->tsi.repr = s;
}

static void
object_tostr_repr(rich_ctrl_t *info, void *obj, bool upper)
{
    // If there's a to_string call it, otherwise fall back to self_repr.
    n00b_string_t *s = n00b_to_string(obj);

    if (!s) {
        object_self_repr(info, obj, NULL, NULL, upper);
        return;
    }

    info->type     = RICH_PAYLOAD;
    info->tsi.repr = s;
}

static void
bool_repr(rich_ctrl_t *info, void *obj, bool upper, bool word, bool yn)
{
    n00b_string_t *repr = n00b_fmt_bool(obj != NULL, upper, word, yn);

    info->tsi.repr = repr;
    info->type     = RICH_PAYLOAD;
}

static void
try_numeric_formatting(rich_ctrl_t *info, void *obj, char *start, char *end)
{
    n00b_string_t *repr;
    bool           commas = false;

    // Handle i, u, x, X, f.
    // For now, we aren't accepting any arguments on any of these except
    // the comma.
    //
    // Comma is ignored for x, X and f, for now.

    if (*(start + 1) == ',') {
        commas = true;
        if (start + 2 != end) {
            return;
        }
    }
    else {
        if (start + 1 != end) {
            return;
        }
    }

    switch (*start) {
    case 'i':
        repr = n00b_fmt_int((__int128_t)(int64_t)obj, commas);
        break;
    case 'u':
        repr = n00b_fmt_uint((int64_t)obj, commas);
        break;
    case 'x':
        repr = n00b_fmt_hex((uint64_t)obj, false);
        break;
    case 'X':
        repr = n00b_fmt_hex((uint64_t)obj, true);
        break;
    case 'f':;
        union {
            double d;
            void  *v;
        } convert;
        convert.v = obj;
        repr      = n00b_fmt_float(convert.d, 0, false);
        break;
    default:
        return;
    }

    if (repr != NULL) {
        info->tsi.repr = repr;
        info->type     = RICH_PAYLOAD;
    }
}

static inline bool
at_end(char *start, char *end)
{
    return (start + 1 == end);
}

static inline void
default_substitution(rich_ctrl_t *info, void *object)
{
    object_self_repr(info, object, 0, 0, false);
}

static inline void
default_then_strip(rich_ctrl_t *info, void *object)
{
    object_self_repr(info, object, 0, 0, true);
}

static void
sub_with_spec(rich_ctrl_t *info, void *object, char *start, char *end)
{
    // Pre-emptive; on success functions set this to the proper value.
    info->type = RICH_AS_TEXT;

    switch (*start) {
    case '!':
        if (at_end(start, end)) {
            default_then_strip(info, object);
        }
        object_self_repr(info, object, start + 1, end, true);
        return;
    case 'p':
        if (at_end(start, end)) {
            pointer_format(info, object, false);
            return;
        }
        break;
    case 'P':
        if (at_end(start, end)) {
            pointer_format(info, object, true);
            return;
        }
        break;
    case 'o':
        if (at_end(start, end)) {
            n00b_alloc_hdr *h = NULL;

            if (n00b_in_any_heap(object)) {
                h = n00b_find_allocation_record(object);
            }
            object_generic_repr(info, object, h, false);
            return;
        }
        break;
    case 'O':
        if (at_end(start, end)) {
            n00b_alloc_hdr *h = NULL;

            if (n00b_in_any_heap(object)) {
                h = n00b_find_allocation_record(object);
            }
            object_generic_repr(info, object, h, true);
            return;
        }
        break;
    case 's':
        if (at_end(start, end)) {
            object_tostr_repr(info, object, false);
            return;
        }
        if (at_end(start + 1, end) && *(start + 1) == '!') {
            object_tostr_repr(info, object, true);
            return;
        }
        break;
    case 'l':
        if (++start == end) {
            object_lit_repr(info, object, false);
            return;
        }
        if (at_end(start + 1, end) && *(start + 1) == '!') {
            object_lit_repr(info, object, true);
            return;
        }
        break;
    case 'c':
        if (at_end(start, end)) {
            info->type     = RICH_PAYLOAD;
            info->tsi.repr = n00b_fmt_codepoint((int)(int64_t)object);
            return;
        }
        break;
    case 'b':
        if (at_end(start, end)) {
            bool_repr(info, object, false, true, false);
            return;
        }
        break;
    case 'B':
        if (at_end(start, end)) {
            bool_repr(info, object, true, true, false);
            return;
        }
        break;
    case 't':
        if (at_end(start, end)) {
            bool_repr(info, object, false, false, false);
            return;
        }
        break;
    case 'T':
        if (at_end(start, end)) {
            bool_repr(info, object, true, false, false);
            return;
        }
        break;
    case 'q':
        if (at_end(start, end)) {
            bool_repr(info, object, false, true, true);
            return;
        }
        break;
    case 'Q':
        if (at_end(start, end)) {
            bool_repr(info, object, true, true, true);
            return;
        }
        break;
    case 'y':
        if (at_end(start, end)) {
            bool_repr(info, object, false, false, true);
            return;
        }
        break;
    case 'Y':
        if (at_end(start, end)) {
            bool_repr(info, object, true, false, true);
            return;
        }
        break;
    default:
        try_numeric_formatting(info, object, start, end);
        if (info->type != RICH_AS_TEXT) {
            return;
        }
        break;
    }
    object_self_repr(info, object, start, end, false);
    if (info->type == RICH_AS_TEXT) {
        try_numeric_formatting(info, object, start, end);
    }
}

static void
process_substitutions(rich_ctrl_t *info, int n, n00b_list_t *args)
{
    // This loops through each item, looks for control sequences, and
    // if it finds one, tries to process argument substitutions.  The
    // actual substitution logic is handled by either
    // 'default_substitution' or 'sub_with_spec', depending on whether
    // any spec was provided.
    int   next_positional = 0;
    int   nargs           = n00b_list_len(args);
    int   num_spec        = 0;
    bool  skip;
    char *p;
    char *end;
    void *item;
    bool  err = false;

    for (int i = 0; i < n; i++) {
        if (info[i].type != RICH_STYLE_TBD) {
            continue;
        }

        if (info[i].start == info[i].end || *info[i].start != '#') {
            continue;
        }

        p   = info[i].start + 1;
        end = info[i].end;

        if (p == end) {
            // Case where we have a solo hash mark.
            if (next_positional >= nargs) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }
            item = n00b_private_list_get(args, next_positional++, &err);

            if (err) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }
            default_substitution(&info[i], item);
            continue;
        }

        switch (*p) {
        case ':':
            p++;
            item = n00b_private_list_get(args, next_positional++, &err);
            if (err) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }
            sub_with_spec(&info[i], item, p, end);
            continue;
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
            num_spec = *p++ - '0';
            if (num_spec >= nargs) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }

            skip = false;

            do {
                switch (*p) {
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
                    num_spec *= 10;
                    num_spec += (*p++ - '0');
                    if (num_spec >= nargs) {
                        skip = true;
                        break;
                    }
                    continue;
                case ':':
                    p++;
                    break;
                case '!':
                    // Omitted : allowed if there's a !
                    break;
                default:
                    break;
                }
                break;
            } while (p != end);

            if (skip) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }

            item = n00b_private_list_get(args, num_spec, &err);
            if (err) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }

            // possible colon here.
            if (p == end) {
                default_substitution(&info[i], item);
                continue;
            }

            sub_with_spec(&info[i], item, p, end);
            continue;
        case '!':
            // Omitted : allowed if there's a !
            item = n00b_private_list_get(args, next_positional++, &err);
            if (err) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }
            sub_with_spec(&info[i], item, p, end);
            continue;
        default:
            // Doesn't follow the syntax, so treat literally.
            info[i].type = RICH_AS_TEXT;
            continue;
        }
    }
}

static inline rich_prop_t
extract_style_info(char *p, char *end, n00b_text_element_t **tp)
{
    int n = end - p;

    switch (n) {
    case 1:
        if (!strncmp(p, "b", n)) {
            return RICH_B;
        }
        if (!strncmp(p, "i", n)) {
            return RICH_I;
        }
        if (!strncmp(p, "u", n)) {
            return RICH_U;
        }
        break;
    case 2:
        if (!strncmp(p, "uu", n)) {
            return RICH_UU;
        }
        break;
    case 4:
        if (!strncmp(p, "bold", n)) {
            return RICH_B;
        }
        if (!strncmp(p, "caps", n)) {
            return RICH_ALLCAPS;
        }
        break;
    case 5:
        if (!strncmp(p, "upper", n)) {
            return RICH_UPPER;
        }
        if (!strncmp(p, "lower", n)) {
            return RICH_LOWER;
        }
        break;
    case 6:
        if (!strncmp(p, "italic", n)) {
            return RICH_I;
        }
        break;
    case 7:
        if (!strncmp(p, "reverse", n)) {
            return RICH_REV;
        }
        if (!strncmp(p, "allcaps", n)) {
            return RICH_ALLCAPS;
        }
        break;
    case 9:
        if (!strncmp(p, "underline", n)) {
            return RICH_U;
        }
        break;
    case 13:
        if (!strncmp(p, "strikethrough", n)) {
            return RICH_ST;
        }
        break;
    default:
        break;
    }

    n00b_box_props_t *x = n00b_lookup_style_by_tag(n00b_utf8(p, n));

    if (!x) {
        *tp = NULL;
    }
    else {
        *tp = &x->text_style;
    }
    return RICH_NONPROP;
}

static inline void
lookup_styles(rich_ctrl_t *info, int n)
{
    // Go through the ctrl items; for the ones still marked as
    // RICH_STYLE_TBD, look up the style name. If we find it, then set
    // the text props appropriately. Otherwise, convert the thing back
    // to text.

    n00b_text_element_t *style;
    rich_prop_t          prop;

    for (int i = 0; i < n; i++) {
        if (info[i].type != RICH_STYLE_TBD) {
            continue;
        }
        char *p = info[i].start;
        if (p == info[i].end) {
            info[i].type = RICH_AS_TEXT;
            continue;
        }
        if (*p == '/') {
            p++;
            if (p == info[i].end) {
                info[i].type = RICH_STYLE_RESET;
                continue;
            }

            prop = extract_style_info(p, info[i].end, &style);

            if (prop != RICH_NONPROP) {
                info[i].type     = RICH_PROP_OFF;
                info[i].tsi.prop = prop;
                continue;
            }
            if (!style) {
                info[i].type = RICH_AS_TEXT;
                continue;
            }
            info[i].type      = RICH_STYLE_OFF;
            info[i].tsi.style = style;
            continue;
        }

        prop = extract_style_info(p, info[i].end, &style);

        if (prop != RICH_NONPROP) {
            info[i].type     = RICH_PROP_ON;
            info[i].tsi.prop = prop;
            continue;
        }
        if (!style) {
            info[i].type = RICH_AS_TEXT;
            continue;
        }
        info[i].type      = RICH_STYLE_ON;
        info[i].tsi.style = style;
    }
}

static inline int
calculate_final_size(rich_ctrl_t *info, int n, int *sc)
{
    int byte_len = 0;
    int style_ct = 0;

    n00b_string_t *s;

    for (int i = 0; i < n; i++) {
        switch (info[i].type) {
        case RICH_AS_TEXT:
            // It's either an ASCII digraph or a 2 byte UTF-8 item.
            info[i].type = RICH_ORIGINAL_TEXT;
            info[i].start -= 2;
            info[i].end += 2;
            if (*info[i].start == '[') {
                info[i].start_cp -= 2;
                info[i].end_cp += 2;
            }
            else {
                info[i].start_cp -= 1;
                info[i].end_cp += 1;
            }

            // fallthrough
        case RICH_ORIGINAL_TEXT:
            byte_len += info[i].end - info[i].start;
            continue;
        case RICH_PAYLOAD:
            s = info[i].tsi.repr;
            byte_len += s->u8_bytes;
            if (s->styling) {
                style_ct += s->styling->num_styles;
            }
            break;
        default:
            // styling only. We may not use this style slot,
            // depending, but it's fine to over-allocate.
            style_ct++;
            continue;
        }
    }

    *sc = style_ct;
    return byte_len;
}

static inline n00b_text_element_t *
empty_props(void)
{
    n00b_text_element_t *result;

    result                   = n00b_gc_alloc_mapped(n00b_text_element_t,
                                  N00B_GC_SCAN_ALL);
    result->fg_palate_index  = N00B_COLOR_PALATE_UNSPECIFIED;
    result->bg_palate_index  = N00B_COLOR_PALATE_UNSPECIFIED;
    result->underline        = N00B_3_UNSPECIFIED;
    result->double_underline = N00B_3_UNSPECIFIED;
    result->bold             = N00B_3_UNSPECIFIED;
    result->italic           = N00B_3_UNSPECIFIED;
    result->strikethrough    = N00B_3_UNSPECIFIED;
    result->reverse          = N00B_3_UNSPECIFIED;
    result->text_case        = N00B_TEXT_UNSPECIFIED;

    return result;
}

static inline n00b_string_t *
final_assembly(rich_ctrl_t *info, int n)
{
    int            nstyles;
    int            new_ix;
    int            next_style = 0;
    int            last_style = -1;
    int            l          = calculate_final_size(info, n, &nstyles);
    n00b_string_t *result     = n00b_new(n00b_type_string(), NULL, true, l);
    int            cp_ix      = 0;
    char          *p          = result->data;
    bool           extend     = true;
    n00b_string_t *s;

    n00b_style_record_t *di, *si;
    n00b_text_element_t *cur_props = NULL;
    n00b_text_element_t *cur_style = empty_props();
    n00b_text_element_t *style;

    // this over-allocates because some of these nstyles will be props
    n00b_text_element_t *style_stack[nstyles];
    int                  style_ix = 0;

    if (nstyles) {
        result->styling = n00b_gc_flex_alloc(n00b_string_style_info_t,
                                             n00b_style_record_t,
                                             nstyles,
                                             N00B_GC_SCAN_ALL);
    }

    for (int i = 0; i < n; i++) {
        switch (info[i].type) {
        case RICH_PAYLOAD:
            s = info[i].tsi.repr;
            memcpy(p, s->data, s->u8_bytes);

            if (s->styling && s->styling->num_styles) {
                if (last_style != -1) {
                    new_ix = cp_ix + s->styling->styles[0].start;

                    result->styling->styles[last_style].end = new_ix;
                    last_style                              = -1;
                }

                for (int j = 0; j < s->styling->num_styles; j++) {
                    si        = &s->styling->styles[j];
                    di        = &result->styling->styles[next_style];
                    di->start = cp_ix + n00b_style_start(s, j);
                    di->end   = cp_ix + n00b_style_end(s, j);
                    di->info  = n00b_text_style_overlay(cur_style, si->info);

                    if (cur_props) {
                        di->info = n00b_text_style_overlay(cur_props, di->info);
                    }

                    if (j + 1 == s->styling->num_styles) {
                        if (n00b_style_extends_end(si)) {
                            extend     = true;
                            last_style = i;
                            cur_style  = empty_props();
                            memcpy(cur_style,
                                   si->info,
                                   sizeof(n00b_text_element_t));
                        }
                    }

                    next_style++;
                }
            }

            p += s->u8_bytes;
            cp_ix += s->codepoints;
            continue;
        case RICH_ORIGINAL_TEXT:
            l = info[i].end - info[i].start;
            memcpy(p, info[i].start, l);
            p += l;
            cp_ix += info[i].end_cp - info[i].start_cp;
            continue;
        case RICH_STYLE_ON:
            if (last_style != -1) {
                result->styling->styles[last_style].end = cp_ix;
            }

            if (style_ix < nstyles) {
                style_stack[style_ix++] = cur_style;
            }

            di         = &result->styling->styles[next_style];
            di->start  = cp_ix;
            last_style = next_style++;
            style      = info[i].tsi.style;

            if (cur_props) {
                style = n00b_text_style_overlay(cur_props, style);
            }
            di->info  = style;
            cur_style = style;
            extend    = true;
            continue;
        case RICH_STYLE_OFF:
            if (last_style != -1) {
                result->styling->styles[last_style].end = cp_ix;
            }
            if (style_ix > 0) {
                n00b_text_element_t *base_style = style_stack[--style_ix];
                if (cur_props) {
                    style = n00b_text_style_overlay(base_style, cur_props);
                }
                else {
                    style = base_style;
                }
                di         = &result->styling->styles[next_style];
                di->start  = cp_ix;
                di->info   = style;
                cur_style  = style;
                last_style = next_style++;
            }
            else if (cur_props) {
                style      = n00b_text_style_overlay(cur_style, cur_props);
                di         = &result->styling->styles[next_style];
                di->start  = cp_ix;
                di->info   = style;
                cur_style  = style;
                last_style = next_style++;
            }
            else {
                cur_style = empty_props();
            }
            extend = false;
            continue;
        case RICH_PROP_ON:
            if (last_style != -1) {
                result->styling->styles[last_style].end = cp_ix;
            }

            if (!cur_props) {
                cur_props = empty_props();
            }

            switch (info[i].tsi.prop) {
            case RICH_B:
                cur_props->bold = N00B_3_YES;
                break;
            case RICH_I:
                cur_props->italic = N00B_3_YES;
                break;
            case RICH_U:
                cur_props->underline        = N00B_3_YES;
                cur_props->double_underline = N00B_3_NO;
                break;
            case RICH_UU:
                cur_props->double_underline = N00B_3_YES;
                cur_props->underline        = N00B_3_NO;
                break;
            case RICH_REV:
                cur_props->reverse = N00B_3_YES;
                break;
            case RICH_ST:
                cur_props->strikethrough = N00B_3_YES;
                break;
            case RICH_UPPER:
                cur_props->text_case = N00B_TEXT_UPPER;
                break;
            case RICH_LOWER:
                cur_props->text_case = N00B_TEXT_LOWER;
                break;
            case RICH_ALLCAPS:
                cur_props->text_case = N00B_TEXT_ALL_CAPS;
                break;
            default:
                n00b_unreachable();
            }

            style = n00b_text_style_overlay(cur_style, cur_props);

            di         = &result->styling->styles[next_style];
            di->start  = cp_ix;
            di->info   = style;
            last_style = next_style++;
            extend     = true;
            continue;
        case RICH_PROP_OFF:
            if (last_style != -1) {
                result->styling->styles[last_style].end = cp_ix;
            }

            if (!cur_props) {
                cur_props = empty_props();
            }
            switch (info[i].tsi.prop) {
            case RICH_B:
                cur_props->bold = N00B_3_NO;
                break;
            case RICH_I:
                cur_props->italic = N00B_3_NO;
                break;
            case RICH_U:
                cur_props->underline        = N00B_3_NO;
                cur_props->double_underline = N00B_3_NO;
                break;
            case RICH_UU:
                cur_props->double_underline = N00B_3_NO;
                cur_props->underline        = N00B_3_NO;
                break;
            case RICH_REV:
                cur_props->reverse = N00B_3_NO;
                break;
            case RICH_ST:
                cur_props->strikethrough = N00B_3_NO;
                break;
            case RICH_UPPER:
                if (cur_props->text_case == N00B_TEXT_UPPER) {
                    cur_props->text_case = N00B_TEXT_AS_IS;
                }
                break;
            case RICH_LOWER:
                if (cur_props->text_case == N00B_TEXT_LOWER) {
                    cur_props->text_case = N00B_TEXT_AS_IS;
                }
                break;
            case RICH_ALLCAPS:
                if (cur_props->text_case == N00B_TEXT_ALL_CAPS) {
                    cur_props->text_case = N00B_TEXT_AS_IS;
                }
                break;
            default:
                n00b_unreachable();
            }

            style = n00b_text_style_overlay(cur_style, cur_props);
            assert(style != cur_props);

            di         = &result->styling->styles[next_style];
            di->start  = cp_ix;
            di->info   = style;
            last_style = next_style++;
            continue;
        case RICH_STYLE_RESET:
            if (last_style != -1) {
                result->styling->styles[last_style].end = cp_ix;
            }
            style_ix   = 0;
            cur_style  = empty_props();
            cur_props  = empty_props();
            di         = &result->styling->styles[next_style];
            di->start  = cp_ix;
            di->info   = style;
            last_style = next_style++;
            extend     = false;
            continue;
        default:
            n00b_unreachable();
        }
    }

    result->codepoints = cp_ix;

    if (result->styling) {
        result->styling->num_styles = next_style;

        if (last_style != -1) {
            int v = extend ? N00B_STYLE_EXTEND : result->codepoints;

            result->styling->styles[last_style].end = v;
        }
    }
    n00b_string_sanity_check(result);

    return result;
}

n00b_string_t *
_n00b_format(n00b_string_t *s, int nargs, ...)
{
    n00b_list_t *args = NULL;
    rich_ctrl_t *info;
    va_list      vlist;
    int          slot_count;

    n00b_break_info_t *text_breaks = n00b_break_alloc(s, 3);
    n00b_break_info_t *ctrl_breaks = n00b_break_alloc(s, 3);

    rich_raw_tokenize(s, &text_breaks, &ctrl_breaks);

    if (!ctrl_breaks->num_breaks) {
        return s;
    }

    slot_count = (text_breaks->num_breaks + ctrl_breaks->num_breaks) / 2;
    info       = n00b_gc_array_alloc(rich_ctrl_t, slot_count);

    assign_slots(info, text_breaks, ctrl_breaks, slot_count);
    set_slice_info(s, info, slot_count);

    va_start(vlist, nargs);

    if (nargs) {
        args = n00b_list(n00b_type_ref());

        for (int i = 0; i < nargs; i++) {
            void *arg = va_arg(vlist, void *);
            n00b_private_list_append(args, arg);
        }
    }

    process_substitutions(info, slot_count, args);
    lookup_styles(info, slot_count);
    return final_assembly(info, slot_count);
}

n00b_string_t *
n00b_format_arg_list(n00b_string_t *s, n00b_list_t *args)
{
    rich_ctrl_t *info;
    int          slot_count;

    n00b_break_info_t *text_breaks = n00b_break_alloc(s, 3);
    n00b_break_info_t *ctrl_breaks = n00b_break_alloc(s, 3);

    rich_raw_tokenize(s, &text_breaks, &ctrl_breaks);

    if (!ctrl_breaks->num_breaks) {
        return s;
    }

    slot_count = (text_breaks->num_breaks + ctrl_breaks->num_breaks) / 2;
    info       = n00b_gc_array_alloc(rich_ctrl_t, slot_count);

    assign_slots(info, text_breaks, ctrl_breaks, slot_count);
    set_slice_info(s, info, slot_count);

    process_substitutions(info, slot_count, args);
    lookup_styles(info, slot_count);
    return final_assembly(info, slot_count);
}

// Evaluates a literal.
n00b_string_t *
n00b_rich(n00b_string_t *s)
{
    return _n00b_format(s, 0);
}
