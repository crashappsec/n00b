#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This will, as a filter, marshal a stream of objects passed to it.
// The code follows all memory references in those objects and copies
// anything indirectly referenced, and maps pointers to a an abstract
// heap, make it easy to unmarshal directly into memory with just some
// pointer rewrites.
//
// The stream keeps a memory of all pointers it rewrote, and future
// instances of that pointer in the stream, always point to the same
// place in the 'fake heap'.
//
// As an example, if you start a new stream where you add object X to
// it, and it references objects A, B and C, the stream produced will
// bundle up all three of those memory allocations and emit them, with
// pointers rewritten to point to consistent places in a 'virtual'
// heap mapped to the stream.
//
// If you now add object Y to the stream, which ALSO references object
// C, it does not re-marshal object C, it converts the reference to
// the same virtual heap reference.
//
// As of right now, this is only being used for STATIC marshalling use
// cases. I urge you to treat it that way; we're turning on the GIL
// for marshals each time right now, but I may want to turn that off.
//
// For the buffered output allocations, we do our own memory
// management so that we don't contribute to GC bloat (using our heap
// interface).
//
// But, we do allocate some dynamic records in the primary shared heap
// for the memoization (and with some tiny probability for backpatches
// when there's a collision between a value and our fake address
// space). So there's some risk to locking the heap so that it cannot
// collect. To address this, we also ask the GC ensure that it has 50%
// of its heap available when we start. If we don't, the GC collects,
// and if it still doesn't, migrates itself into a heap 2x the size.
//
// It's still possible given other threads that we'll get to a place
// that we'll barf due to not being able to allocate, in which case,
// for the time being, this is the same as if there's a system OOM
// problem-- we're going to end up with a crash (will fix that
// eventually).
//
// The way the algorithm works is by translating all pointers to a
// 'virtual' pointer in a heap that doesn't exist, in such a way that
// the pointer's OFFSET from that imaginary heap's beginning maps to
// the offset from the start of the marshal'd stream.
//
// We use the virtual heap instead of raw offsets for ... reasons:
//
// 1. We want the automarshal memory layout to be valid memory
//    layout when unmarshaling (all we should need to do are
//    fix-ups of allocation headers and pointer rewrites).  So we
//    don't want to add type info to each word; we want it to be
//    clear what's a pointer or not.
//
// 2. We want to be able to use streams for marshal / unmarshal that
//    do not need to seek backwards, since that sometimes is not
//    possible. So we cannot put info at the front of the marshal
//    output that we won't have until the end (which would be one way
//    to help address the previous requirement).
//
// Therefore, our rough approach is to take things that are
// pointers, and convert them to an address in our fake heap.  Of
// course, values may still collide with our fake heap, though the
// effect should be minimal given enough randomization.
//
// As with everything in N00b, we require pointers to be
// word-aligned, and require all our allocations to be so as well.
//
// To deal with the rare collision, we:
//
// a) Generate a random number to use for the virtual heap, that
//    is the 32 most significant bits of a pointer.
// b) When a (non-pointer or possible pointer) value in memory we're
//    processing does collide, we write out a ZERO in the memory
//    address where that pointer lives, but record the offset and
//    value.
// c) If there are any collisions, when the end of all the allocations
//    in the marshal'd stream are reached, we output the list of
//    necessary patches, for each, writing the 'virtual heap offset'
//    followed by the value to write.
//
// Note that we treat EVERYTHING as little endian. We're only ever
// targeting little endian, but we explicitly marshal 64 bits at a
// time which *will* swap if we're ever on a 64-bit machine.
//
//
// Currently, this all ignores any actual finalizers; my intent for
// handling this is to have a callback for the toplevel object
// that can be used to auto-re-initialize. But that's a TODO item.
//
// Also, we currently are treating anything that looks like a valid
// heap address as a pointer. This is mainly because, while the GC is
// capable to tracking the difference, I'm not using it yet, and
// planning on making some eventual changes.
//
//
// TODO:
// - GC work
// - Add marshal errors

static inline uint64_t
lsb_erase(uint64_t n)
{
    return n & 0xffffffffffff0000ull;
}

static inline int64_t
msb_erase(uint64_t n, n00b_unpickle_ctx *ctx)
{
    return n - ctx->vaddr_start;
}

static int64_t translate_pointer(n00b_pickle_ctx *ctx, void *p);

#if defined(N00B_ALLOC_LOC_INF0)
#define N00B_MARSHAL_OPT_1 0x01
#else
#define N00B_MARSHAL_OPT_1 0x00
#endif

