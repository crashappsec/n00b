#include "n00b.h"

void
n00b_marshal_i64(int64_t i, n00b_stream_t *s)
{
    little_64(i);

    n00b_stream_raw_write(s, sizeof(int64_t), (char *)&i);
}

int64_t
n00b_unmarshal_i64(n00b_stream_t *s)
{
    int64_t result;

    n00b_stream_raw_read(s, sizeof(int64_t), (char *)&result);
    little_64(result);

    return result;
}

void
n00b_marshal_i32(int32_t i, n00b_stream_t *s)
{
    little_32(i);

    n00b_stream_raw_write(s, sizeof(int32_t), (char *)&i);
}

int32_t
n00b_unmarshal_i32(n00b_stream_t *s)
{
    int32_t result;

    n00b_stream_raw_read(s, sizeof(int32_t), (char *)&result);
    little_32(result);

    return result;
}

void
n00b_marshal_i16(int16_t i, n00b_stream_t *s)
{
    little_16(i);
    n00b_stream_raw_write(s, sizeof(int16_t), (char *)&i);
}

int16_t
n00b_unmarshal_i16(n00b_stream_t *s)
{
    int16_t result;

    n00b_stream_raw_read(s, sizeof(int16_t), (char *)&result);
    little_16(result);
    return result;
}

// These are for NON-object types only.
// Everything else is plucked from object vtables.
//
// Maintaining the order of these will be important to avoid future
// breaking changes.  If things go away, null them out instead.
//
// In fact, data layout changes for anything written in C could end up
// being an issue.

// We want to make sure to be able to marshal / unmarshal across
// various compilation options that affect the n00b header.
//
// The last two nibbles of the magic word have the low bit set to 0 if
// we're not using any of the options; we will use those bits to
// determine the disposition of those options.
//
// For the moment, we're just going to error if the disposition doesn't
// match across marshal / unmarshal. But eventually perhaps we will
// add a function to adapt, which is pretty straightforward.
//
// The rest of the magic will be used to ensure we're not trying to
// accidentally unmarshal something we shouldn't.
//
// Note that we don't currently marshal the actual info that's
// conditionally compiled, just make space for it. The memory guard we
// recompute when unmarshaling, and we need to do that when the header
// doesn't contain it, so not too useful to write it out.

#define N00B_MARSHAL_MAGIC 0xc0cac01ab0ba1ceeULL
#if defined(N00B_FULL_MEMCHECK)
#define M1 01ULL
#else
#define M1 00ULL
#endif
#if defined(N00B_FULL_MEMCHECK)
#define M2 10ULL
#else
#define M2 00ULL
#endif

#define SCAN_FN_END (void *)0xffffffffffffffff

const uint64_t     n00b_marshal_magic = N00B_MARSHAL_MAGIC | M1 | M2;
extern n00b_type_t *n00b_type_node_for_list_of_type_objects;
// These live in automarshal.c, which needs to know the size.
extern const void *n00b_builtin_gc_bit_fns[];

static inline n00b_dict_t *
n00b_alloc_marshal_memos()
{
    return n00b_new(n00b_type_dict(n00b_type_ref(), n00b_type_u64()));
}

static int64_t
map_scan_fn(void *fn)
{
    void **p = (void **)&n00b_builtin_gc_bit_fns[0];

    while (*p != SCAN_FN_END) {
        if (*p == fn) {
            return p - (void **)&n00b_builtin_gc_bit_fns[0];
        }
        p++;
    }

    return -1;
}

bool
n00b_in_data_range(void *ptr, n00b_segment_range_t *range)
{
    // TODO: Add heaps from other threads.
    if (n00b_in_heap(ptr)) {
        if (range) {
            *range = (n00b_segment_range_t){
                .start = NULL,
            };
        }
        return true;
    }

    n00b_segment_range_t *seg = atomic_load(&n00b_static_segments);

    while (seg != NULL) {
        if (seg->start < ptr && seg->end > ptr) {
            if (range) {
                *range = *seg;
            }
            return true;
        }
        seg = seg->next;
    }

    return false;
}

