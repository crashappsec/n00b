// This was built mainly to make it easier to add IO filters that
// buffer.
//
//
// If you initialize with a buffer, you're declaring that the buffer is
// mutable.
//
// Currently, this does not support multi-threaded access.
#include "n00b.h"

void
n00b_bytering_init(n00b_bytering_t *self, va_list args)
{
    int64_t     copy_len  = 0;
    char       *p         = NULL;
    int64_t     alloc_len = -1;
    n00b_str_t *str       = NULL;
    n00b_buf_t *buffer    = NULL;

    n00b_karg_va_init(args);
    n00b_kw_int64("length", alloc_len);
    n00b_kw_ptr("string", str);
    n00b_kw_ptr("buffer", buffer);

    if (str && buffer) {
        N00B_CRAISE("Cannot set 'string' and 'buffer' parameters at once.");
    }

    n00b_static_lock_init(self->internal_lock);

    if (str) {
        str      = n00b_to_utf8(str);
        copy_len = n00b_str_byte_len(str);
        p        = str->data;
    }
    if (buffer) {
        copy_len = n00b_buffer_len(buffer);
        p        = buffer->data;
    }

    if (alloc_len == -1) {
        alloc_len = copy_len;
    }

    else {
        if (alloc_len < copy_len) {
            copy_len = alloc_len;
        }
    }

    self->read_ptr = self->data;

    if (alloc_len) {
        // Always add one byte to make sure we have space to
        // accomodate the desired usable length while still allowing
        // separation between read / write pointers.
        //
        // This way, the only time they should point to the same place
        // is if the ring is empty.
        self->data      = n00b_gc_array_value_alloc(char, alloc_len + 1);
        self->alloc_len = alloc_len;
        self->write_ptr = self->data + copy_len;

        if (copy_len) {
            memcpy(self->data, p, copy_len);
        }
    }
    else {
        self->write_ptr = self->data;
    }
}

static inline int64_t
n00b_bytering_len_raw(n00b_bytering_t *l)
{
    if (l->write_ptr >= l->read_ptr) {
        return l->write_ptr - l->read_ptr;
    }
    else {
        return l->alloc_len - (l->read_ptr - l->write_ptr);
    }
}

int64_t
n00b_bytering_len(n00b_bytering_t *l)
{
    int64_t result;

    n00b_lock_acquire(&l->internal_lock);
    result = n00b_bytering_len_raw(l);
    n00b_lock_release(&l->internal_lock);

    return result;
}

static inline void
add_bytes_size_checked(n00b_bytering_t *l, char *p, int n)
{
    if (n) {
        memcpy(l->write_ptr, p, n);
        l->write_ptr += n;
    }
}

// For this call, there's expected to be enough room in t1 without
// wrapping.  But t2 may need to wrap.
static inline void
add_bytes_from_ring_size_checked(n00b_bytering_t *t1, n00b_bytering_t *t2)
{
    int t2len = n00b_bytering_len_raw(t2);
    int seg1;
    int seg2;

    if (t2->read_ptr < t2->write_ptr) {
        seg2 = t2->write_ptr - t2->data;
        seg1 = t2len - seg2;
        add_bytes_size_checked(t1, t2->read_ptr, seg1);
        add_bytes_size_checked(t1, t2->data, seg2);
    }
    else {
        add_bytes_size_checked(t1, t2->data, t2len);
    }
}

extern n00b_buf_t *
n00b_bytering_to_buffer(n00b_bytering_t *l)
{
    n00b_buf_t *result;
    int         n;

    n00b_lock_acquire(&l->internal_lock);

    result = n00b_new(n00b_type_buffer(),
                      n00b_kw("length", n00b_ka(l->alloc_len)));
    n      = l->write_ptr - l->read_ptr;
    if (n > 0) {
        memcpy(result->data, l->read_ptr, n);
        result->byte_len = n;
    }
    else {
        n = l->alloc_len - (l->read_ptr - l->data);
        memcpy(result->data, l->read_ptr, n);
        char *p          = result->data + n;
        result->byte_len = n;
        n                = l->write_ptr - l->data;
        memcpy(p, l->data, n);
        result->byte_len += n;
    }

    n00b_lock_release(&l->internal_lock);

    return result;
}