#if defined(N00B_FULL_MEMCHECK)
#define N00B_MARSHAL_OPT_2 0x02
#else
#define N00B_MARSHAL_OPT_2 0x00
#endif

#define N00B_MAGIC_FLIPS (N00B_MARSHAL_OPT_1 | N00B_MARSHAL_OPT_2)

static inline uint64_t
n00b_get_marshal_magic(void)
{
    return (N00B_MARSHAL_MAGIC_BASE & 0xffffffffffffff00)
         | ((N00B_MAGIC_FLIPS) ^ N00B_MARSHAL_MAGIC_BASE);
}

static inline bool
check_magic(uint64_t found)
{
    return found == n00b_get_marshal_magic();
}

static void
new_local_heap(n00b_pickle_ctx *ctx, size_t len)
{
    int tlen = n00b_max(len + sizeof(n00b_alloc_hdr),
                        N00B_MARSHAL_CHUNK_SIZE);

    if (ctx->current_output_buf) {
        n00b_list_append(ctx->buffered_heap_info,
                         ctx->current_output_buf->data);
        n00b_list_append(ctx->buffered_heap_info,
                         (void *)ctx->offset);
    }
    else {
        ctx->buffered_heap_info = n00b_list(n00b_type_ref());
    }

    ctx->current_output_buf = n00b_new(n00b_type_buffer(), length : tlen);

    ctx->current_output_buf->byte_len = 0;
}

static void *
local_heap_alloc(n00b_pickle_ctx *ctx, size_t len)
{
    int         tlen = len; // + sizeof(n00b_alloc_hdr);
    n00b_buf_t *b    = ctx->current_output_buf;

    if (!b || b->byte_len + tlen > b->alloc_len) {
        return NULL;
    }

    char *p = b->data + b->byte_len;

    b->byte_len += tlen;
    return p;
}

static int64_t translate_pointer(n00b_pickle_ctx *, void *);

#if defined(N00B_ADD_ALLOC_LOC_INFO)
static char *
copy_alloc_filename(n00b_pickle_ctx *ctx, char *fname)
{
    return NULL;
    n00b_string_t *as_n00b = n00b_dict_get(ctx->file_cache, fname, NULL);

    if (!as_n00b) {
        as_n00b = n00b_cstring(fname);
        n00b_dict_put(ctx->file_cache, fname, as_n00b);
    }

    return (char *)translate_pointer(ctx, (char *)as_n00b->data);
}
#endif

static inline void
setup_new_header(n00b_pickle_ctx *ctx, n00b_marshaled_hdr *h, n00b_alloc_hdr *s)
{
    h->empty_guard   = N00B_MARSHAL_RECORD_GUARD;
    h->alloc_len     = s->alloc_len;
    h->n00b_ptr_scan = s->n00b_ptr_scan;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
    // TODO: Fix these.
    h->alloc_file = copy_alloc_filename(ctx, s->alloc_file);
    h->alloc_line = 0xeeee; // s->alloc_line;
#endif

    h->type           = (void *)translate_pointer(ctx, s->type);
    h->n00b_obj       = s->n00b_obj;
    h->cached_hash[0] = ((n00b_marshaled_hdr *)s)->cached_hash[0];
    h->cached_hash[1] = ((n00b_marshaled_hdr *)s)->cached_hash[1];

    // Convert endianness on big endian machines; should not generate
    // any code at all most places.
    little_32(h->alloc_len);

#if BYTE_ORDER == BIG_ENDIAN
    uint64_t *p = (uint64_t *)&h->cached_hash;
    little_64(*p[0]);
    little_64(*p[1]);
#endif
}

// Alloc one internal record big enough to copy a record we've found,
// passing the current offset in the 3rd param.
static n00b_alloc_hdr *
get_write_record(n00b_pickle_ctx *ctx, n00b_alloc_hdr *hdr, int64_t *offset_ptr)
{
    bool    found;
    int64_t offset = (int64_t)n00b_dict_get(ctx->memos, hdr, &found);

    if (found) {
        *offset_ptr = offset;
        return NULL;
    }
    else {
        offset = ctx->offset;
    }

    *offset_ptr = offset;

    if ((hdr->alloc_len * 2) >= N00B_MARSHAL_CHUNK_SIZE) {
        new_local_heap(ctx, hdr->alloc_len * 4);
    }

    n00b_alloc_hdr *h = local_heap_alloc(ctx, hdr->alloc_len);

    if (!h) {
        new_local_heap(ctx, hdr->alloc_len * 4);
        h = local_heap_alloc(ctx, hdr->alloc_len);
    }

    ptrdiff_t record_size = hdr->alloc_len;
    n00b_dict_put(ctx->memos, hdr, (void *)offset);

    ctx->offset += record_size;

    setup_new_header(ctx, (n00b_marshaled_hdr *)h, hdr);

    return h;
}

