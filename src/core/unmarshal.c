#include "n00b.h"

#define SCAN_FN_END (void *)0xffffffffffffffff

#define FIRST_BAD_SCAN_IX \
    (int)((sizeof(n00b_builtin_gc_bit_fns) / sizeof(void *)) - 1)

extern const uint64_t n00b_marshal_magic;
extern n00b_type_t    *n00b_type_node_for_list_of_type_objects;
extern uint64_t       n00b_end_guard;
const void           *n00b_builtin_gc_bit_fns[] = {
    NULL,
    n00b_buffer_set_gc_bits,
    n00b_zcallback_gc_bits,
    n00b_dict_gc_bits_raw,
    n00b_store_bits,
    n00b_grid_set_gc_bits,
    n00b_renderable_set_gc_bits,
    n00b_list_set_gc_bits,
    n00b_set_set_gc_bits,
    n00b_stream_set_gc_bits,
    n00b_str_set_gc_bits,
    n00b_tree_node_set_gc_bits,
    n00b_exception_gc_bits,
    n00b_type_set_gc_bits,
    n00b_pnode_set_gc_bits,
    n00b_dict_gc_bits_bucket_full,
    n00b_dict_gc_bits_bucket_key,
    n00b_dict_gc_bits_bucket_value,
    n00b_dict_gc_bits_bucket_hdr_only,
    n00b_cookie_gc_bits,
    n00b_basic_http_set_gc_bits,
    n00b_party_gc_bits,
    n00b_monitor_gc_bits,
    n00b_subscription_gc_bits,
    n00b_fmt_gc_bits,
    n00b_tag_gc_bits,
    n00b_rs_gc_bits,
    n00b_tpat_gc_bits,
    n00b_cctx_gc_bits,
    n00b_module_ctx_gc_bits,
    n00b_token_set_gc_bits,
    n00b_checkpoint_gc_bits,
    n00b_comment_node_gc_bits,
    n00b_sym_gc_bits,
    n00b_scope_gc_bits,
    n00b_cfg_gc_bits,
    n00b_cfg_status_gc_bits,
    n00b_zinstr_gc_bits,
    n00b_zfn_gc_bits,
    n00b_jump_info_gc_bits,
    n00b_backpatch_gc_bits,
    n00b_module_param_gc_bits,
    n00b_sig_info_gc_bits,
    n00b_fn_decl_gc_bits,
    n00b_err_set_gc_bits,
    n00b_objfile_gc_bits,
    n00b_spec_gc_bits,
    n00b_attr_info_gc_bits,
    n00b_section_gc_bits,
    n00b_spec_field_gc_bits,
    n00b_vm_gc_bits,
    n00b_vmthread_gc_bits,
    n00b_deferred_cb_gc_bits,
    n00b_frame_gc_bits,
    n00b_smem_gc_bits,
    n00b_call_resolution_gc_bits,
    SCAN_FN_END,
};

static inline uint64_t
validate_header(n00b_unmarshal_ctx *ctx)
{
    n00b_mem_ptr p = ctx->base_ptr;

    uint64_t found_magic = *p.u64;
    p.u64++;

    little_64(found_magic);

    if (found_magic != n00b_marshal_magic) {
        n00b_utf8_t *err = NULL;

        if ((found_magic & ~03ULL) == (n00b_marshal_magic & ~03ULL)) {
            err = n00b_cstr_format(
                "Input was marshaled using different memory options. Expected "
                "[i]a N00b marshal header value of[/] of [em]{:x}[/], but "
                "got [em]{:x}[/]",
                n00b_box_u64(n00b_marshal_magic),
                n00b_box_u64(found_magic));
        }
        else {
            if ((found_magic & 0xffff) == (n00b_marshal_magic & 0xffff)) {
                err = n00b_cstr_format(
                    "N00b version used for marshaling is not known to this "
                    "version of n00b [em]({:x})[/]",
                    n00b_box_u64(found_magic));
            }
            else {
                err = n00b_cstr_format(
                    "First 8 bytes [em]({:x})[/] is not a valid N00b marshal "
                    "header.",
                    n00b_box_u64(found_magic));
            }
        }
        N00B_RAISE(err);
    }

    uint64_t value = *p.u64;
    little_64(value);

    if (value) {
        ctx->fake_heap_base = value & 0xffffffff00000000ULL;
        return value & 0x00000000ffffffffULL;
    }

    return 0;
}

