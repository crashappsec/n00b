#define N00B_USE_INTERNAL_API
#include "n00b.h"

// Parse ANSI codes from a UTF-8 string, and be able to generate
// strings from a series of ansi nodes.
//
// Going to use this for our 'expect'-like functionality. Some reasons:
//
// 1. Text matching should ignore these things.
//
// 2. We'd like to be able to convert ANSI codes back into our rich strings
//    when possible.
//
// 3. We will end up implementing some of the client side of the terminal
//    protocol at some point.
//
//
// The parser assumes that you will be incrementally adding to the
// state, and that there count be partial matches.
//
// This also does NOT lock the buffer. It assumes buffers are exclusive.
//
// This API is meant to do as little as possible in the parse, so doesn't
// do any up-leveling of codes, doesn't pull out parameters, etc.

static inline n00b_ansi_node_t *
ansi_node(n00b_ansi_ctx *ctx, n00b_ansi_kind kind, int backup)
{
    n00b_ansi_node_t *result = n00b_gc_alloc_mapped(n00b_ansi_node_t,
                                                    N00B_GC_SCAN_ALL);
    result->kind             = kind;
    result->start            = ctx->cur - (backup ? 1 : 0);

    n00b_list_append(ctx->results, result);

    return result;
}

static inline char *
ansi_advance(n00b_ansi_ctx *ctx, int l)
{
    ctx->cur += l;
    return ctx->cur;
}

static inline int
ansi_current(n00b_ansi_ctx *ctx, n00b_codepoint_t *ptr)
{
    if (ctx->cur == ctx->end) {
        *ptr = 0;
        return 0;
    }

    int len = n00b_min(4, ctx->end - ctx->cur);
    return utf8proc_iterate((uint8_t *)ctx->cur, len, ptr);
}

static inline void
c0_code(n00b_ansi_ctx *ctx, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_C0_CODE, 0);
    n->ctrl.ctrl_byte   = *ctx->cur;
    n->end              = ansi_advance(ctx, l);
}

static inline void
c1_code(n00b_ansi_ctx *ctx, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_C1_CODE, 0);
    n00b_codepoint_t  cp;
    ansi_current(ctx, &cp);
    n->ctrl.ctrl_byte = cp;
    n->end            = ansi_advance(ctx, l);
}

static inline void
printable_string(n00b_ansi_ctx *ctx, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_TEXT, 0);
    n00b_codepoint_t  cp;

    while (ctx->cur < ctx->end) {
        ansi_advance(ctx, l);
        l = ansi_current(ctx, &cp);
        if (l < 0) {
            n->kind = N00B_ANSI_PARTIAL;
            return;
        }

        if (utf8proc_category(cp) == UTF8PROC_CATEGORY_CC) {
            break;
        }
    }

    n->end = ctx->cur;
}

static inline void
node_invalidate(n00b_ansi_ctx *ctx, n00b_ansi_node_t *n)
{
    n->kind = N00B_ANSI_INVALID;
    n->end  = ctx->cur;
}

static inline void
private_ctrl(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l, int backup)
{
    n00b_ansi_node_t *n   = ansi_node(ctx, N00B_ANSI_PRIVATE_CONTROL, backup);
    int               len = 0;

    n->ctrl.ppi    = (uint8_t)cp;
    n->ctrl.pstart = ansi_advance(ctx, l);

    while (ctx->cur < ctx->end) {
        l = ansi_current(ctx, &cp);
        if (l < 0) {
            n->kind = N00B_ANSI_PARTIAL;
            return;
        }
        if (cp < 0x30 || cp > 0x3f) {
            break;
        }
        len++;
        ansi_advance(ctx, l);
    }

    n->ctrl.plen   = len;
    n->ctrl.istart = ctx->cur;
    len            = 0;

    while (ctx->cur < ctx->end) {
        if (cp < 0x20 || cp > 0x2f) {
            break;
        }
        len++;

        l = ansi_current(ctx, &cp);
        if (l < 0) {
            n->kind = N00B_ANSI_PARTIAL;
            return;
        }
        ansi_advance(ctx, l);
    }

    n->ctrl.ilen = len;

    if (ctx->cur == ctx->end && !cp) {
        n->kind = N00B_ANSI_PARTIAL;
        return;
    }

    if (cp < 0x40 || cp > 0x73) {
        node_invalidate(ctx, n);
    }
    else {
        n->ctrl.ctrl_byte = (uint8_t)cp;
    }

    n->end = ansi_advance(ctx, l);
}

