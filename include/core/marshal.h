#pragma once

#include "n00b.h"

typedef struct n00b_marshal_wl_item_t {
    n00b_alloc_hdr                *header;
    uint64_t                     *data;
    uint64_t                      dataoffset;
    struct n00b_marshal_wl_item_t *next;
} n00b_marshal_wl_item_t;

// This needs to stay in sync w/ n00b_alloc_hdr
typedef struct n00b_marshal_ctx {
    n00b_stream_t          *s;
    n00b_dict_t            *memos;
    n00b_dict_t            *needed_patches;
    n00b_marshal_wl_item_t *worklist_start;
    n00b_marshal_wl_item_t *worklist_end;
    n00b_marshal_wl_item_t *cur;
    uint64_t               base;
    uint64_t               memo;
    uint64_t               write_offset;
} n00b_marshal_ctx;

typedef struct {
    n00b_mem_ptr    base_ptr;
    n00b_mem_ptr    end;
    uint64_t       fake_heap_base;
    n00b_alloc_hdr *this_alloc;
    n00b_list_t    *to_restore;

} n00b_unmarshal_ctx;

typedef struct n00b_marshaled_hdr {
    uint64_t    empty_guard;
    uint64_t    next_offset;
    uint64_t    empty_fw;
    n00b_type_t *type;
    int64_t     scan_fn_id; // Only marshaled for non-object types.
    uint32_t    alloc_len;
    uint32_t    request_len;
#if defined(N00B_FULL_MEMCHECK)
    uint64_t end_guard_loc;
#endif
#if defined(N00B_ADD_ALLOC_LOC_INFO)
    char *alloc_file;
    int   alloc_line;
#endif

    uint32_t    finalize  : 1;
    uint32_t    n00b_obj : 1;
    __uint128_t cached_hash;
    alignas(N00B_FORCED_ALIGNMENT) uint64_t data[];
} n00b_marshaled_hdr;

// This is going to be the same layout as n00b_alloc_hdr_t because we
// are going to get unmarshalling down to the point where data can
// be placed statically with relocations. Unmarshalling will only
// involve fixing up pointers and filling in any necessary bits of
// alloc headers.

typedef struct {
    uint32_t scan_fn;
    uint32_t alloc_len : 31;
} n00b_marshal_record_t;

extern void    n00b_marshal_cstring(char *, n00b_stream_t *);
extern char   *n00b_unmarshal_cstring(n00b_stream_t *);
extern void    n00b_marshal_i64(int64_t, n00b_stream_t *);
extern int64_t n00b_unmarshal_i64(n00b_stream_t *);
extern void    n00b_marshal_i32(int32_t, n00b_stream_t *);
extern int32_t n00b_unmarshal_i32(n00b_stream_t *);
extern void    n00b_marshal_i16(int16_t, n00b_stream_t *);
extern int16_t n00b_unmarshal_i16(n00b_stream_t *);

// These functions need to either be exported, because they're needed wherever
// we allocate, and they're needed centrally to support auto-marshaling.
//
// n00b object types are first here.
extern void n00b_buffer_set_gc_bits(uint64_t *, void *);
extern void n00b_zcallback_gc_bits(uint64_t *, void *);
extern void n00b_dict_gc_bits_obj(uint64_t *, void *);
extern void n00b_dict_gc_bits_raw(uint64_t *, void *);
extern void n00b_grid_set_gc_bits(uint64_t *, void *);
extern void n00b_renderable_set_gc_bits(uint64_t *, void *);
extern void n00b_list_set_gc_bits(uint64_t *, void *);
extern void n00b_set_set_gc_bits(uint64_t *, void *);
extern void n00b_stream_set_gc_bits(uint64_t *, void *);
extern void n00b_str_set_gc_bits(uint64_t *, void *);
extern void n00b_tree_node_set_gc_bits(uint64_t *, void *);
extern void n00b_exception_gc_bits(uint64_t *, void *);
extern void n00b_type_set_gc_bits(uint64_t *, void *);
extern void n00b_pnode_set_gc_bits(uint64_t *, void *);

// Adt / IO related, etc.
extern void n00b_dict_gc_bits_bucket_full(uint64_t *, mmm_header_t *);
extern void n00b_dict_gc_bits_bucket_key(uint64_t *, mmm_header_t *);
extern void n00b_dict_gc_bits_bucket_value(uint64_t *, mmm_header_t *);
extern void n00b_dict_gc_bits_bucket_hdr_only(uint64_t *, mmm_header_t *);
extern void n00b_store_bits(uint64_t *, mmm_header_t *);
extern void n00b_cookie_gc_bits(uint64_t *, n00b_cookie_t *);
extern void n00b_basic_http_set_gc_bits(uint64_t *, n00b_basic_http_t *);
extern void n00b_party_gc_bits(uint64_t *, n00b_party_t *);
extern void n00b_monitor_gc_bits(uint64_t *, n00b_monitor_t *);
extern void n00b_subscription_gc_bits(uint64_t *, n00b_subscription_t *);
extern void n00b_fmt_gc_bits(uint64_t *, n00b_fmt_info_t *);
extern void n00b_tag_gc_bits(uint64_t *, n00b_tag_item_t *);
extern void n00b_rs_gc_bits(uint64_t *, n00b_render_style_t *);
extern void n00b_tpat_gc_bits(uint64_t *, n00b_tpat_node_t *);

