#pragma once

#include "n00b.h"

// in object.c
extern const n00b_dt_info_t n00b_base_type_info[N00B_NUM_BUILTIN_DTS];

extern n00b_type_t        *n00b_type_resolve(n00b_type_t *);
extern n00b_utf8_t        *n00b_new_utf8(char *);
static inline n00b_type_t *n00b_type_internal();
static inline n00b_type_t *n00b_type_void();
static inline n00b_type_t *n00b_type_i64();

extern n00b_type_t *n00b_get_my_type(n00b_obj_t);

static inline n00b_dt_info_t *
n00b_object_type_info(n00b_obj_t user_object)
{
    uint64_t n = n00b_get_my_type(user_object)->base_index;
    return (n00b_dt_info_t *)&n00b_base_type_info[n];
}

static inline n00b_vtable_t *
n00b_vtable(n00b_obj_t user_object)
{
    return (n00b_vtable_t *)n00b_object_type_info(user_object)->vtable;
}

static inline uint64_t
n00b_base_type(n00b_obj_t user_object)
{
    return n00b_get_my_type(user_object)->base_index;
}

static inline n00b_utf8_t *
n00b_base_type_name(n00b_obj_t user_object)
{
    return n00b_new_utf8((char *)n00b_object_type_info(user_object)->name);
}

// The first 2 words are pointers, but the first one is static.
#define N00B_HEADER_SCAN_CONST 0x02ULL

static inline void
n00b_set_bit(uint64_t *bitfield, int ix)
{
    int word = ix / 64;
    int bit  = ix % 64;

    bitfield[word] |= (1ULL << bit);
}

static inline void
n00b_mark_address(uint64_t *bitfield, void *base, void *address)
{
    n00b_set_bit(bitfield, n00b_ptr_diff(base, address));
}

static inline void
n00b_restore(n00b_obj_t user_object)
{
    n00b_vtable_t  *vt = n00b_vtable(user_object);
    n00b_restore_fn fn = (void *)vt->methods[N00B_BI_RESTORE];

    if (fn != NULL) {
        (*fn)(user_object);
    }
}

static inline bool
n00b_is_null(void *n)
{
    return (n == NULL) && !n00b_in_heap(n);
}

extern void n00b_scan_header_only(uint64_t *, int);

#if defined(N00B_GC_STATS) || defined(N00B_DEBUG)
extern n00b_obj_t _n00b_new(char *, int, n00b_type_t *, ...);

#define n00b_new(tid, ...) _n00b_new(__FILE__, __LINE__, tid, N00B_VA(__VA_ARGS__))
#else
extern n00b_obj_t _n00b_new(n00b_type_t *type, ...);

#define n00b_new(tid, ...) _n00b_new(tid, N00B_VA(__VA_ARGS__))
#endif

extern n00b_str_t  *n00b_repr(void *, n00b_type_t *);
extern n00b_str_t  *n00b_to_str(void *, n00b_type_t *);
extern bool        n00b_can_coerce(n00b_type_t *, n00b_type_t *);
extern n00b_obj_t   n00b_coerce(void *, n00b_type_t *, n00b_type_t *);
extern n00b_obj_t   n00b_coerce_object(const n00b_obj_t, n00b_type_t *);
extern n00b_obj_t   n00b_copy(n00b_obj_t);
extern n00b_obj_t   n00b_copy_object_of_type(n00b_obj_t, n00b_type_t *);
extern n00b_obj_t   n00b_add(n00b_obj_t, n00b_obj_t);
extern n00b_obj_t   n00b_sub(n00b_obj_t, n00b_obj_t);
extern n00b_obj_t   n00b_mul(n00b_obj_t, n00b_obj_t);
extern n00b_obj_t   n00b_div(n00b_obj_t, n00b_obj_t);
extern n00b_obj_t   n00b_mod(n00b_obj_t, n00b_obj_t);
extern bool        n00b_eq(n00b_type_t *, n00b_obj_t, n00b_obj_t);
extern bool        n00b_lt(n00b_type_t *, n00b_obj_t, n00b_obj_t);
extern bool        n00b_gt(n00b_type_t *, n00b_obj_t, n00b_obj_t);
extern int64_t     n00b_len(n00b_obj_t);
extern n00b_obj_t   n00b_index_get(n00b_obj_t, n00b_obj_t);
extern void        n00b_index_set(n00b_obj_t, n00b_obj_t, n00b_obj_t);
extern n00b_obj_t   n00b_slice_get(n00b_obj_t, int64_t, int64_t);
extern void        n00b_slice_set(n00b_obj_t, int64_t, int64_t, n00b_obj_t);
extern n00b_str_t  *n00b_value_obj_repr(n00b_obj_t);
extern n00b_str_t  *n00b_value_obj_to_str(n00b_obj_t);
extern n00b_type_t *n00b_get_item_type(n00b_obj_t);
extern void       *n00b_get_view(n00b_obj_t, int64_t *);
extern n00b_obj_t   n00b_container_literal(n00b_type_t *,
                                         n00b_list_t *,
                                         n00b_utf8_t *);