static inline void
normal_ctrl(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l, int bu)
{
    n00b_ansi_node_t *n        = ansi_node(ctx, N00B_ANSI_CONTROL_SEQUENCE, bu);
    bool              colon_ok = true;
    int               len      = 0;

    l              = ansi_current(ctx, &cp);
    n->ctrl.pstart = ctx->cur;

    if (l < 0) {
        n->kind = N00B_ANSI_PARTIAL;
        return;
    }

    while (ctx->cur < ctx->end) {
        if (cp >= '0' && cp <= '9') {
param_advance:
            ansi_advance(ctx, l);
            l = ansi_current(ctx, &cp);
            if (l < 0) {
                n->kind = N00B_ANSI_PARTIAL;
                return;
            }
            n->ctrl.plen++;
            len++;
            continue;
        }
        if (cp == ';') {
            colon_ok = true;
            goto param_advance;
        }
        if (cp == ':') {
            if (!colon_ok) {
                break;
            }
            colon_ok = false;
            goto param_advance;
        }
        break;
    }

    n->ctrl.plen   = len;
    n->ctrl.istart = ctx->cur;
    len            = 0;

    while (ctx->cur < ctx->end) {
        if (cp < 0x20 || cp > 0x2f) {
            break;
        }
        len++;

        l = ansi_current(ctx, &cp);
        ansi_advance(ctx, l);
    }

    n->ctrl.ilen = len;

    if (ctx->cur == ctx->end && !cp) {
        n->kind = N00B_ANSI_PARTIAL;
        return;
    }

    if (cp < 0x40 || cp > 0x73) {
        node_invalidate(ctx, n);
    }
    else {
        n->ctrl.ctrl_byte = (uint8_t)cp;
    }

    n->end = ansi_advance(ctx, l);
}

static inline void
control_sequence(n00b_ansi_ctx *ctx, int backup)
{
    n00b_codepoint_t cp;
    int              l = ansi_current(ctx, &cp);

    if (l <= 0) {
        ansi_node(ctx, N00B_ANSI_PARTIAL, backup);
        return;
    }
    if (cp >= 0x3c && cp <= 0x3f) {
        private_ctrl(ctx, cp, l, backup);
    }
    else {
        normal_ctrl(ctx, cp, l, backup);
    }
}

static inline void
nf_sequence(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_NF_SEQUENCE, true);

    do {
        n->end = ansi_advance(ctx, l);
        l      = ansi_current(ctx, &cp);
        if (l < 0) {
            n->kind = N00B_ANSI_PARTIAL;
            return;
        }
    } while (ctx->cur < ctx->end && cp >= 0x20 && cp < 0x2f);

    if (ctx->cur == ctx->end && cp == 0) {
        n->kind = N00B_ANSI_PARTIAL;
        return;
    }

    if (cp < 0x30 || cp > 0x7e) {
        node_invalidate(ctx, n);
    }
    else {
        n->end = ansi_advance(ctx, l);
    }
}

static inline void
fp_sequence(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_FP_SEQUENCE, true);
    n->end              = ansi_advance(ctx, l);
}

static inline void
fe_sequence(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_FE_SEQUENCE, true);
    n->end              = ansi_advance(ctx, l);
}

static inline void
fs_sequence(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_FS_SEQUENCE, true);
    n->end              = ansi_advance(ctx, l);
}

static inline void
bad_sequence(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_INVALID, true);
    n->end              = ansi_advance(ctx, l);
}

static inline void
command_string(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l, int backup)
{
    n00b_ansi_node_t *n       = ansi_node(ctx, N00B_ANSI_CTL_STR_CHAR, backup);
    bool              got_esc = false;
    bool              got_eos = false;

    while (ctx->cur < ctx->end) {
        l = ansi_current(ctx, &cp);
        if (cp == 0x9c || (got_esc && cp == 0x5c)) {
            got_eos = true;
            ansi_advance(ctx, l);
            break;
        }
        if (cp == 0x1b) {
            got_esc = true;
            goto advance;
        }
        got_esc = false;
        if (cp >= 0x08 && cp <= 0xd) {
            goto advance;
        }
        if (cp >= 0x20 && cp <= 0x7e) {
            goto advance;
        }
        if (cp >= 0xa0) {
advance:
            ansi_advance(ctx, l);
            l = ansi_current(ctx, &cp);
            continue;
        }

        node_invalidate(ctx, n);
        return;
    }

    n->end = ctx->cur;

    if (!got_eos) {
        n->kind = N00B_ANSI_PARTIAL;
    }
}