// Compiler.
extern void n00b_cctx_gc_bits(uint64_t *, n00b_compile_ctx *);
extern void n00b_smem_gc_bits(uint64_t *, n00b_static_memory *);
extern void n00b_module_ctx_gc_bits(uint64_t *, n00b_module_t *);
extern void n00b_token_set_gc_bits(uint64_t *, void *);
extern void n00b_checkpoint_gc_bits(uint64_t *, n00b_checkpoint_t *);
extern void n00b_comment_node_gc_bits(uint64_t *, n00b_comment_node_t *);
extern void n00b_sym_gc_bits(uint64_t *, n00b_symbol_t *);
extern void n00b_scope_gc_bits(uint64_t *, n00b_scope_t *);

extern void n00b_cfg_gc_bits(uint64_t *, n00b_cfg_node_t *);
extern void n00b_cfg_status_gc_bits(uint64_t *, n00b_cfg_status_t *);
extern void n00b_zinstr_gc_bits(uint64_t *, n00b_zinstruction_t *);
extern void n00b_zfn_gc_bits(uint64_t *, n00b_zfn_info_t *);
extern void n00b_jump_info_gc_bits(uint64_t *, n00b_jump_info_t *);
extern void n00b_backpatch_gc_bits(uint64_t *, n00b_call_backpatch_info_t *);
extern void n00b_module_param_gc_bits(uint64_t *, n00b_module_param_info_t *);
extern void n00b_sig_info_gc_bits(uint64_t *, n00b_sig_info_t *);
extern void n00b_fn_decl_gc_bits(uint64_t *, n00b_fn_decl_t *);
extern void n00b_err_set_gc_bits(uint64_t *, n00b_compile_error *);
extern void n00b_objfile_gc_bits(uint64_t *, n00b_zobject_file_t *);
extern void n00b_call_resolution_gc_bits(uint64_t *, void *);

// N00b specific runtime stuff
extern void n00b_spec_gc_bits(uint64_t *, n00b_spec_t *);
extern void n00b_attr_info_gc_bits(uint64_t *, n00b_attr_info_t *);
extern void n00b_section_gc_bits(uint64_t *, n00b_spec_section_t *);
extern void n00b_spec_field_gc_bits(uint64_t *, n00b_spec_field_t *);
extern void n00b_vm_gc_bits(uint64_t *, n00b_vm_t *);
extern void n00b_vmthread_gc_bits(uint64_t *, n00b_vmthread_t *);
extern void n00b_deferred_cb_gc_bits(uint64_t *, n00b_deferred_cb_t *);
extern void n00b_frame_gc_bits(uint64_t *, n00b_fmt_frame_t *);

static inline void
n00b_marshal_i8(int8_t c, n00b_stream_t *s)
{
    n00b_stream_raw_write(s, 1, (char *)&c);
}

static inline int8_t
n00b_unmarshal_i8(n00b_stream_t *s)
{
    int8_t ret;

    n00b_stream_raw_read(s, 1, (char *)&ret);

    return ret;
}

static inline void
n00b_marshal_u8(uint8_t n, n00b_stream_t *s)
{
    n00b_marshal_i8((int8_t)n, s);
}

static inline uint8_t
n00b_unmarshal_u8(n00b_stream_t *s)
{
    return (uint8_t)n00b_unmarshal_i8(s);
}

static inline void
n00b_marshal_bool(bool value, n00b_stream_t *s)
{
    n00b_marshal_i8(value ? 1 : 0, s);
}

static inline bool
n00b_unmarshal_bool(n00b_stream_t *s)
{
    return (bool)n00b_unmarshal_i8(s);
}

static inline void
n00b_marshal_u64(uint64_t n, n00b_stream_t *s)
{
    n00b_marshal_i64((int64_t)n, s);
}

static inline uint64_t
n00b_unmarshal_u64(n00b_stream_t *s)
{
    return (uint64_t)n00b_unmarshal_i64(s);
}

static inline void
n00b_marshal_u32(uint32_t n, n00b_stream_t *s)
{
    n00b_marshal_i32((int32_t)n, s);
}

static inline uint32_t
n00b_unmarshal_u32(n00b_stream_t *s)
{
    return (uint32_t)n00b_unmarshal_i32(s);
}

static inline void
n00b_marshal_u16(uint16_t n, n00b_stream_t *s)
{
    n00b_marshal_i16((int16_t)n, s);
}

static inline uint16_t
n00b_unmarshal_u16(n00b_stream_t *s)
{
    return (uint16_t)n00b_unmarshal_i16(s);
}

static inline n00b_alloc_hdr *
n00b_find_allocation_record(void *addr)
{
    // Align the pointer for scanning back to the header.
    // It should be aligned, but people do the craziest things.
    void **p = (void **)(((uint64_t)addr) & ~0x0000000000000007);

    while (*(uint64_t *)p != n00b_gc_guard) {
        --p;
        if (!n00b_in_heap(p)) {
            return NULL;
        }
    }

    return (n00b_alloc_hdr *)p;
}

extern void       n00b_automarshal_stream(void *, n00b_stream_t *);
extern n00b_buf_t *n00b_automarshal(void *);
extern void      *n00b_autounmarshal_stream(n00b_stream_t *);
extern void      *n00b_autounmarshal(n00b_buf_t *);