static void
enqueue_write_record(n00b_pickle_ctx *ctx,
                     n00b_alloc_hdr  *src,
                     n00b_alloc_hdr  *dst,
                     int64_t          alloc_offset)
{
    n00b_pickle_wl_item_t *item = n00b_gc_alloc_mapped(n00b_pickle_wl_item_t,
                                                       N00B_GC_SCAN_ALL);
    item->src                   = src;
    item->dst                   = dst;
    item->cur_offset            = alloc_offset;
    item->next                  = ctx->wl;
    ctx->wl                     = item;
}

static inline void
handle_collision(n00b_pickle_ctx *ctx, uint64_t val, int64_t o)
{
    if (!ctx->needed_patches) {
        ctx->needed_patches = n00b_dict(n00b_type_u64(),
                                        n00b_type_u64());
    }

    n00b_dict_put(ctx->needed_patches, (void *)o, (void *)val);
}

static int64_t
translate_pointer(n00b_pickle_ctx *ctx, void *p)
{
    if (!n00b_in_heap(p)) {
        return (int64_t)p;
    }

    n00b_alloc_hdr *src_hdr = n00b_find_allocation_record(p);

    if (!src_hdr) {
        return (int64_t)p;
    }

    int64_t         offset;
    n00b_alloc_hdr *dst_hdr = get_write_record(ctx, src_hdr, &offset);

    if (dst_hdr) {
        n00b_assert(dst_hdr->guard == N00B_MARSHAL_RECORD_GUARD);
        enqueue_write_record(ctx, src_hdr, dst_hdr, offset);
    }

    int64_t diff = ((char *)p) - (char *)src_hdr;

    return diff + offset;
}

static inline int64_t
process_one_word(n00b_pickle_ctx *ctx, uint64_t *s, int64_t offset)
{
    uint64_t val = *s;

    if (lsb_erase(val) == ctx->base) {
        handle_collision(ctx, val, offset);
        return 0;
    }

    return translate_pointer(ctx, (void *)val);
}

static inline void
process_queue_item(n00b_pickle_ctx *ctx, n00b_pickle_wl_item_t *item)
{
    uint64_t *sptr      = item->src->data;
    uint64_t *dptr      = item->dst->data;
    uint64_t  num_words = (item->src->alloc_len - sizeof(n00b_alloc_hdr)) / 8;
    int64_t   offset    = item->cur_offset + sizeof(n00b_alloc_hdr);
    uint64_t *end       = &((uint64_t *)((unsigned char *)sptr))[num_words];
    n00b_assert((((char *)sptr) - (char *)item->src) == sizeof(n00b_alloc_hdr));
    n00b_assert((((char *)dptr) - (char *)item->dst) == sizeof(n00b_alloc_hdr));
    n00b_assert(item->src->guard == n00b_gc_guard);
    n00b_assert(item->dst->guard == N00B_MARSHAL_RECORD_GUARD);

    while (sptr < end) {
        *dptr = process_one_word(ctx, sptr, offset);
        sptr++;
        dptr++;
        offset++;
    }
}

static inline void
process_queue(n00b_pickle_ctx *ctx)
{
    n00b_pickle_wl_item_t *item = ctx->wl;

    while (item) {
        ctx->wl    = item->next;
        // This isn't really necesssary, but the point is we shouldn't follow
        // the "next" field later, because processing can push more things onto
        // the queue, which is currently a stack, not a fifo.
        item->next = NULL;
        process_queue_item(ctx, item);
        item = ctx->wl;
    }
}