static inline void
character_string(n00b_ansi_ctx *ctx, int backup)
{
    n00b_ansi_node_t *n = ansi_node(ctx, N00B_ANSI_CTL_STR_CHAR, backup);
    n00b_codepoint_t  cp;
    int               l;
    bool              got_esc = false;
    bool              got_eos = false;

    while (ctx->cur < ctx->end) {
        l = ansi_current(ctx, &cp);
        if (cp == 0x9c || (got_esc && cp == 0x5c)) {
            got_eos = true;
            ansi_advance(ctx, l);
            break;
        }
        if (cp == 0x1b) {
            got_esc = true;
            goto advance;
        }
        got_esc = false;
        if (cp == 0x18 || cp == 0x1a || (cp > 0x7f && cp < 0xa0)) {
            node_invalidate(ctx, n);
            return;
        }
advance:
        ansi_advance(ctx, l);
        l = ansi_current(ctx, &cp);
        continue;
    }

    n->end = ctx->cur;

    if (!got_eos) {
        n->kind = N00B_ANSI_PARTIAL;
    }
}

static inline void
control_start(n00b_ansi_ctx *ctx, n00b_codepoint_t cp, int l)
{
    if (cp == 0x9b) {
        ansi_advance(ctx, l);
        control_sequence(ctx, 0);
        return;
    }

    if (cp == 0x98) {
        ansi_advance(ctx, l);
        character_string(ctx, 0);
        return;
    }

    if (cp == 0x90 || (cp >= 0x9d && cp <= 0x9f)) {
        command_string(ctx, cp, l, 0);
        return;
    }

    if (cp == 0x1b) { // CSI
        ansi_advance(ctx, l);
        if (ctx->end == ctx->cur) {
            ansi_node(ctx, N00B_ANSI_PARTIAL, 1);
            return;
        }
        l = ansi_current(ctx, &cp);
        switch (cp) {
        case 0x5b:
            ansi_advance(ctx, l);
            control_sequence(ctx, 1);
            return;
        case 0x50:
        case 0x5d:
        case 0x5e:
        case 0x5f:
            command_string(ctx, cp, l, 1);
            return;
        case 0x58:
            ansi_advance(ctx, l);
            character_string(ctx, 1);
            return;
        default:
            if (cp >= 0x20 && cp <= 0x2f) {
                nf_sequence(ctx, cp, l);
                return;
            }
            if (cp >= 0x30 && cp <= 0x3f) {
                fp_sequence(ctx, cp, l);
                return;
            }
            if (cp >= 0x40 && cp <= 0x5a && cp != 0x50 && cp != 0x58) {
                fe_sequence(ctx, cp, l);
                return;
            }
            if (cp >= 0x60 && cp <= 0x7e) {
                fs_sequence(ctx, cp, l);
                return;
            }
            bad_sequence(ctx, cp, l);
            return;
        }
    }

    if (cp < 0x1f) {
        c0_code(ctx, l);
    }
    else {
        c1_code(ctx, l);
    }
}

n00b_ansi_ctx *
n00b_ansi_parser_create(void)
{
    n00b_ansi_ctx *result = n00b_gc_alloc_mapped(n00b_ansi_ctx,
                                                 N00B_GC_SCAN_ALL);
    result->results       = n00b_list(n00b_type_ref());

    return result;
}

static void
n00b_ansi_parse_internal(n00b_ansi_ctx *ctx)
{
    n00b_codepoint_t cp;

    while (ctx->cur < ctx->end) {
        int l = ansi_current(ctx, &cp);
        // We can end up past the end of the input in our accounting.
        // Ideally, we won't have checked those bytes, we just add
        // to the pointer optimistically.
        if (l < 0) {
            return;
        }

        if (utf8proc_category(cp) == UTF8PROC_CATEGORY_CC) {
            control_start(ctx, cp, l);
        }
        else {
            printable_string(ctx, l);
        }
    }
}