static uint64_t
fake_heap(void)
{
    // The inverse value is used to mark parts of the type system that
    // need to restore back to pointers to internal memory addresses
    // that are consistent across marshaled objects. This includes
    // type objects for *primitive* types, and a single complex type,
    // the type used for list objects that contain lists of type
    // objects (which is specially constructed to avoid messy
    // recursion issues).

    uint64_t result;
    uint64_t inverse;

    do {
        result  = ((uint64_t)n00b_rand32());
        inverse = (~result) << 32;
        result <<= 32;

        if (n00b_unlikely(!result) || n00b_unlikely(!inverse)) {
            continue;
        }

        // clang-format off
    } while (n00b_in_data_range((void *)result, NULL) ||
	     n00b_in_data_range((void *)inverse, NULL));
    // clang-format on

    return result;
}

static uint64_t
add_alloc_to_worklist(n00b_alloc_hdr   *hdr,
                      n00b_marshal_ctx *ctx)
{
    uint64_t               fake_base = ctx->memo;
    n00b_marshal_wl_item_t *rec;

    ctx->memo += hdr->alloc_len;
    rec = n00b_gc_alloc_mapped(n00b_marshal_wl_item_t,
                              N00B_GC_SCAN_ALL);

    hatrack_dict_put(ctx->memos, hdr, (void *)fake_base);

    rec->header     = hdr;
    rec->data       = hdr->data;
    rec->dataoffset = fake_base;

    if (ctx->worklist_start == NULL) {
        ctx->worklist_start = rec;
        ctx->worklist_end   = rec;
    }
    else {
        ctx->worklist_end->next = rec;
        ctx->worklist_end       = rec;
    }

    return fake_base;
}

static inline uint64_t
lsb_erase(uint64_t n)
{
    return n & 0xffffffff00000000ull;
}

static inline void
n00b_handle_collision(void *pointer, n00b_marshal_ctx *ctx)
{
    n00b_static_c_backtrace();
    if (!ctx->needed_patches) {
        ctx->needed_patches = n00b_dict(n00b_type_u64(), n00b_type_u64());
    }

    // Record the offset that needs to be written, and the value that
    // needs to be written into it. But we don't write it now because
    // we don't want it to be considered a pointer during unmarshalling.
    //
    hatrack_dict_put(ctx->needed_patches, (void *)ctx->write_offset, pointer);
}

static inline uint64_t
translate_to_fake_pointer(void *pointer, n00b_marshal_ctx *ctx)
{
    // This function should be called only on an address we know we
    // want to marshal, that we think might have pointers, but
    // could also be data.
    //
    // If it doesn't have a pointer, then we treat it as data.

    if (lsb_erase((uint64_t)pointer) == lsb_erase((uint64_t)ctx->memo)) {
        n00b_handle_collision(pointer, ctx);
        return 0ULL;
    }

    if (!n00b_in_data_range(pointer, NULL)) {
        return (uint64_t)pointer;
    }

    // If the target allocation already has a place in the fake heap,
    // use that. Otherwise, we create a new one and add the
    // allocation to the marshaling worklist.
    n00b_alloc_hdr *hdr = n00b_find_allocation_record(pointer);

    if (!hdr) {
        return (uint64_t)pointer;
    }

    uint64_t memo = (uint64_t)hatrack_dict_get(ctx->memos, hdr, NULL);

    if (memo == 0ULL) {
        memo = add_alloc_to_worklist(hdr, ctx);
    }

    uint64_t pointer_ix = ((char *)pointer) - (char *)hdr;

    return memo + pointer_ix;
}

static void
write_one_word(void *pointer, n00b_marshal_ctx *ctx, bool always_data)
{
    if (always_data) {
        n00b_marshal_u64((uint64_t)pointer, ctx->s);
    }
    else {
        n00b_marshal_u64(translate_to_fake_pointer(pointer, ctx), ctx->s);
    }

    ctx->write_offset += sizeof(int64_t);
}