static n00b_alloc_hdr *
create_object_end_record(n00b_pickle_ctx *ctx)
{
    uint32_t  n = 0;
    uint64_t *patches;
    int64_t   saved_offset = ctx->offset;
    int64_t   ignored_offset;

    // The end record consists of a memory record where there is no
    // 'next' pointer, along with any backpatches that are necessary
    // due to data colliding with our 'fake' heap addresses.
    //
    // To get this into the output stream, we allocate either the
    // patches or an unused allocation, and allocate it as if it were
    // properly referenced by the object we're marshaling. Then,
    // instead of sticking it on the worklist that we've already
    // finished with, we manually copy in what we need.
    //
    // Once done with that, we need to fix up the state of the
    // context, because the object should not be considered part of
    // the virtual address space. So we reset the context's offset to
    // the allocation start (it won't get overwritten; the next
    // message will pick up in a new heap).
    //
    // We also explicitly mark the next offset as NULL to indicate on
    // the receive side that this is an object-end record.
    //
    // Note that we only add these records in a stream at the boundary
    // of where we actually made write() calls (per-object).

    if (ctx->needed_patches) {
        patches = (uint64_t *)n00b_dict_items_array(ctx->needed_patches, &n);
    }
    else {
        patches             = n00b_gc_alloc_mapped(uint64_t, N00B_GC_SCAN_NONE);
        ctx->needed_patches = NULL; // Shouldn't be needed.
    }

    n00b_alloc_hdr *p_hdr = (n00b_alloc_hdr *)patches;
    p_hdr                 = p_hdr - 1;
    n00b_alloc_hdr *rec   = get_write_record(ctx, p_hdr, &ignored_offset);

    rec->alloc_len = n * 16; // Byte len of patches.
    ctx->offset    = saved_offset;

    if (n) {
        memcpy(rec->data, patches, n * sizeof(uint64_t));
        ctx->needed_patches = NULL;
    }

    rec->n00b_marshal_end = true;

    return rec;
}

static n00b_buf_t *
bundle_result(n00b_pickle_ctx *ctx, n00b_alloc_hdr *end_record)
{
    // Note that, to allocate space, we need to copy over what we've
    // used of the virtual heap, so measure between last_offset_end
    // and the current offset.
    //
    // Plus, we need to add in enough space for the object end record,
    // and if this is the first thing output to this stream, we need
    // to write the two-word header (a magic value and the random base
    // address).

    int64_t      to_alloc = ctx->offset - ctx->last_offset_end;
    int64_t      start    = ctx->last_offset_end;
    int64_t      end      = ctx->offset;
    int64_t      local_end;
    int64_t      to_copy;
    n00b_heap_t *heap;
    n00b_buf_t  *b;
    char        *p;

    ctx->last_offset_end = end;
    to_alloc += sizeof(n00b_alloc_hdr);

    if (!ctx->started) {
        to_alloc += sizeof(uint64_t) * 2;
    }

    to_alloc += (sizeof(uint64_t) * end_record->alloc_len);

    b           = n00b_new(n00b_type_buffer(), length : to_alloc);
    b->byte_len = to_alloc;
    p           = b->data;

    // If this is the first write to the stream, add the magic value
    // and the heap start address.
    if (!ctx->started) {
        *((uint64_t *)p) = n00b_get_marshal_magic();
        p += sizeof(uint64_t);
        *((uint64_t *)p) = ctx->base;
        p += sizeof(uint64_t);
        ctx->started = true;
    }

    // If we used more than one internal heap, we need to deal w/ previous
    // heaps.
    if (ctx->buffered_heap_info) {
        while (n00b_list_len(ctx->buffered_heap_info)) {
            heap      = n00b_list_dequeue(ctx->buffered_heap_info);
            local_end = (int64_t)n00b_list_dequeue(ctx->buffered_heap_info);
            to_copy   = local_end - start;

            memcpy(p, heap, to_copy);

            p     = p + to_copy;
            start = local_end;
        }
        ctx->buffered_heap_info = NULL;
    }

    to_copy = end - start;

    memcpy(p, ctx->current_output_buf->data, to_copy);

    p = p + to_copy;

    memcpy(p, end_record, sizeof(n00b_alloc_hdr));

    p = p + sizeof(n00b_alloc_hdr);

    if (end_record->alloc_len) {
        to_copy = end_record->alloc_len * sizeof(uint64_t);

        memcpy(p, end_record->data, to_copy);

        p = p + to_copy;
    }

    ctx->current_output_buf = NULL;

    n00b_assert(b->byte_len == to_alloc);
    n00b_assert(!(b->byte_len % 16));
    return b;
}

static uint64_t
n00b_get_virtual_heap_start(void)
{
    // This technically has a problem where, while we check for heap
    // collisions now, we don't keep checking as the GC collects and
    // moves heaps. For this reason, we start by taking a 60-bit
    // random value (the bottom 4 bits are 0 as traditional for
    // pointer alignment).
    //
    // Then, we mask out a full 16 bits. If that's in the current
    // heap, or if the highest address with the same 48-bit prefix is
    // in the heap, we retry.
    //
    // Depending on the size of the ACTUAL heap, ideally a collision
    // isn't particularly practical. Eventually we should register
    // these virtual heaps with the GC so that it can avoid the
    // somewhat improbable collisions.

    while (true) {
        uint64_t result     = n00b_rand64() & ~0x0fULL;
        uint64_t low_check  = result & 0xffffffffffff0000ULL;
        uint64_t high_check = result | 0xffffULL;

        if (!n00b_in_heap((void *)low_check)
            && !n00b_in_heap((void *)high_check)) {
            result &= ~0xffff;
            return result;
        }
    }
}