extern void        n00b_finalize_allocation(void *);
extern n00b_obj_t   n00b_shallow(n00b_obj_t);
extern void       *n00b_autobox(void *);

extern const n00b_vtable_t n00b_i8_type;
extern const n00b_vtable_t n00b_u8_type;
extern const n00b_vtable_t n00b_i32_type;
extern const n00b_vtable_t n00b_u32_type;
extern const n00b_vtable_t n00b_i64_type;
extern const n00b_vtable_t n00b_u64_type;
extern const n00b_vtable_t n00b_bool_type;
extern const n00b_vtable_t n00b_float_type;
extern const n00b_vtable_t n00b_u8str_vtable;
extern const n00b_vtable_t n00b_u32str_vtable;
extern const n00b_vtable_t n00b_buffer_vtable;
extern const n00b_vtable_t n00b_grid_vtable;
extern const n00b_vtable_t n00b_renderable_vtable;
extern const n00b_vtable_t n00b_flexarray_vtable;
extern const n00b_vtable_t n00b_queue_vtable;
extern const n00b_vtable_t n00b_ring_vtable;
extern const n00b_vtable_t n00b_logring_vtable;
extern const n00b_vtable_t n00b_stack_vtable;
extern const n00b_vtable_t n00b_dict_vtable;
extern const n00b_vtable_t n00b_set_vtable;
extern const n00b_vtable_t n00b_list_vtable;
extern const n00b_vtable_t n00b_sha_vtable;
extern const n00b_vtable_t n00b_render_style_vtable;
extern const n00b_vtable_t n00b_exception_vtable;
extern const n00b_vtable_t n00b_type_spec_vtable;
extern const n00b_vtable_t n00b_tree_vtable;
extern const n00b_vtable_t n00b_tuple_vtable;
extern const n00b_vtable_t n00b_mixed_vtable;
extern const n00b_vtable_t n00b_ipaddr_vtable;
extern const n00b_vtable_t n00b_stream_vtable;
extern const n00b_vtable_t n00b_vm_vtable;
extern const n00b_vtable_t n00b_parse_node_vtable;
extern const n00b_vtable_t n00b_callback_vtable;
extern const n00b_vtable_t n00b_flags_vtable;
extern const n00b_vtable_t n00b_box_vtable;
extern const n00b_vtable_t n00b_basic_http_vtable;
extern const n00b_vtable_t n00b_datetime_vtable;
extern const n00b_vtable_t n00b_date_vtable;
extern const n00b_vtable_t n00b_time_vtable;
extern const n00b_vtable_t n00b_size_vtable;
extern const n00b_vtable_t n00b_duration_vtable;
extern const n00b_vtable_t n00b_grammar_vtable;
extern const n00b_vtable_t n00b_grammar_rule_vtable;
extern const n00b_vtable_t n00b_terminal_vtable;
extern const n00b_vtable_t n00b_nonterm_vtable;
extern const n00b_vtable_t n00b_parser_vtable;
extern const n00b_vtable_t n00b_gopt_parser_vtable;
extern const n00b_vtable_t n00b_gopt_command_vtable;
extern const n00b_vtable_t n00b_gopt_option_vtable;