static void
automarshal_scan_and_copy_possible_pointers(n00b_marshal_ctx *ctx,
                                            n00b_alloc_hdr   *h)
{
    uint64_t  n   = ctx->cur->header->alloc_len - sizeof(n00b_alloc_hdr);
    uint64_t *p   = ctx->cur->data;
    uint64_t *end = (uint64_t *)(((char *)p) + n);

    while (p < end) {
        write_one_word((void *)*p, ctx, false);
        p++;
    }
}

static void
automarshal_scan_and_copy_values(n00b_marshal_ctx *ctx, n00b_alloc_hdr *h)
{
    uint64_t  n   = ctx->cur->header->alloc_len - sizeof(n00b_alloc_hdr);
    uint64_t *p   = ctx->cur->data;
    uint64_t *end = (uint64_t *)(((char *)p) + n);

    while (p < end) {
        write_one_word((void *)*p, ctx, true);
        p++;
    }
}

static void
automarshal_scan_and_copy_via_bitmap(n00b_marshal_ctx *ctx,
                                     n00b_mem_scan_fn  fn,
                                     n00b_alloc_hdr   *hdr)
{
    uint64_t *p           = ctx->cur->data;
    uint32_t  numwords    = (hdr->alloc_len - sizeof(n00b_alloc_hdr)) / 8;
    uint32_t  bf_byte_len = ((numwords / 64) + 1) * 8;
    uint64_t *map         = alloca(bf_byte_len);

    memset(map, 0, bf_byte_len);

    (*fn)(map, hdr->data);

    uint64_t *scan_ptr = map;
    uint32_t  scan_ix  = 0;
    uint64_t  value;

    for (uint32_t i = 0; i < numwords; i++) {
        value            = *p;
        bool always_data = !((*scan_ptr) & (1 << scan_ix++));

        write_one_word((void *)value, ctx, always_data);

        if (n00b_unlikely(scan_ix == 64)) {
            ++scan_ptr;
            scan_ix = 0;
        }

        p++;
    }
}

static inline void
automarshal_one_allocation(n00b_marshal_ctx *ctx)
{
    n00b_alloc_hdr *h = ctx->cur->header;
    char          *p = (char *)h;

    assert(h->alloc_len == ((char *)h->next_addr) - p);
    if (!n00b_in_heap(h)) {
        abort();
    }
    // To deal with one allocation, we need to do the following: Write
    // out a 'fake' header. We write this out to the size of
    // n00b_alloc_hdr that WE have; the discrepencies are dealt with
    // when unmarshaling.
    //
    // Instead of just streaming things out, we're going to build
    // a new object, then write it out.
    n00b_marshaled_hdr fake_hdr = {
        .empty_guard = 0x13addbbeab3ddddaULL,
        .next_offset = ctx->cur->dataoffset + h->alloc_len,
        .empty_fw    = 0ULL,
        .alloc_len   = h->alloc_len,
        .request_len = h->request_len,
        .scan_fn_id  = map_scan_fn(h->scan_fn),
#if defined(N00B_FULL_MEMCHECK)
        .end_guard_loc = 0x0ULL,
#endif
#if defined(N00B_ADD_ALLOC_LOC_INFO)
        .alloc_file = 0x0ULL, //(void *)0,
        .alloc_line = 0x0,
#endif
        .type        = (void *)translate_to_fake_pointer(h->type, ctx),
        .finalize    = h->finalize,
        .n00b_obj   = h->n00b_obj,
        .cached_hash = h->cached_hash,
    };

    if (fake_hdr.scan_fn_id == -2) {
        n00b_utf8_t *s = n00b_cstr_format(
            "Unknown gc pointer bitmap scanning "
            "function found in allocation. "
#if defined(N00B_ADD_ALLOC_LOC_INFO)
            "Allocation location: [em]{}:{}",
            n00b_new_utf8(h->alloc_file),
            n00b_box_u64(h->alloc_line));
#else
            "Compile with [em]N00B_ADD_ALLOC_LOC_INFO[/] for diagnostics.");
#endif
        N00B_RAISE(s);
    }

    assert(sizeof(n00b_marshaled_hdr) == sizeof(n00b_alloc_hdr));

    // Conver endianness on big endian machines; should not generate
    // any code at all most places.
    little_64(fake_hdr.next_offset);
    little_32(fake_hdr.alloc_len);
    little_32(fake_hdr.request_len);
    little_64(fake_hdr.scan_fn_id);