static void __attribute__((noreturn))
unmarshal_invalid()
{
    N00B_CRAISE("Invalid or corrupt marshal data.");
}

static void __attribute__((noreturn))
future_n00b_version()
{
    N00B_CRAISE(
        "Marshaled data provides information that "
        "does not exist in this version of N00b, thus cannot "
        "be unmarshaled.");
}

// Make sure the value already went through little_64() before
// calling.
static inline bool
is_fake_heap_ptr(n00b_unmarshal_ctx *ctx, uint64_t value)
{
    return (value & 0xffffffff00000000ULL) == ctx->fake_heap_base;
}

static uint64_t
translate_pointer(n00b_unmarshal_ctx *ctx, uint64_t value)
{
    n00b_mem_ptr p;
    uint64_t    offset = value & 0x00000000ffffffffUL;
    p.c                = ctx->base_ptr.c + offset;

    if (p.c >= ctx->end.c) {
        unmarshal_invalid();
    }

    return (uint64_t)p.c;
}

static void *
lookup_scan_fn(void *index_as_void)
{
    int64_t index = (int64_t)index_as_void;
    if (!index || index == -1) {
        return index_as_void;
    }

    if (index < 0 || index >= FIRST_BAD_SCAN_IX) {
        future_n00b_version();
    }

    return (void *)n00b_builtin_gc_bit_fns[index];
}

static inline void
setup_header(n00b_unmarshal_ctx *ctx, n00b_marshaled_hdr *marshaled_hdr)
{
    little_32(marshaled_hdr->alloc_len);
    little_32(marshaled_hdr->request_len);
    little_64(marshaled_hdr->scan_fn_id);
    little_64(marshaled_hdr->next_offset);

    uint64_t next_ix = marshaled_hdr->next_offset;
    char    *next    = 0;

    if (next_ix) {
        assert(!next_ix || is_fake_heap_ptr(ctx, next_ix));

        if (n00b_unlikely(!is_fake_heap_ptr(ctx, next_ix))) {
            unmarshal_invalid();
        }

        next = (void *)translate_pointer(ctx, next_ix);
        assert(next - marshaled_hdr->alloc_len == (char *)marshaled_hdr);
        assert(*((uint64_t *)next) == 0x13addbbeab3ddddaULL);
    }

    n00b_alloc_hdr *hdr = (n00b_alloc_hdr *)marshaled_hdr;

    hdr->guard     = n00b_gc_guard;
    hdr->fw_addr   = NULL;
    hdr->next_addr = (void *)next;
    hdr->scan_fn   = lookup_scan_fn(hdr->scan_fn);
    if (hdr->type) {
        hdr->type = (void *)translate_pointer(ctx, (uint64_t)hdr->type);
    }

#if defined(N00B_FULL_MEMCHECK)
    uint64_t  word_len       = hdr->alloc_len / sizeof(uint64_t);
    uint64_t *end_guard_addr = &hdr->data[word_len - 2];
    *end_guard_addr          = n00b_end_guard;
    hdr->end_guard_loc       = end_guard_addr;
#endif
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    hdr->alloc_file = "marshaled data";
    hdr->alloc_line = (int)(ctx->fake_heap_base >> 32);
#endif
}