static inline void
n00b_receive_unpickle_info(n00b_unpickle_ctx *ctx, n00b_buf_t *inbuf)
{
#ifdef N00B_DEBUG
    int dbg_old = 0;
    int dbg_new = n00b_buffer_len(inbuf);
#endif

    if (!ctx->partial) {
        ctx->partial     = inbuf;
        ctx->part_cursor = inbuf->data;
    }
    else {
#ifdef N00B_DEBUG
        dbg_old = n00b_buffer_len(ctx->partial);
#endif

        // The add op will probably resize the thing.
        int offset       = ctx->part_cursor - ctx->partial->data;
        ctx->partial     = n00b_buffer_add(ctx->partial, inbuf);
        ctx->part_cursor = ctx->partial->data + offset;

        n00b_assert(dbg_old + dbg_new == n00b_buffer_len(ctx->partial));
    }

    return;
}

static inline n00b_buf_t *
n00b_slice_one_pickle(n00b_unpickle_ctx *ctx, char *end)
{
    int64_t     slice_len = end - ctx->partial->data;
    int64_t     blen      = n00b_buffer_len(ctx->partial);
    n00b_buf_t *result;

    // If ctx->partial is exactly the right length, we can just use it
    // directly; just fix up the accounting.

    if (blen == slice_len) {
        result           = ctx->partial;
        ctx->partial     = NULL;
        ctx->part_cursor = NULL;
    }
    else {
        result           = n00b_slice_get(ctx->partial, 0, slice_len);
        ctx->partial     = n00b_slice_get(ctx->partial, slice_len, blen);
        ctx->part_cursor = ctx->partial->data;

        n00b_assert(ctx->partial->byte_len == blen - slice_len);
    }

    return result;
}

static inline n00b_buf_t *
n00b_check_unpickle_readiness(n00b_unpickle_ctx *ctx)
{
    if (!ctx->partial) {
        return NULL;
    }

    int             buflen  = n00b_buffer_len(ctx->partial);
    char           *buf_end = ctx->partial->data + buflen;
    n00b_alloc_hdr *h;

    while (true) {
        h = (n00b_alloc_hdr *)ctx->part_cursor;

        // If there's not a new header's worth of data read,
        // we can't be ready to unpickle a segment (we need to get
        // all the way through an end record).
        if (ctx->part_cursor + sizeof(n00b_alloc_hdr) > buf_end) {
            return NULL;
        }

        // If the 'guard' field does not contain the marshal guard constant,
        // then we know we've got corrupt data.
        if (h->guard != N00B_MARSHAL_RECORD_GUARD) {
            abort();
            N00B_CRAISE("Corrupt data stream when unmarshaling.");
        }

        if (h->n00b_marshal_end) {
            int   needed_bytes = h->alloc_len;
            char *expected_end = ((char *)h->data) + needed_bytes;

            if (buf_end >= expected_end) {
                // This extracts the same.
                return n00b_slice_one_pickle(ctx, expected_end);
            }

            // Still waiting on data.
            return NULL;
        }

        ctx->part_cursor += h->alloc_len;
    }
}

static inline n00b_alloc_hdr *
find_end_record(n00b_buf_t *buf)
{
    // The end record grows in 16 byte chunks, one for each backpatch.
    char *p = buf->data + n00b_buffer_len(buf) - sizeof(n00b_alloc_hdr);

    while (true) {
        n00b_alloc_hdr *h = (void *)p;

        if (h->guard == N00B_MARSHAL_RECORD_GUARD) {
            n00b_assert(h->n00b_marshal_end);
            return h;
        }
        p--;
    }
}

static void *
n00b_raw_unmunge_pointer(n00b_unpickle_ctx *ctx, void *vptr)
{
    // Input to this function must be in the virtual address space.
    // It's seperated out from n00b_safe_unmunge_pointer because when
    // we bulk-process data, we will check outside this function, and
    // skip over writes for non-pointers.

    int64_t offset = msb_erase((uint64_t)vptr, ctx);

    // This checks for pointers within the current allocation before
    // scanning through all previous allocations.
    if (offset < 0 || ((uint64_t)offset) >= ctx->cur_record_start_offset) {
        if (((uint64_t)offset) > ctx->cur_record_end_offset) {
            N00B_CRAISE("Invalid pointer marshaled.");
        }

        return ctx->cur_record + offset - ctx->cur_record_start_offset;
    }

    // Could do a binary search, but KISS.
    int n = n00b_list_len(ctx->vaddr_info);
    for (int i = 0; i < n; i++) {
        n00b_vaddr_map_t *entry = n00b_list_get(ctx->vaddr_info, i, NULL);
        if (((uint64_t)offset) < entry->vaddr_end_offset) {
            return entry->true_segment_loc + offset - entry->vaddr_start_offset;
        }
    }
    n00b_unreachable();
}