n00b_utf8_t *
n00b_bytering_to_utf8(n00b_bytering_t *l)
{
    return n00b_buf_to_utf8_string(n00b_bytering_to_buffer(l));
}

void
n00b_bytering_resize(n00b_bytering_t *l, int64_t sz)
{
    n00b_bytering_t *result;
    int              n;

    result = n00b_new(n00b_type_bytering(), n00b_kw("length", n00b_ka(sz)));

    n00b_lock_acquire(&l->internal_lock);

    n                 = n00b_min(n00b_bytering_len_raw(l), sz);
    result->read_ptr  = result->data;
    result->write_ptr = result->data + n;

    if (l->read_ptr < l->write_ptr) {
        add_bytes_size_checked(result, l->read_ptr, n);
    }
    else {
        int remaining = l->alloc_len - (l->read_ptr - l->data);
        int m         = n00b_min(remaining, n);
        add_bytes_size_checked(result, l->write_ptr, m);
        n -= m;
        add_bytes_size_checked(result, l->data, n);
    }

    n00b_lock_release(&l->internal_lock);
}

static inline char *
n00b_bytering_index_addr(n00b_bytering_t *l, int64_t ix)
{
    char *end = l->data + l->alloc_len;
    char *p   = l->data + ix;

    if (p > end) {
        p = l->data + (p - end);
    }

    return p;
}

static inline char *
n00b_bytering_end(n00b_bytering_t *l)
{
    return l->data + l->alloc_len;
}

int8_t
n00b_bytering_get_index(n00b_bytering_t *l, int64_t ix)
{
    int8_t result;
    n00b_lock_acquire(&l->internal_lock);

    int len = n00b_bytering_len_raw(l);
    if (ix < 0) {
        ix += len;
    }

    if (ix < 0 || ix >= len) {
        n00b_lock_release(&l->internal_lock);
        N00B_CRAISE("Index out of bounds.");
    }

    result = *n00b_bytering_index_addr(l, ix);

    n00b_lock_release(&l->internal_lock);

    return result;
}

void
n00b_bytering_set_index(n00b_bytering_t *l, int64_t ix, int8_t c)
{
    n00b_lock_acquire(&l->internal_lock);

    int len = n00b_bytering_len_raw(l);
    if (ix < 0) {
        ix += len;
    }

    if (ix < 0 || ix >= len) {
        N00B_CRAISE("Index out of bounds.");
    }

    *n00b_bytering_index_addr(l, ix) = c;

    n00b_lock_release(&l->internal_lock);

    return;
}

n00b_bytering_t *
bytering_internal_slice(n00b_bytering_t *l, int64_t start, int64_t end)
{
    int len = n00b_bytering_len_raw(l);

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
bounds_err:
            n00b_lock_release(&l->internal_lock);
            N00B_CRAISE("Slice out of bounds.");
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
    if ((start | end) < 0 || start > end) {
        goto bounds_err;
    }

    int              slice_len = end - start;
    n00b_bytering_t *result    = n00b_new(n00b_type_bytering(),
                                       n00b_kw("length", n00b_ka(slice_len)));

    if (l->read_ptr < l->write_ptr) {
        memcpy(result->data, l->read_ptr, slice_len);
    }
    else {
        int second_segment = l->write_ptr - l->data;
        int first_segment  = slice_len - second_segment;

        if (first_segment) {
            memcpy(result->data, l->read_ptr, first_segment);
            result->write_ptr = result->data + first_segment;
        }
        if (second_segment) {
            memcpy(result->write_ptr, l->data, second_segment);
            result->write_ptr += second_segment;
        }
    }

    return result;
}

n00b_bytering_t *
n00b_bytering_get_slice(n00b_bytering_t *l, int64_t start, int64_t end)
{
    n00b_bytering_t *result;

    n00b_lock_acquire(&l->internal_lock);
    result = bytering_internal_slice(l, start, end);
    n00b_lock_release(&l->internal_lock);

    return result;
}