static inline n00b_buf_t *
combine_partial(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    n00b_ansi_node_t *n = n00b_list_pop(ctx->results);

    while (*n->start != '\e') {
        --n->start;
    }

    // If there's room in the current buffer, we'll slide over the
    // contents and add in the partials from last time.
    int diff = ctx->end - n->start;

    if (b->alloc_len - b->byte_len > diff) {
        memmove(b->data + diff, b->data, b->byte_len);
        memcpy(b->data, n->start, diff);
        b->byte_len += diff;
        return b;
    }

    // Otherwise, instead of complicating the parse, we make
    // ourselves a new buffer.

    int         new_len = b->byte_len + diff;
    n00b_buf_t *new_buf = n00b_new(n00b_type_buffer(), length : new_len);

    memcpy(new_buf->data, n->start, diff);
    memcpy(new_buf->data + diff, b->data, b->byte_len);

    return new_buf;
}

// Incrementally parses. If we were in the middle of a control
// sequence or in the middle of a unicode code point last time, then
// we combine that ol' partial.
void
n00b_ansi_parse(n00b_ansi_ctx *ctx, n00b_buf_t *b)
{
    int len = n00b_list_len(ctx->results);
    if (len) {
        n00b_ansi_node_t *n = n00b_list_get(ctx->results, len - 1, NULL);

        if (n->kind == N00B_ANSI_PARTIAL) {
            b = combine_partial(ctx, b);
        }
    }

    ctx->cur = b->data;
    ctx->end = b->data + b->byte_len;

    n00b_ansi_parse_internal(ctx);
}
n00b_list_t *
n00b_ansi_parser_results(n00b_ansi_ctx *ctx)
{
    n00b_list_t *new_list = n00b_list(n00b_type_ref());
    n00b_list_t *result   = ctx->results;
    int          len      = n00b_list_len(ctx->results);

    ctx->results = new_list;

    if (len) {
        n00b_ansi_node_t *n = n00b_list_get(result, len - 1, NULL);
        if (n->kind == N00B_ANSI_PARTIAL) {
            n00b_list_pop(result);
            n00b_list_append(new_list, n);
        }
    }

    return result;
}

static n00b_string_t *
format_control_sequence(n00b_ansi_node_t *node)
{
    n00b_string_t *hdr;
    n00b_string_t *params;
    n00b_string_t *ctrl;

    if (node->ctrl.ppi) {
        hdr = n00b_cformat("private ([|#:x|])", (int64_t)node->ctrl.ppi);
    }
    else {
        hdr = n00b_cstring("ctrl");
    }

    if (node->ctrl.plen) {
        params = n00b_cformat("'[|#|]'",
                              n00b_utf8(node->ctrl.pstart, node->ctrl.plen));
    }
    else {
        params = n00b_cstring("(none)");
    }

    // This should get the control byte.
    ctrl = n00b_utf8(node->ctrl.istart, node->ctrl.ilen + 1);

    return n00b_cformat("[|#|] [|#|]: params=[|#|]", hdr, ctrl, params);
}

static n00b_string_t *
format_control_string(n00b_ansi_node_t *node, int len)
{
    // The end contains a string terminator we back up over.
    // The start contains some kind of initiator.
    //
    // For both of these, either it's ESC + a low-ascii, or a two-byte
    // sequence. So we move forward the start by 2 bytes, and will subtract
    // 4 from the length when creating a string object.
    char *p = node->start + 2;

    return n00b_cformat("ctrl str: [|#|]", n00b_bytes_to_hex(p, len - 4));
}

static n00b_string_t *
format_control_command(n00b_ansi_node_t *node, int len)
{
    n00b_codepoint_t cp;
    if (utf8proc_iterate((uint8_t *)node->start, 2, &cp) == 1) {
        // Was an 'escape', so grab the next char.
        cp = *(node->start + 1);
    }

    // The + 2 skips over the whole sequence.
    char *p = node->start + 2;

    n00b_string_t *kind;

    switch (cp) {
    case 0x50:
    case 0x90:
        kind = n00b_cstring("DCS");
        break;
    case 0x5d:
    case 0x9d:
        kind = n00b_cstring("OSC");
        break;
    case 0x5e:
    case 0x9e:
        kind = n00b_cstring("PM");
        break;
    case 0x5f:
    case 0x9f:
        kind = n00b_cstring("APC");
        break;
    default:
        n00b_unreachable();
    }

    // The subtract by 4 removes the start and end sequence.
    return n00b_cformat("ctrl cmd [|#|]: [|#|]",
                        kind,
                        n00b_bytes_to_hex(p, len - 4));
}