static inline void *
n00b_safe_unmunge_pointer(n00b_unpickle_ctx *ctx, void *vptr)
{
    // Safe in this context means it accepts inputs outside the
    // virtual address space.
    if (lsb_erase((uint64_t)vptr) != ctx->vaddr_start) {
        return vptr;
    }

    return n00b_raw_unmunge_pointer(ctx, vptr);
}

static inline void
n00b_munge_user_data(n00b_unpickle_ctx *ctx, n00b_alloc_hdr *record)
{
    uint64_t *cur      = record->data;
    uint64_t  word_len = record->alloc_len / 8;
    uint64_t *end      = cur + word_len;

    while (cur != end) {
        uint64_t word = *cur;

        if (lsb_erase(word) == ctx->vaddr_start) {
            *cur = (uint64_t)n00b_raw_unmunge_pointer(ctx, (void *)word);
        }

        cur++;
    }
}

static inline void
n00b_unmarshal_records(n00b_unpickle_ctx *ctx, n00b_alloc_hdr *end_record)
{
    n00b_alloc_hdr *hdr = (void *)ctx->cur_record;

    // The format gets checked before calling this, below in
    // n00b_check_unpickle_readiness.
    while (hdr->alloc_len && hdr < end_record) {
        n00b_assert(hdr->guard == N00B_MARSHAL_RECORD_GUARD);
        hdr->guard = n00b_gc_guard;
        hdr->type  = n00b_safe_unmunge_pointer(ctx, hdr->type);
#if defined(N00B_ADD_ALLOC_LOC_INFO)
        hdr->alloc_file = n00b_safe_unmunge_pointer(ctx, hdr->alloc_file);
#endif
        n00b_munge_user_data(ctx, hdr);

        hdr = (void *)(((char *)hdr) + hdr->alloc_len);
    }
}

static inline void
n00b_perform_unmarshal_backpatching(n00b_unpickle_ctx *ctx, n00b_alloc_hdr *end)
{
    int       n          = end->alloc_len / 16;
    uint64_t *cur_record = end->data;

    while (n) {
        uint64_t offset = *cur_record++;
        uint64_t value  = *cur_record++;

        if (offset < ctx->cur_record_start_offset
            || offset > ctx->cur_record_end_offset) {
            N00B_CRAISE("Bad backpatch when unmarshaling.");
        }

        offset -= ctx->cur_record_start_offset;

        char     *p         = ctx->cur_record + offset;
        uint64_t *patch_loc = (uint64_t *)p;

        *patch_loc = value;
    }
}

static inline uint64_t
calculate_end_alloc_len(n00b_alloc_hdr *start_alloc, n00b_alloc_hdr *end_rec)
{
    char *true_end = ((char *)start_alloc) + start_alloc->alloc_len;

    return true_end - (char *)end_rec->data;
}

static inline void
n00b_fixup_edge_records(n00b_unpickle_ctx *ctx,
                        n00b_buf_t        *b,
                        n00b_alloc_hdr    *end_record)
{
    // First, find the original memory header for the buffer's
    // payload.
    n00b_alloc_hdr *buf_record = (void *)ctx->cur_record;
    buf_record--;
    //
    // Let's start with fixing up the end record, which holds patch info,
    // if anything.

    end_record->guard            = n00b_gc_guard;
    end_record->alloc_len        = calculate_end_alloc_len(buf_record,
                                                    end_record);
    end_record->n00b_obj         = false;
    end_record->n00b_finalize    = false;
    end_record->n00b_marshal_end = false;

    buf_record->alloc_len = 0;
    // Finally, let's neuter the actual buffer passed in, partially to
    // avoid unnecessary dangling references.
    b->alloc_len          = 0;
    b->byte_len           = 0;
    b->data               = NULL;
}

static inline void
n00b_record_vaddr_segment(n00b_unpickle_ctx *ctx)
{
    n00b_vaddr_map_t *record   = n00b_gc_alloc_mapped(n00b_vaddr_map_t,
                                                    N00B_GC_SCAN_ALL);
    record->vaddr_start_offset = ctx->cur_record_start_offset;
    record->vaddr_end_offset   = ctx->cur_record_end_offset;
    record->true_segment_loc   = ctx->cur_record;

    ctx->cur_record_start_offset = ctx->cur_record_end_offset;

    n00b_list_append(ctx->vaddr_info, record);
}