static inline n00b_bytering_t *
n00b_bytering_plus_raw(n00b_bytering_t *t1, n00b_bytering_t *t2)
{
    int              l1     = n00b_bytering_len_raw(t1);
    int              l2     = n00b_bytering_len_raw(t2);
    int              len    = l1 + l2;
    n00b_bytering_t *result = n00b_new(n00b_type_bytering(),
                                       n00b_kw("length", n00b_ka(len)));

    add_bytes_from_ring_size_checked(result, t1);
    add_bytes_from_ring_size_checked(result, t2);

    return result;
}

n00b_bytering_t *
n00b_bytering_plus(n00b_bytering_t *t1, n00b_bytering_t *t2)
{
    n00b_bytering_t *result;

    if (!t1) {
        return t2;
    }
    if (!t2) {
        return t1;
    }

    n00b_lock_acquire(&t1->internal_lock);
    n00b_lock_acquire(&t2->internal_lock);

    result = n00b_bytering_plus_raw(t1, t2);

    n00b_lock_release(&t2->internal_lock);
    n00b_lock_release(&t1->internal_lock);

    return result;
}

static inline void
plus_eq_via_resize(n00b_bytering_t *t1, n00b_bytering_t *t2, int64_t needed)
{
    n00b_bytering_t *tmp = n00b_bytering_plus(t1, t2);
    t1->alloc_len        = tmp->alloc_len;
    t1->data             = tmp->data;
    t1->write_ptr        = tmp->write_ptr;
    t1->read_ptr         = tmp->read_ptr;
}

void
n00b_bytering_plus_eq(n00b_bytering_t *t1, n00b_bytering_t *t2)
{
    n00b_lock_acquire(&t1->internal_lock);
    n00b_lock_acquire(&t2->internal_lock);

    int64_t needed = n00b_bytering_len_raw(t1) + n00b_bytering_len_raw(t2);

    if (needed > t1->alloc_len) {
        plus_eq_via_resize(t1, t2, needed);
    }
    else {
        add_bytes_from_ring_size_checked(t1, t2);
    }

    n00b_lock_release(&t2->internal_lock);
    n00b_lock_release(&t1->internal_lock);
}

void
n00b_bytering_set_slice(n00b_bytering_t *l,
                        int64_t          start,
                        int64_t          end,
                        n00b_bytering_t *sub)
{
    n00b_bytering_t *tmp;
    n00b_bytering_t *slice;
    int              alloc_len;
    int              len;

    n00b_lock_acquire(&l->internal_lock);
    n00b_lock_acquire(&sub->internal_lock);

    len = n00b_bytering_len_raw(l);

    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
bounds_err:
            n00b_lock_release(&sub->internal_lock);
            n00b_lock_release(&l->internal_lock);
            N00B_CRAISE("Slice out of bounds.");
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
    if ((start | end) < 0 || start > end) {
        goto bounds_err;
    }

    alloc_len = n00b_bytering_len_raw(l) + n00b_bytering_len_raw(sub);
    tmp       = n00b_new(n00b_type_bytering(),
                   n00b_kw("length", n00b_ka(alloc_len)));

    if (start > 0) {
        slice = bytering_internal_slice(l, 0, start);
        add_bytes_from_ring_size_checked(tmp, slice);
    }

    add_bytes_from_ring_size_checked(tmp, sub);

    if (end < len) {
        slice = bytering_internal_slice(l, end, len);
        add_bytes_from_ring_size_checked(tmp, slice);
    }

    l->alloc_len = tmp->alloc_len;
    l->data      = tmp->data;
    l->write_ptr = tmp->write_ptr;
    l->read_ptr  = tmp->read_ptr;

    n00b_lock_release(&sub->internal_lock);
    n00b_lock_release(&l->internal_lock);
}

n00b_bytering_t *
n00b_bytering_copy(n00b_bytering_t *r)
{
    n00b_lock_acquire(&r->internal_lock);
    n00b_bytering_t *result = n00b_new(n00b_type_bytering(),
                                       n00b_kw("length",
                                               n00b_ka(r->alloc_len)));

    add_bytes_from_ring_size_checked(result, r);
    n00b_lock_release(&r->internal_lock);

    return result;
}

static n00b_type_t *
n00b_bytering_item_type(n00b_obj_t x)
{
    return n00b_type_byte();
}

