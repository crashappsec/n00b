#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
buffer_init(n00b_buf_t *obj, va_list args)
{
    int64_t        length = -1;
    char          *raw    = NULL;
    n00b_string_t *hex    = NULL;
    char          *ptr    = NULL;

    n00b_karg_va_init(args);

    n00b_kw_int64("length", length);
    n00b_kw_ptr("raw", raw);
    n00b_kw_ptr("hex", hex);
    n00b_kw_ptr("ptr", ptr);

    if (raw == NULL && hex == NULL && ptr == NULL) {
        if (length < 0) {
            N00B_CRAISE("Invalid buffer initializer.");
        }
    }
    if (length < 0) {
        if (hex == NULL) {
            N00B_CRAISE("Must specify length for raw / ptr initializer.");
        }
        else {
            length = n00b_string_codepoint_len(hex) >> 1;
        }
    }

    n00b_static_rw_lock_init(obj->lock);
    // Asserts the lock is never around code that would wait long
    // enough to impair the GC.
    n00b_rw_lock_set_nosleep(&obj->lock);

    if (length == 0) {
        obj->alloc_len = N00B_EMPTY_BUFFER_ALLOC;
        obj->data      = n00b_gc_raw_alloc(obj->alloc_len, NULL);
        obj->byte_len  = 0;
        return;
    }

    if (length > 0 && ptr == NULL) {
        int64_t alloc_len = hatrack_round_up_to_power_of_2(length);

        obj->data      = n00b_gc_raw_alloc(alloc_len, NULL);
        obj->alloc_len = alloc_len;
    }

    if (raw != NULL) {
        if (hex != NULL) {
            N00B_CRAISE("Cannot set both 'hex' and 'raw' fields.");
        }
        if (ptr != NULL) {
            N00B_CRAISE("Cannot set both 'ptr' and 'raw' fields.");
        }

        memcpy(obj->data, raw, length);
    }

    if (ptr != NULL) {
        if (hex != NULL) {
            N00B_CRAISE("Cannot set both 'hex' and 'ptr' fields.");
        }

        obj->data = ptr;
    }

    if (hex != NULL) {
        uint8_t cur         = 0;
        int     valid_count = 0;

        for (int i = 0; i < hex->u8_bytes; i++) {
            uint8_t byte = hex->data[i];
            if (byte >= '0' && byte <= '9') {
                if ((++valid_count) % 2 == 1) {
                    cur = (byte - '0') << 4;
                }
                else {
                    cur |= (byte - '0');
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
            if (byte >= 'a' && byte <= 'f') {
                if ((++valid_count) % 2 == 1) {
                    cur = ((byte - 'a') + 10) << 4;
                }
                else {
                    cur |= (byte - 'a') + 10;
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
            if (byte >= 'A' && byte <= 'F') {
                if ((++valid_count) % 2 == 1) {
                    cur = ((byte - 'A') + 10) << 4;
                }
                else {
                    cur |= (byte - 'A') + 10;
                    obj->data[obj->byte_len++] = cur;
                }
                continue;
            }
        }
    }
    else {
        obj->byte_len = length;
    }
}

void
n00b_buffer_resize(n00b_buf_t *buffer, uint64_t new_sz)
{
    n00b_buffer_acquire_w(buffer);

    if ((int64_t)new_sz <= buffer->alloc_len) {
        buffer->byte_len = new_sz;
        n00b_buffer_release(buffer);
        return;
    }

    // Resize up, copying old data and leaving the rest zero'd.
    uint64_t new_alloc_sz = hatrack_round_up_to_power_of_2(new_sz);
    char    *new_data     = n00b_gc_raw_alloc(new_alloc_sz, NULL);

    memcpy(new_data, buffer->data, buffer->byte_len);

    buffer->data      = new_data;
    buffer->byte_len  = new_sz;
    buffer->alloc_len = new_alloc_sz;

    printf("Resized buffer %p to have %d bytes (%d alloced)\n",
           buffer,
           buffer->byte_len,
           buffer->alloc_len);

    n00b_buffer_release(buffer);
}

n00b_string_t *
n00b_buffer_to_hex_str(n00b_buf_t *buf)
{
    n00b_string_t *result;

    n00b_buffer_acquire_r(buf);
    result  = n00b_alloc_utf8_to_copy(buf->byte_len * 2);
    char *p = result->data;

    for (int i = 0; i < buf->byte_len; i++) {
        uint8_t c = ((uint8_t *)buf->data)[i];
        *p++      = n00b_hex_map_lower[(c >> 4)];
        *p++      = n00b_hex_map_lower[c & 0x0f];
    }

    result->codepoints = p - result->data;
    result->u8_bytes   = result->codepoints;
    result->styling    = NULL;

    n00b_buffer_release(buf);

    return result;
}

static n00b_string_t *
buffer_repr(n00b_buf_t *buf)
{
    n00b_buffer_acquire_r(buf);

    n00b_string_t *result = n00b_hex_dump(buf->data, buf->byte_len);

    result->styling = NULL;
    n00b_buffer_release(buf);

    return result;
}

static n00b_string_t *
buffer_fmt(n00b_buf_t *buf, n00b_string_t *spec)
{
    if (!spec || !n00b_string_codepoint_len(spec)) {
        return buffer_repr(buf);
    }

    if (n00b_string_codepoint_len(spec)) {
        switch (spec->data[0]) {
        case 'h':
            return buffer_repr(buf);
        case 'x':
            return n00b_buffer_to_hex_str(buf);
        case 'p':
            return n00b_cformat("@«#:p»", buf);
        case 'u':
            return n00b_buf_to_utf8_string(buf);
        default:
            break;
        }
    }

    N00B_CRAISE("Invalid format specifier for buffer.");
}

n00b_buf_t *
n00b_buffer_add(n00b_buf_t *b1, n00b_buf_t *b2)
{
    if (!b1) {
        return b2;
    }
    if (!b2) {
        return b1;
    }

    n00b_buffer_acquire_r(b1);
    n00b_buffer_acquire_r(b2);

    int64_t     l1     = n00b_max(n00b_buffer_len(b1), 0);
    int64_t     l2     = n00b_max(n00b_buffer_len(b2), 0);
    int64_t     lnew   = l1 + l2;
    n00b_buf_t *result = n00b_new(n00b_type_buffer(),
                                  n00b_kw("length", n00b_ka(lnew)));

    if (l1 > 0) {
        memcpy(result->data, b1->data, l1);
    }

    if (l2 > 0) {
        char *p = result->data + l1;
        memcpy(p, b2->data, l2);
    }

    n00b_buffer_release(b2);
    n00b_buffer_release(b1);

    return result;
}

n00b_buf_t *
n00b_buffer_join(n00b_list_t *list, n00b_buf_t *joiner)
{
    n00b_buffer_acquire_r(joiner);

    int64_t num_items = n00b_list_len(list);
    int64_t new_len   = 0;
    int     jlen      = 0;

    for (int i = 0; i < num_items; i++) {
        n00b_buf_t *n = n00b_list_get(list, i, NULL);

        n00b_buffer_acquire_r(n);
        new_len += n00b_buffer_len(n);
    }

    if (joiner != NULL) {
        jlen = n00b_buffer_len(joiner);
        new_len += jlen * (num_items - 1);
    }

    n00b_buf_t *result = n00b_new(n00b_type_buffer(),
                                  n00b_kw("length", n00b_ka(new_len)));
    char       *p      = result->data;
    n00b_buf_t *cur    = n00b_list_get(list, 0, NULL);
    int         clen   = n00b_buffer_len(cur);

    memcpy(p, cur->data, clen);

    n00b_buffer_release(cur);

    for (int i = 1; i < num_items; i++) {
        p += clen;

        if (jlen != 0) {
            memcpy(p, joiner->data, jlen);
            p += jlen;
        }

        cur  = n00b_list_get(list, i, NULL);
        clen = n00b_buffer_len(cur);
        memcpy(p, cur->data, clen);
        n00b_buffer_release(cur);
    }

    n00b_buffer_release(joiner);

    n00b_assert(p - result->data == new_len);

    return result;
}

int64_t
n00b_buffer_len(n00b_buf_t *buffer)
{
    return (int64_t)buffer->byte_len;
}

n00b_string_t *
n00b_buf_to_utf8_string(n00b_buf_t *buffer)
{
    n00b_buffer_acquire_r(buffer);
    n00b_string_t *result = n00b_utf8(buffer->data, buffer->byte_len);
    n00b_buffer_release(buffer);

    return result;
}

static bool
buffer_can_coerce_to(n00b_type_t *my_type, n00b_type_t *target_type)
{
    // clang-format off
    if (n00b_types_are_compat(target_type, n00b_type_string(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_buffer(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_bytering(), NULL) ||
	n00b_types_are_compat(target_type, n00b_type_bool(), NULL)) {
        return true;
    }
    // clang-format on

    return false;
}

static n00b_obj_t
buffer_coerce_to(n00b_buf_t *b, n00b_type_t *target_type)
{
    if (n00b_types_are_compat(target_type, n00b_type_buffer(), NULL)) {
        return (n00b_obj_t)b;
    }

    if (n00b_types_are_compat(target_type, n00b_type_bool(), NULL)) {
        if (!b || b->byte_len == 0) {
            return (n00b_obj_t) false;
        }
        else {
            return (n00b_obj_t) true;
        }
    }

    if (n00b_types_are_compat(target_type, n00b_type_bytering(), NULL)) {
        n00b_buffer_acquire_r(b);
        return n00b_new(target_type, n00b_kw("buffer", b));
        n00b_buffer_release(b);
    }

    if (n00b_types_are_compat(target_type, n00b_type_string(), NULL)) {
        n00b_buffer_acquire_r(b);

        int32_t          count = 0;
        uint8_t         *p     = (uint8_t *)b->data;
        uint8_t         *end   = p + b->byte_len;
        n00b_codepoint_t cp;
        int              cplen;

        while (p < end) {
            count++;
            cplen = utf8proc_iterate(p, 4, &cp);
            if (cplen < 0) {
                N00B_CRAISE("Buffer contains invalid UTF-8");
            }
            p += cplen;
        }

        n00b_string_t *result = n00b_new(target_type,
                                         n00b_kw("length", n00b_ka(b->byte_len)));
        result->codepoints    = count;

        memcpy(result->data, b->data, b->byte_len);
        n00b_buffer_release(b);
    }

    N00B_CRAISE("Invalid conversion from buffer type");
}

static uint8_t
buffer_get_index(n00b_buf_t *b, int64_t n)
{
    n00b_buffer_acquire_r(b);
    if (n < 0) {
        n += b->byte_len;

        if (n < 0) {
            n00b_buffer_release(b);
            N00B_CRAISE("Index would be before the start of the buffer.");
        }
    }

    if (n >= b->byte_len) {
        n00b_buffer_release(b);
        N00B_CRAISE("Index out of bounds.");
    }

    uint8_t result = b->data[n];
    n00b_buffer_release(b);
    return result;
}

static void
buffer_set_index(n00b_buf_t *b, int64_t n, int8_t c)
{
    n00b_buffer_acquire_w(b);

    if (n < 0) {
        n += b->byte_len;

        if (n < 0) {
            n00b_buffer_release(b);
            N00B_CRAISE("Index would be before the start of the buffer.");
        }
    }

    if (n >= b->byte_len) {
        n00b_buffer_release(b);
        N00B_CRAISE("Index out of bounds.");
    }

    b->data[n] = c;
    n00b_buffer_release(b);
}

static n00b_buf_t *
buffer_get_slice(n00b_buf_t *b, int64_t start, int64_t end)
{
    n00b_buffer_acquire_r(b);

    int64_t len = b->byte_len;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            n00b_buffer_release(b);
            return n00b_new(n00b_type_buffer(), n00b_kw("length", n00b_ka(0)));
        }
    }
    if (end < 0) {
        end += len + 1;
    }
    else {
        if (end > len) {
            end = len;
        }
    }
    if ((start | end) < 0 || start >= end) {
        n00b_buffer_release(b);
        return n00b_new(n00b_type_buffer(), n00b_kw("length", n00b_ka(0)));
    }

    int64_t     slice_len = end - start;
    n00b_buf_t *result    = n00b_new(n00b_type_buffer(),
                                  n00b_kw("length", n00b_ka(slice_len)));

    memcpy(result->data, b->data + start, slice_len);

    n00b_buffer_release(b);

    return result;
}

static void
buffer_set_slice(n00b_buf_t *b, int64_t start, int64_t end, n00b_buf_t *val)
{
    n00b_buffer_acquire_w(b);

    int64_t len = b->byte_len;

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            n00b_buffer_release(b);
            N00B_CRAISE("Slice out-of-bounds.");
        }
    }
    if (end < 0) {
        end += len + 1;
    }
    else {
        if (end > len) {
            end = len;
        }
    }
    if ((start | end) < 0 || start >= end) {
        n00b_buffer_release(b);
        N00B_CRAISE("Slice out-of-bounds.");
    }

    int64_t slice_len   = end - start;
    int64_t new_len     = b->byte_len - slice_len;
    int64_t replace_len = 0;

    if (val != NULL) {
        replace_len = val->byte_len;
        new_len += replace_len;
    }

    if (new_len < b->byte_len) {
        if (val != NULL && val->byte_len > 0) {
            memcpy(b->data + start, val->data, replace_len);
        }

        if (end < b->byte_len) {
            memcpy(b->data + start + replace_len,
                   b->data + end,
                   b->byte_len - end);
        }
    }
    else {
        char *new_buf = n00b_gc_raw_alloc(new_len, NULL);
        if (start > 0) {
            memcpy(new_buf, b->data, start);
        }
        if (replace_len != 0) {
            memcpy(new_buf + start, val->data, replace_len);
        }
        if (end < b->byte_len) {
            memcpy(new_buf + start + replace_len,
                   b->data + end,
                   b->byte_len - end);
        }
        b->data = new_buf;
    }

    b->byte_len = new_len;
    n00b_buffer_release(b);
}

static n00b_obj_t
buffer_lit(n00b_string_t        *su8,
           n00b_lit_syntax_t     st,
           n00b_string_t        *lu8,
           n00b_compile_error_t *err)
{
    char *s      = su8->data;
    char *litmod = lu8->data;

    if (!strcmp(litmod, "h") || !strcmp(litmod, "hex")) {
        int length = strlen(s);
        if (length & 2) {
            *err = n00b_err_parse_lit_odd_hex;
            return NULL;
        }
        return n00b_new(n00b_type_buffer(),
                        n00b_kw("length", n00b_ka(length), "hex", n00b_ka(s)));
    }

    return n00b_new(n00b_type_buffer(), n00b_kw("raw", n00b_ka(s)));
}

// NOTE: this function is currently of the utmost importance.
// We cannot use automarshal to marshal / unmarshal arbitrary
// buffers, because autounmarshal might try to copy the buffer
// passed into it. So this one *has* to be manual.
static n00b_buf_t *
buffer_copy(n00b_buf_t *inbuf)
{
    n00b_buffer_acquire_r(inbuf);
    n00b_buf_t *outbuf = n00b_new(n00b_type_buffer(),
                                  n00b_kw("length", n00b_ka(inbuf->byte_len)));

    memcpy(outbuf->data, inbuf->data, inbuf->byte_len);

    n00b_buffer_release(inbuf);

    return outbuf;
}

static n00b_type_t *
buffer_item_type(n00b_obj_t x)
{
    return n00b_type_byte();
}

static void *
buffer_view(n00b_buf_t *inbuf, uint64_t *outlen)
{
    n00b_buf_t *result = buffer_copy(inbuf);

    *outlen = result->byte_len;

    return result->data;
}

int64_t
_n00b_buffer_find(n00b_buf_t *b, n00b_buf_t *sub, ...)
{
    int64_t start = 0;
    int64_t end   = -1;

    n00b_karg_only_init(sub);

    n00b_kw_int64("start", start);
    n00b_kw_int64("end", end);

    uint64_t blen   = n00b_len(b);
    uint64_t sublen = n00b_len(sub);

    if (start < 0) {
        start += blen;
    }
    if (start < 0) {
        return -1;
    }
    if (end < 0) {
        end += blen + 1;
    }
    if (end <= start) {
        return -1;
    }
    if ((uint64_t)end > blen) {
        end = blen;
    }
    if (sublen == 0) {
        return start;
    }

    char *bp   = b->data + start;
    char *endp = b->data + end - sublen;
    char *subp;
    char *p;

    while (bp <= endp) {
next_start:
        p    = bp;
        subp = sub->data;
        for (uint64_t i = 0; i < sublen; i++) {
            if (*p++ != *subp++) {
                bp++;
                goto next_start;
            }
        }
        return bp - b->data;
    }
    return -1;
}

char *
n00b_buffer_to_c(n00b_buf_t *b, int64_t *len_ptr)
{
    // Assumes non-mutability.
    if (len_ptr) {
        *len_ptr = b->byte_len;
    }

    return b->data;
}

// We conservatively scan the data pointer.
void
n00b_buffer_set_gc_bits(uint64_t *bitfield, void *alloc)
{
    n00b_buf_t *buf = (n00b_buf_t *)alloc;

    n00b_mark_raw_to_addr(bitfield, buf, buf->data);
}

const n00b_vtable_t n00b_buffer_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)buffer_init,
        [N00B_BI_REPR]         = (n00b_vtable_entry)buffer_repr,
        [N00B_BI_TO_STR]       = (n00b_vtable_entry)n00b_buffer_to_hex_str,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)buffer_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)buffer_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)buffer_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)buffer_lit,
        [N00B_BI_COPY]         = (n00b_vtable_entry)buffer_copy,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_buffer_add,
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_buffer_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)buffer_get_index,
        [N00B_BI_INDEX_SET]    = (n00b_vtable_entry)buffer_set_index,
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)buffer_get_slice,
        [N00B_BI_SLICE_SET]    = (n00b_vtable_entry)buffer_set_slice,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)buffer_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)buffer_view,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};

// First word really means 5th word due to the GC header.
const uint64_t n00b_pmap_first_word[2] = {0x1, 0x0800000000000000};