static void *
n00b_unpickle_one(n00b_unpickle_ctx *ctx, n00b_buf_t *buf)
{
    // We have a single buffer that we are going to break into
    // allocation records. The allocation records are, due to the
    // marshal magic, guaranteed to match our existing memory layout.
    // However, we need to do the following:
    //
    //
    // 1) Fix all allocation header info to match what we'd expect for
    //    local allocations (e.g., setting the appropriate guards, and
    //    translating the type info and, if appropriate, file name
    //    reference.
    //
    // 2) Translate 'virtual' addresses into heap addresses. Note that
    //    we use a contiguous address space per unmarshaled object,
    //    but there can be pointers across objects (always backwards
    //    pointers). Therefore, while the virtual address space used
    //    in marshaling is contiguous, we need to do some work to
    //    munge properly.
    //
    // 3) Backpatch any 'collisions' that occured while marshaling the
    //    object. Collisions should be very rare-- they occur when
    //    non-pointer data looks like it could be a pointer inside our
    //    virtual heap. When we're marshaling such things, we zero-out
    //    the data, and add a backpatch entry, which constitues an
    //    offset to patch, and the original value.
    //
    // 4) Properly update the heap's allocation record linked list;
    //    the buffer's 'next' record should look like it was 0-length,
    //    and the object-end record should link to whatever the buffer
    //    was linking to. Their allocation records should be
    //    appropriately updated.
    //
    // We also need to record this memory segment in the address info
    // list we use to map virtual addresses to actual ones, and
    // properly update 'last_offset'.

    n00b_alloc_hdr *end_record = find_end_record(buf);

    ctx->cur_record_start_offset = ctx->cur_record_end_offset;
    ctx->cur_record_end_offset += ((char *)end_record) - buf->data;
    ctx->cur_record = buf->data;

    n00b_unmarshal_records(ctx, end_record);
    n00b_perform_unmarshal_backpatching(ctx, end_record);
    n00b_fixup_edge_records(ctx, buf, end_record);
    n00b_record_vaddr_segment(ctx);

    n00b_alloc_hdr *rec = (n00b_alloc_hdr *)ctx->cur_record;

    return rec->data;
}

static void *
marshal_setup(n00b_marshal_filter_t *c, int64_t opts)
{
    if (opts) {
        c->pickle.memos           = n00b_dict(n00b_type_ref(),
                                    n00b_type_u64());
        c->pickle.base            = n00b_get_virtual_heap_start();
        c->pickle.offset          = c->pickle.base;
        c->pickle.last_offset_end = c->pickle.base;

#if defined(N00B_ADD_ALLOC_LOC_INFO)
        c->pickle.file_cache = n00b_new(n00b_type_ref(),
                                        n00b_type_ref(),
                                        cstring : true,
                                                  mmap : true);
        n00b_lock_init(&c->pickle.lock, N00B_NLT_MUTEX);
#endif
    }
    else {
        c->unpickle.vaddr_info = n00b_list(n00b_type_ref());
        n00b_lock_init(&c->unpickle.lock, N00B_NLT_MUTEX);
    }

    return NULL;
}
static n00b_list_t *
n00b_internal_marshal(n00b_pickle_ctx *ctx, void *addr)
{
    int64_t         offset;
    n00b_alloc_hdr *src = n00b_object_header(addr);
    n00b_alloc_hdr *dst = get_write_record(ctx, src, &offset);

    if (!dst) {
        // Record was already written, so we added nothing.
        return NULL;
    }

    n00b_stop_the_world();
    n00b_assert(dst->guard == N00B_MARSHAL_RECORD_GUARD);
    enqueue_write_record(ctx, src, dst, offset);
    process_queue(ctx);
    n00b_restart_the_world();

    n00b_alloc_hdr *end = create_object_end_record(ctx);
    n00b_buf_t     *b   = bundle_result(ctx, end);

    if (!b) {
        return NULL;
    }

    n00b_list_t *result = n00b_list(n00b_type_buffer());
    n00b_list_append(result, b);

    return result;
}

static n00b_list_t *
incremental_marshal(n00b_pickle_ctx *ctx, void *obj)
{
    if (!n00b_in_heap(obj)) {
        obj = n00b_box_u64((uint64_t)obj);
    }
    else {
        // Adjust to ensure the object starts at a record boundary.
        // Basically, if it's not, we create a new record, and copy
        // out the single word being pointed to.
        n00b_alloc_hdr *hdr = n00b_object_header(obj);

        if (hdr->guard != n00b_gc_guard) {
            void **tmp = n00b_gc_alloc_mapped(void **, N00B_GC_SCAN_ALL);

            *tmp = *(void **)obj;
            obj  = tmp;
        }
    }

    return n00b_internal_marshal(ctx, obj);
}