void *
n00b_bytering_view(n00b_bytering_t *r, uint64_t *outlen)
{
    n00b_bytering_t *result = n00b_bytering_copy(r);
    *outlen                 = (uint64_t)n00b_bytering_len_raw(result);

    return result;
}

void
n00b_bytering_truncate_end(n00b_bytering_t *r, int64_t len)
{
    if (len <= 0) {
        return;
    }

    n00b_lock_acquire(&r->internal_lock);
    int cur_len = n00b_bytering_len_raw(r);

    if (cur_len <= len) {
        r->read_ptr  = r->data;
        r->write_ptr = r->data;
    }
    else {
        r->write_ptr -= len;

        if (r->write_ptr < r->data) {
            int wrap     = r->data - r->write_ptr;
            r->write_ptr = n00b_bytering_end(r) - wrap;
        }
    }

    n00b_lock_release(&r->internal_lock);
}

void
n00b_bytering_truncate_front(n00b_bytering_t *r, int64_t len)
{
    if (len <= 0) {
        return;
    }

    n00b_lock_acquire(&r->internal_lock);
    int cur_len = n00b_bytering_len_raw(r);

    if (cur_len <= len) {
        r->read_ptr  = r->data;
        r->write_ptr = r->data;
    }
    else {
        r->read_ptr += len;

        char *end = n00b_bytering_end(r);

        if (r->read_ptr > end) {
            int wrap    = r->read_ptr - end;
            r->read_ptr = r->data + wrap;
        }
    }
    n00b_lock_release(&r->internal_lock);
}

static bool
n00b_bytering_can_coerce_to(n00b_type_t *ignore, n00b_type_t *t)
{
    return (n00b_type_is_string(t) || n00b_type_is_buffer(t)
            || n00b_type_is_bool(t) || n00b_type_is_bytering(t));
}

static n00b_obj_t
n00b_bytering_coerce_to(n00b_bytering_t *r, n00b_type_t *t)
{
    n00b_obj_t result = NULL;

    n00b_lock_acquire(&r->internal_lock);

    if (n00b_type_is_bytering(t)) {
        result = r;
    }
    if (n00b_type_is_string(t)) {
        result = n00b_bytering_to_utf8(r);
    }
    if (n00b_type_is_buffer(t)) {
        result = n00b_bytering_to_buffer(r);
    }
    if (n00b_type_is_bool(t)) {
        if (!r || !n00b_bytering_len_raw(r)) {
            result = (n00b_obj_t) false;
        }
        else {
            result = (n00b_obj_t) true;
        }
    }

    n00b_lock_release(&r->internal_lock);

    return result;
}

static void
n00b_bytering_write_raw(n00b_bytering_t *r, char *ptr, size_t bytes)
{
    size_t l = r->alloc_len - 1;

    if (bytes > l) {
        // Skip the bytes that will immediately get overwritten before
        // we unlock.
        size_t diff = bytes - l;
        ptr += diff;
        bytes = l;
    }

    add_bytes_size_checked(r, ptr, bytes);
}

void
n00b_bytering_write_buffer(n00b_bytering_t *r, n00b_buf_t *b)
{
    n00b_lock_acquire(&r->internal_lock);
    n00b_buf_t *n = n00b_copy(b);
    n00b_bytering_write_raw(r, n->data, n->byte_len);
    n00b_lock_release(&r->internal_lock);
}

void
n00b_bytering_write_string(n00b_bytering_t *r, n00b_str_t *s)
{
    n00b_lock_acquire(&r->internal_lock);
    s = n00b_to_utf8(s);
    n00b_bytering_write_raw(r, s->data, n00b_str_byte_len(s));
    n00b_lock_release(&r->internal_lock);
}

void
n00b_bytering_write_bytering(n00b_bytering_t *dst, n00b_bytering_t *src)
{
    ptrdiff_t to_copy;
    n00b_lock_acquire(&dst->internal_lock);
    n00b_lock_acquire(&src->internal_lock);

    if (src->read_ptr < src->write_ptr) {
        to_copy = src->write_ptr - src->read_ptr;
        n00b_bytering_write_raw(dst, src->read_ptr, to_copy);
    }
    else {
        char *end = n00b_bytering_end(src);
        to_copy   = end - src->read_ptr;
        n00b_bytering_write_raw(dst, src->read_ptr, to_copy);
        to_copy = src->write_ptr - src->data;
        n00b_bytering_write_raw(dst, src->data, to_copy);
    }

    n00b_lock_release(&src->internal_lock);
    n00b_lock_release(&dst->internal_lock);
}