#if BYTE_ORDER == BIG_ENDIAN
    uint64_t *p = (uint64_t *)&fake_hdr.cached_hash;
#endif
    little_64(*p[0]);
    little_64(*p[1]);

    n00b_stream_raw_write(ctx->s,
                         sizeof(n00b_marshaled_hdr),
                         (char *)&fake_hdr);
    ctx->write_offset += sizeof(n00b_marshaled_hdr);

    n00b_mem_scan_fn scanner = h->scan_fn;

    // For n00b objects, we need to either track or repair important
    // static pointers. One of those is in the n00b object header--
    // the very first word is the 'base_type' field, which points
    // into memory we don't want to marshal for a bunch of reasons.
    // So we pass the value of n00b_obj to the scan functions, and if
    // it's true, then those functions special-case the first word.

    if ((void *)scanner == N00B_GC_SCAN_ALL) {
        automarshal_scan_and_copy_possible_pointers(ctx, h);
    }
    else {
        if ((void *)scanner == N00B_GC_SCAN_NONE) {
            automarshal_scan_and_copy_values(ctx, h);
        }
        else {
            automarshal_scan_and_copy_via_bitmap(ctx, scanner, h);
        }
    }
}

// This takes work off the worklist one item at a time until there's no
// work to do. Doing work can easily create other work, of course.
static inline void
automarshal_worklist_process(n00b_marshal_ctx *ctx)
{
    while (ctx->worklist_start != NULL) {
        ctx->cur            = ctx->worklist_start;
        ctx->worklist_start = ctx->cur->next;
        // Todo: if the GC initiates in the middle of marshaling, this
        // assert fails... need to figure that out or, more likely, use
        // a tmp heap.  assert(ctx->write_offset ==
        // ctx->cur->dataoffset);
        automarshal_one_allocation(ctx);
    }
    ctx->worklist_end = NULL;
}

// We need a header that indicates to the unmarshal routine that
// the main unmarshaling is done. Also, on the off-chance of a
// collision, we will add them here too.
static inline void
automarshal_write_footer(n00b_marshal_ctx *ctx)
{
    uint64_t  n = 0;
    uint64_t *patches;

    if (ctx->needed_patches != NULL) {
        patches = (uint64_t *)hatrack_dict_items_sort(ctx->needed_patches,
                                                      &n);
    }

    n00b_marshaled_hdr fake_hdr = {
        .empty_guard = 0x13addbbeab3ddddaULL,
        .next_offset = 0, // THIS signals we are done with allocs.
        .alloc_len   = n, // Ths is the only field that might not be 0!
        0,
    };

    n00b_stream_raw_write(ctx->s, sizeof(fake_hdr), (char *)&fake_hdr);

    if (n != 0) {
        n00b_stream_raw_write(ctx->s,
                             (sizeof(uint64_t) * 2) * n,
                             (char *)patches);
    }
}

void
n00b_automarshal_stream(void *value, n00b_stream_t *s)
{
    // The way this works is by translating all pointers to a
    // 'virtual' pointer in a heap that doesn't exist, in such a way
    // that the pointer's OFFSET from that imaginary heap's beginning
    // maps to the offset from the start of the marshal'd stream.
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

    uint64_t heap_start = fake_heap();

    n00b_marshal_u64(n00b_marshal_magic, s);

    n00b_marshal_ctx ctx = {
        .s              = s,
        .memos          = n00b_alloc_marshal_memos(),
        .needed_patches = NULL, // Instantiate only if needed.
        .worklist_start = NULL,
        .worklist_end   = NULL,
        .write_offset   = heap_start + 8,
        // We write the magic, and the first value we write is not part of
        // an allocation.
        .base           = heap_start,
        .memo           = heap_start + 16,

    };

    if (!n00b_in_data_range(value, NULL)) {
        write_one_word((void *)0ULL, &ctx, true);
    }

    write_one_word(value, &ctx, false);
    automarshal_worklist_process(&ctx);
    automarshal_write_footer(&ctx);

    return;
}