n00b_string_t *
n00b_ansi_node_repr(n00b_ansi_node_t *node)
{
    n00b_string_t *tmp;
    int            len = node->end - node->start;

    switch (node->kind) {
    case N00B_ANSI_TEXT:
        tmp = n00b_utf8(node->start, len);
        return n00b_cformat("string ([|#|] bytes): [|#|]", len, tmp);
    case N00B_ANSI_C0_CODE:
        return n00b_cformat("c0: [|#:x|]", (int64_t)(*node->start));
    case N00B_ANSI_C1_CODE:
        return n00b_cformat("c1: [|#:x|]", (int64_t)(*node->start));
    case N00B_ANSI_NF_SEQUENCE:
        return n00b_cformat("nf: [|#|] bytes: [|#|]",
                            (int64_t)len,
                            n00b_bytes_to_hex(node->start, len));
    case N00B_ANSI_FP_SEQUENCE:
        return n00b_cformat("fp: [|#:x|]", (int64_t)(*node->start));
    case N00B_ANSI_FE_SEQUENCE:
        return n00b_cformat("fe: [|#:x|]", (int64_t)(*node->start));
    case N00B_ANSI_FS_SEQUENCE:
        return n00b_cformat("fs: [|#:x|]", (int64_t)(*node->start));
    case N00B_ANSI_CONTROL_SEQUENCE:
    case N00B_ANSI_PRIVATE_CONTROL:
        return format_control_sequence(node);
    case N00B_ANSI_CTL_STR_CHAR:
        return format_control_string(node, len);
    case N00B_ANSI_CRL_STR_CMD:
        return format_control_command(node, len);
    case N00B_ANSI_PARTIAL:
        return n00b_cformat("partial: [|#|] bytes: [|#|]",
                            (int64_t)len,
                            n00b_bytes_to_hex(node->start, len));
    case N00B_ANSI_INVALID:
        return n00b_cformat("invalid: [|#|] bytes: [|#|]",
                            (int64_t)len,
                            n00b_bytes_to_hex(node->start, len));
    default:
        n00b_unreachable();
    }
}

static inline n00b_string_t *
one_node_to_string(n00b_ansi_node_t *node)
{
    char *p   = node->start;
    char *end = node->end;

    switch (node->kind) {
    case N00B_ANSI_CONTROL_SEQUENCE:
    case N00B_ANSI_PRIVATE_CONTROL:
    case N00B_ANSI_NF_SEQUENCE:
    case N00B_ANSI_FP_SEQUENCE:
    case N00B_ANSI_FE_SEQUENCE:
    case N00B_ANSI_FS_SEQUENCE:
    case N00B_ANSI_CTL_STR_CHAR:
    case N00B_ANSI_CRL_STR_CMD:
        while (*p != '\e') {
            p -= 1;
        }
        break;
    case N00B_ANSI_C0_CODE:
    case N00B_ANSI_C1_CODE:
        // Always strip cr, we don 't really want them.
        if (node->ctrl.ctrl_byte == '\r') {
            return n00b_cached_empty_string();
        }
        return n00b_string_from_codepoint(node->ctrl.ctrl_byte);
    default:
        break;
    }

    return n00b_utf8(p, end - p);
}

n00b_string_t *
n00b_ansi_nodes_to_string(n00b_list_t *nodes, bool keep_control)
{
    // Newlines always get kept, even if we're stripping control
    // sequences.
    n00b_list_t   *pieces = n00b_list(n00b_type_string());
    int            n      = n00b_list_len(nodes);
    n00b_string_t *s;

    if (keep_control) {
        for (int i = 0; i < n; i++) {
            n00b_ansi_node_t *node = n00b_list_get(nodes, i, NULL);
            s                      = one_node_to_string(node);
            n00b_string_sanity_check(s);
            n00b_list_append(pieces, s);
        }
    }
    else {
        for (int i = 0; i < n; i++) {
            n00b_ansi_node_t *node = n00b_list_get(nodes, i, NULL);

            switch (node->kind) {
            case N00B_ANSI_TEXT:
                break;
            case N00B_ANSI_C0_CODE:
                switch (node->ctrl.ctrl_byte) {
                case '\n':
                    n00b_list_append(pieces, n00b_cached_newline());
                    continue;
                case '\t':
                    n00b_list_append(pieces, n00b_string_from_codepoint('\t'));
                    continue;
                default:
                    continue;
                }
            default:
                continue;
            }

            s = one_node_to_string(node);
            n00b_string_sanity_check(s);
            n00b_list_append(pieces, s);
        }
    }

    return n00b_string_join(pieces, n00b_cached_empty_string());
}