// Write a string, buffer or bytering into an existing object at the
// write head, potentially dropping content.
void
n00b_bytering_write(n00b_bytering_t *r, void *obj)
{
    n00b_type_t *t = n00b_get_my_type(obj);

    if (n00b_type_is_string(t)) {
        n00b_bytering_write_string(r, obj);
        return;
    }
    if (n00b_type_is_buffer(t)) {
        n00b_bytering_write_buffer(r, obj);
        return;
    }
    if (n00b_type_is_bytering(t)) {
        n00b_bytering_write_bytering(r, obj);
        return;
    }
    N00B_CRAISE(
        "Object must be a string, buffer or bytering to be written "
        "into a bytering.");
}

static n00b_obj_t
n00b_bytering_literal(n00b_utf8_t          *su8,
                      n00b_lit_syntax_t     st,
                      n00b_utf8_t          *lu8,
                      n00b_compile_error_t *err)
{
    char       *s      = su8->data;
    char       *litmod = lu8->data;
    n00b_buf_t *tbuf;

    if (!strcmp(litmod, "h") || !strcmp(litmod, "hex")) {
        int length = strlen(s);
        if (length & 2) {
            *err = n00b_err_parse_lit_odd_hex;
            return NULL;
        }
        tbuf = n00b_new(n00b_type_buffer(),
                        n00b_kw("length", n00b_ka(length), "hex", n00b_ka(s)));
    }
    else {
        tbuf = n00b_new(n00b_type_buffer(), n00b_kw("raw", n00b_ka(s)));
    }

    n00b_bytering_t *res = n00b_new(n00b_type_bytering(),
                                    n00b_kw("length", n00b_ka(0)));

    res->alloc_len = tbuf->byte_len;
    res->data      = tbuf->data;
    res->read_ptr  = tbuf->data;
    res->write_ptr = tbuf->data + tbuf->byte_len;

    return res;
}

// For now, just convert to a buffer.
static n00b_utf8_t *
n00b_bytering_to_str(n00b_bytering_t *r)
{
    return n00b_to_str(n00b_bytering_to_buffer(r), n00b_type_buffer());
}

static n00b_utf8_t *
n00b_bytering_repr(n00b_bytering_t *r)
{
    return n00b_repr(n00b_bytering_to_buffer(r));
}

// This does not grab the lock, and assumes we are iterating internally
// when we know we have the lock.
void
n00b_bytering_advance_ptr(n00b_bytering_t *r, char **ptrp)
{
    char *ptr = (*ptrp) + 1;

    if (ptr == n00b_bytering_end(r)) {
        ptr = r->data;
    }

    *ptrp = ptr;
}

const n00b_vtable_t n00b_bytering_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)n00b_bytering_init,
        [N00B_BI_REPR]         = (n00b_vtable_entry)n00b_bytering_repr,
        [N00B_BI_TO_STR]       = (n00b_vtable_entry)n00b_bytering_to_str,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)n00b_bytering_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)n00b_bytering_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)n00b_bytering_literal,
        [N00B_BI_COPY]         = (n00b_vtable_entry)n00b_bytering_copy,
        [N00B_BI_ADD]          = (n00b_vtable_entry)n00b_bytering_plus,
        [N00B_BI_LEN]          = (n00b_vtable_entry)n00b_bytering_len,
        [N00B_BI_INDEX_GET]    = (n00b_vtable_entry)n00b_bytering_get_index,
        [N00B_BI_INDEX_SET]    = (n00b_vtable_entry)n00b_bytering_set_index,
        [N00B_BI_SLICE_GET]    = (n00b_vtable_entry)n00b_bytering_get_slice,
        [N00B_BI_SLICE_SET]    = (n00b_vtable_entry)n00b_bytering_set_slice,
        [N00B_BI_ITEM_TYPE]    = (n00b_vtable_entry)n00b_bytering_item_type,
        [N00B_BI_VIEW]         = (n00b_vtable_entry)n00b_bytering_view,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        NULL,
    },
};