static inline n00b_marshaled_hdr *
validate_record(n00b_unmarshal_ctx *ctx, void *ptr)
{
    n00b_mem_ptr hdr = (n00b_mem_ptr){.v = ptr};

    setup_header(ctx, hdr.v);

    // This shouldn't get called on the end record,
    // so the 'next_alloc' field should always look right.
    // Worst case, the stream could end in a zero-byte alloc and
    // still be valid.

    if (!hdr.alloc->next_addr) {
        return hdr.v;
    }

    char *next_check = hdr.c + hdr.alloc->alloc_len;
    if (hdr.alloc->next_addr != next_check) {
        unmarshal_invalid();
    }
    uint64_t *last_possible = (uint64_t *)(ctx->end.c - sizeof(n00b_alloc_hdr));

    if ((void *)hdr.alloc->next_addr > (void *)last_possible) {
        unmarshal_invalid();
    }

    return hdr.v;
}
static inline void
process_pointer_location(n00b_unmarshal_ctx *ctx, n00b_mem_ptr ptr)
{
    uint64_t value = *ptr.u64;

    if (is_fake_heap_ptr(ctx, value)) {
        *ptr.u64 = translate_pointer(ctx, value);
    }
}

static inline void
process_all_words(n00b_unmarshal_ctx *ctx,
                  n00b_alloc_hdr     *hdr,
                  n00b_mem_ptr        cur,
                  n00b_mem_ptr        end)
{
    while (cur.u64 < end.u64) {
        process_pointer_location(ctx, cur);
        cur.u64++;
    }
}

static void
process_marked_addresses(n00b_unmarshal_ctx *ctx,
                         n00b_alloc_hdr     *hdr)
{
    // n00b_mem_scan_fn scanner     = hdr->scan_fn;
    uint32_t    numwords    = hdr->alloc_len / 8;
    uint32_t    bf_byte_len = ((numwords / 64) + 1) * sizeof(uint64_t);
    uint64_t   *map         = malloc(bf_byte_len); // alloca(bf_byte_len);
    n00b_mem_ptr base        = (n00b_mem_ptr){.v = hdr->data};

    memset(map, 0, bf_byte_len);

    // TODO: Need to pass heap bounds, because some data structures
    //       might have data-dependent scanners that do their own
    //       checking. Easy thing is to convert the second parameter
    //       into a context object (or add a 3rd parameter for the rest).

    if (hdr->scan_fn != NULL) {
        process_all_words(ctx, hdr, base, (n00b_mem_ptr){.v = hdr->next_addr});
        return;
    }
    else {
        (*hdr->scan_fn)(map, hdr->data);

        int         last_cell = numwords / 64;
        n00b_mem_ptr p;

        for (int i = 0; i <= last_cell; i++) {
            uint64_t w = map[i];

            while (w) {
                int ix = 63 - __builtin_clzll(w);
                w &= ~(1ULL << ix);
                p.u64 = base.u64 + ix;
                process_pointer_location(ctx, p);
            }
        }
    }
}

static void
process_one_record(n00b_unmarshal_ctx *ctx, n00b_alloc_hdr *hdr)
{
    // This generally will re-process the header, but it's not
    // worth the twisting of the logic to deal with it.

    n00b_mem_ptr cur = (n00b_mem_ptr){.v = hdr->data};
    n00b_mem_ptr end = (n00b_mem_ptr){.v = hdr->next_addr};

    if (!(hdr->scan_fn == (void *)N00B_GC_SCAN_NONE)) {
        if (hdr->scan_fn == (void *)N00B_GC_SCAN_ALL) {
            process_all_words(ctx, hdr, cur, end);
        }
        else {
            process_marked_addresses(ctx, hdr);
        }
    }
    if (hdr->n00b_obj) {
        n00b_restore(hdr->data);
    }
}