static n00b_list_t *
incremental_unmarshal(n00b_unpickle_ctx *ctx, n00b_buf_t *inbuf)
{
    n00b_list_t *result = NULL;

    n00b_receive_unpickle_info(ctx, inbuf);
    // The first 16 bytes are the 'magic value', and the virtual address
    // space.
    if (!ctx->vaddr_start) {
        int64_t blen = n00b_buffer_len(ctx->partial);

        if (blen < 16) {
            return result;
        }
        uint64_t *p = (uint64_t *)ctx->partial->data;

        if (!check_magic(*p)) {
            return N00B_CRAISE("Unmarshal data in invalid format."),
                   NULL;
        }
        p++;
        ctx->vaddr_start = *p;

        if (lsb_erase(ctx->vaddr_start) != ctx->vaddr_start) {
            return N00B_CRAISE("Bad virtual address space."), NULL;
        }

        if (blen == 16) {
            ctx->partial     = NULL;
            ctx->part_cursor = NULL;
            return result;
        }

        ctx->partial     = n00b_slice_get(ctx->partial, 16, blen);
        ctx->part_cursor = ctx->partial->data;
    }

    while (true) {
        n00b_buf_t *ready = n00b_check_unpickle_readiness(ctx);

        if (!ready) {
            return result;
        }

        if (!result) {
            result = n00b_list(n00b_type_ref());
        }

        n00b_list_append(result, n00b_unpickle_one(ctx, ready));
    }
}

static n00b_filter_impl marshal_filter = {
    .cookie_size = sizeof(n00b_marshal_filter_t),
    .setup_fn    = (void *)marshal_setup,
    .read_fn     = (void *)incremental_unmarshal,
    .write_fn    = (void *)incremental_marshal,
    .name        = NULL,
};

static n00b_filter_impl inverse_marshal_filter = {
    .cookie_size = sizeof(n00b_marshal_filter_t),
    .setup_fn    = (void *)marshal_setup,
    .write_fn    = (void *)incremental_unmarshal,
    .read_fn     = (void *)incremental_marshal,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_marshal(bool marshal_reads)
{
    n00b_filter_impl   *f;
    int                 policy;
    n00b_filter_spec_t *result;

    if (!marshal_filter.name) {
        marshal_filter.name         = n00b_cstring("marshal");
        inverse_marshal_filter.name = n00b_cstring("marshal (inverse)");
    }

    result = n00b_gc_alloc_mapped(n00b_filter_spec_t, N00B_GC_SCAN_ALL);
    f      = marshal_reads ? &inverse_marshal_filter : &marshal_filter;
    policy = marshal_reads ? N00B_FILTER_READ : N00B_FILTER_WRITE;

    result->impl   = f;
    result->policy = policy;
    result->param  = (void *)1ULL;

    return result;
}

n00b_filter_spec_t *
n00b_filter_unmarshal(bool unmarshal_writes)
{
    n00b_filter_impl   *f;
    int                 policy;
    n00b_filter_spec_t *result;

    if (!marshal_filter.name) {
        marshal_filter.name         = n00b_cstring("marshal");
        inverse_marshal_filter.name = n00b_cstring("marshal (inverse)");
    }

    result = n00b_gc_alloc_mapped(n00b_filter_spec_t, N00B_GC_SCAN_ALL);
    f      = unmarshal_writes ? &inverse_marshal_filter : &marshal_filter;
    policy = unmarshal_writes ? N00B_FILTER_WRITE : N00B_FILTER_READ;

    result->impl   = f;
    result->policy = policy;
    result->param  = (void *)0ULL;

    return result;
}

static void
helper_cb(void **addr, void *value)
{
    *addr = value;
}

void *
n00b_autounmarshal(n00b_buf_t *b)
{
    void               *result;
    n00b_filter_spec_t *f  = n00b_filter_unmarshal(true);
    n00b_stream_t      *cb = n00b_new_callback_stream(helper_cb, &result, f);

    n00b_write(cb, b);

    return result;
}

n00b_buf_t *
n00b_automarshal(void *obj)
{
    void               *result;
    n00b_filter_spec_t *f  = n00b_filter_marshal(false);
    n00b_stream_t      *cb = n00b_new_callback_stream(helper_cb, &result, f);

    n00b_write(cb, obj);

    return result;
}