static inline void
finish_unmarshaling(n00b_unmarshal_ctx *ctx,
                    n00b_buf_t         *buf,
                    n00b_alloc_hdr     *hdr)
{
    // First, process any patches, even though there should never
    // be any.

    int         num_patches = hdr->alloc_len;
    n00b_mem_ptr bufdata;
    n00b_mem_ptr p;
    n00b_mem_ptr patch_loc;

    bufdata.c = buf->data;

    if (hdr->data + (2 * num_patches) > ctx->end.u64) {
        unmarshal_invalid();
    }

    p.u64 = hdr->data;

    for (int i = 0; i < num_patches; i++) {
        uint64_t offset = *p.u64;
        p.u64++;
        uint64_t value = *p.u64;
        p.u64++;

        if (!is_fake_heap_ptr(ctx, offset)) {
            unmarshal_invalid();
        }

        patch_loc.u64  = (uint64_t *)translate_pointer(ctx, offset);
        *patch_loc.u64 = value;
    }

    // Now, make the parent allocation header and the last allocation
    // header all look as if we alloc'd in small chunks.
    hdr->next_addr = ctx->this_alloc->next_addr;

#if defined(N00B_FULL_MEMCHECK)
    hdr->end_guard_loc             = ctx->this_alloc->end_guard_loc;
    ctx->this_alloc->end_guard_loc = (uint64_t *)buf->data;
    bufdata.u64[0]                 = n00b_end_guard;
#else
    bufdata.u64[0] = 0;
#endif

    // Make sure there cannot be an attempt to unmarshal the same heap
    // memory a second time.
    //
    // If the data is on the heap and you want to keep it around,
    // copy the buffer first, before calling unmarshal.
    // Note that, even if you managed to keep around a pointer to
    // the data location, we overwrote the header to signal that
    // we were done...
    bufdata.u64[1]               = ~0ULL;
    buf->byte_len                = 0;
    buf->alloc_len               = 0;
    buf->data                    = NULL;
    buf->flags                   = 0;
    ctx->this_alloc->request_len = 0;
    ctx->this_alloc->next_addr   = (char *)(ctx->base_ptr.u64);
}

void *
n00b_autounmarshal(n00b_buf_t *buf)
{
    if (!buf || !buf->data) {
        unmarshal_invalid();
    }

    n00b_alloc_hdr *hdr = n00b_find_allocation_record(buf->data);
    printf("Unmarshal @%p\n", hdr);

    if (!n00b_in_heap(buf->data) || ((char *)hdr->data) != buf->data) {
        buf = n00b_copy(buf);
        hdr = n00b_find_allocation_record(buf->data);
    }

    if (buf->byte_len < 16) {
        N00B_CRAISE("Missing full n00b marshal header.");
    }

    n00b_unmarshal_ctx ctx = {
        .base_ptr.c = buf->data,
        .end.c      = buf->data + buf->byte_len,
        .this_alloc = hdr,
    };

    uint64_t result_offset = validate_header(&ctx);

    // What was marshaled wasn't a pointer, just a single value.
    // Don't know why you'd do that, but you did...
    if (result_offset == 0) {
        uint64_t result_as_i64 = ctx.base_ptr.u64[1];
        little_64(result_as_i64);

        return (void *)result_as_i64;
    }

    // First record is 16 bytes into the data object.
    n00b_marshaled_hdr *mhdr = validate_record(&ctx, buf->data + 16);

    while (mhdr->next_offset) {
        if (ctx.end.v <= (void *)mhdr) {
            unmarshal_invalid();
        }
        process_one_record(&ctx, (n00b_alloc_hdr *)mhdr);
        mhdr = validate_record(&ctx, (void *)mhdr->next_offset);
    }

    char *result = ctx.base_ptr.c + result_offset;

    finish_unmarshaling(&ctx, buf, (n00b_alloc_hdr *)mhdr);

    return (void *)result;
}

void *
n00b_autounmarshal_stream(n00b_stream_t *stream)
{
    n00b_buf_t *b = (n00b_buf_t *)n00b_stream_read_all(stream);

    if (n00b_base_type((void *)b) != N00B_T_BUFFER) {
        N00B_CRAISE("Automarshal requires a binary buffer.");
    }

    return n00b_autounmarshal(b);
}

n00b_buf_t *
n00b_automarshal(void *ptr)
{
    n00b_buf_t    *result = n00b_buffer_empty();
    n00b_stream_t *stream = n00b_buffer_outstream(result, true);

    n00b_automarshal_stream(ptr, stream);
    n00b_stream_close(stream);

    return result;
}
