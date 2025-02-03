#define N00B_USE_INTERNAL_API
#include "n00b.h"

static n00b_utf8_t *
nil_repr(void *ignored)
{
    return n00b_new_utf8("0");
}

static const n00b_vtable_t n00b_type_nil_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_REPR] = (n00b_vtable_entry)nil_repr,
    },

};

const n00b_dt_info_t n00b_base_type_info[N00B_NUM_BUILTIN_DTS] = {
    [N00B_T_ERROR] = {
        .name     = "error",
        .typeid   = N00B_T_ERROR,
        .dt_kind  = N00B_DT_KIND_nil,
        .by_value = true,
    },
    [N00B_T_VOID] = {
        .name     = "void",
        .typeid   = N00B_T_VOID,
        .dt_kind  = N00B_DT_KIND_nil,
        .by_value = true,
    },
    [N00B_T_NIL] = {
        .name     = "nil",
        .typeid   = N00B_T_NIL,
        .vtable   = &n00b_type_nil_vtable,
        .dt_kind  = N00B_DT_KIND_primitive,
        .by_value = true,
    },
    [N00B_T_TYPESPEC] = {
        .name      = "typespec",
        .typeid    = N00B_T_TYPESPEC,
        .alloc_len = sizeof(n00b_type_t),
        .vtable    = &n00b_type_spec_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_INTERNAL_TLIST] = {
        // Used to represent the type of lists of types inside type objects
        // ONLY. Helps prevent complexity during bootstrapping.
        .name    = "internal_tlist",
        .typeid  = N00B_T_INTERNAL_TLIST,
        .dt_kind = N00B_DT_KIND_internal,
    },
    // Should only be used for views on bitfields and similar, where
    // the representation is packed bits. These should be 100%
    // castable back and forth in practice, as long as we know about
    // them.
    [N00B_T_BOOL] = {
        .name      = "bool",
        .typeid    = N00B_T_BOOL,
        .alloc_len = 1,
        .vtable    = &n00b_bool_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_I8] = {
        .name      = "i8",
        .typeid    = N00B_T_I8,
        .alloc_len = 1,
        .vtable    = &n00b_i8_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_BYTE] = {
        .name      = "byte",
        .typeid    = N00B_T_BYTE,
        .alloc_len = 1,
        .vtable    = &n00b_u8_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_I32] = {
        .name      = "i32",
        .typeid    = N00B_T_I32,
        .alloc_len = 4,
        .vtable    = &n00b_i32_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_CHAR] = {
        .name      = "char",
        .typeid    = N00B_T_CHAR,
        .alloc_len = 4,
        .vtable    = &n00b_u32_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_U32] = {
        .name      = "u32",
        .typeid    = N00B_T_U32,
        .alloc_len = 4,
        .vtable    = &n00b_u32_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_INT] = {
        .name      = "int",
        .typeid    = N00B_T_INT,
        .alloc_len = 8,
        .vtable    = &n00b_i64_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_UINT] = {
        .name      = "uint",
        .typeid    = N00B_T_UINT,
        .alloc_len = 8,
        .vtable    = &n00b_u64_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_INT,
        .by_value  = true,
    },
    [N00B_T_F32] = {
        .name      = "f32",
        .typeid    = N00B_T_F32,
        .alloc_len = 4,
        .vtable    = &n00b_float_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    [N00B_T_F64] = {
        .name      = "float",
        .typeid    = N00B_T_F64,
        .alloc_len = 8,
        .vtable    = &n00b_float_type,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_REAL,
        .by_value  = true,
    },
    [N00B_T_UTF8] = {
        .name      = "string",
        .typeid    = N00B_T_UTF8,
        .alloc_len = sizeof(n00b_str_t),
        .vtable    = &n00b_u8str_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
    },
    [N00B_T_BUFFER] = {
        .name      = "buffer",
        .typeid    = N00B_T_BUFFER,
        .alloc_len = sizeof(n00b_buf_t),
        .vtable    = &n00b_buffer_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
        .mutable   = true,
    },
    [N00B_T_UTF32] = {
        .name      = "utf32",
        .typeid    = N00B_T_UTF32,
        .alloc_len = sizeof(n00b_str_t),
        .vtable    = &n00b_u32str_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_CUSTOM,
    },
    [N00B_T_GRID] = {
        .name      = "grid",
        .typeid    = N00B_T_GRID,
        .alloc_len = sizeof(n00b_grid_t),
        .vtable    = &n00b_grid_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_LIST] = {
        .name      = "list",
        .typeid    = N00B_T_XLIST,
        .alloc_len = sizeof(n00b_list_t),
        .vtable    = &n00b_list_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_TUPLE] = {
        .name      = "tuple",
        .typeid    = N00B_T_TUPLE,
        .alloc_len = sizeof(n00b_tuple_t),
        .vtable    = &n00b_tuple_vtable,
        .dt_kind   = N00B_DT_KIND_tuple,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_DICT] = {
        .name      = "dict",
        .typeid    = N00B_T_DICT,
        .alloc_len = sizeof(n00b_dict_t),
        .vtable    = &n00b_dict_vtable,
        .dt_kind   = N00B_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_SET] = {
        .name      = "set",
        .typeid    = N00B_T_SET,
        .alloc_len = sizeof(n00b_set_t),
        .vtable    = &n00b_set_vtable,
        .dt_kind   = N00B_DT_KIND_dict,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_IPV4] = {
        .name      = "Address",
        .typeid    = N00B_T_IPV4,
        .vtable    = &n00b_ipaddr_vtable,
        .alloc_len = sizeof(n00b_net_addr_t),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_DURATION] = {
        .name      = "Duration",
        .typeid    = N00B_T_DURATION,
        .vtable    = &n00b_duration_vtable,
        .alloc_len = sizeof(struct timespec),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_SIZE] = {
        .name      = "Size",
        .typeid    = N00B_T_SIZE,
        .vtable    = &n00b_size_vtable,
        .alloc_len = sizeof(n00b_size_t),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_DATETIME] = {
        .name      = "Datetime",
        .typeid    = N00B_T_DATETIME,
        .vtable    = &n00b_datetime_vtable,
        .alloc_len = sizeof(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_DATE] = {
        .name      = "Date",
        .typeid    = N00B_T_DATE,
        .vtable    = &n00b_date_vtable,
        .alloc_len = sizeof(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_TIME] = {
        .name      = "Time",
        .typeid    = N00B_T_TIME,
        .vtable    = &n00b_time_vtable,
        .alloc_len = sizeof(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_URL] = {
        .name    = "Url",
        .typeid  = N00B_T_URL,
        .dt_kind = N00B_DT_KIND_primitive,
        .hash_fn = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_FLAGS] = {
        .name      = "Flags",
        .typeid    = N00B_T_FLAGS,
        .alloc_len = sizeof(n00b_flags_t),
        .vtable    = &n00b_flags_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_CALLBACK] = {
        .name      = "callback",
        .typeid    = N00B_T_CALLBACK,
        .alloc_len = sizeof(n00b_zcallback_t),
        .vtable    = &n00b_callback_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_QUEUE] = {
        .name      = "Queue",
        .typeid    = N00B_T_QUEUE,
        .alloc_len = sizeof(queue_t),
        .vtable    = &n00b_queue_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_RING] = {
        .name      = "Ring",
        .typeid    = N00B_T_RING,
        .alloc_len = sizeof(hatring_t),
        .vtable    = &n00b_ring_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_LOGRING] = {
        .name      = "Logring",
        .typeid    = N00B_T_LOGRING,
        .alloc_len = sizeof(logring_t),
        .vtable    = &n00b_logring_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_STACK] = {
        .name      = "Stack",
        .typeid    = N00B_T_STACK,
        .alloc_len = sizeof(stack_t),
        .vtable    = &n00b_stack_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_RENDERABLE] = {
        .name      = "Renderable",
        .typeid    = N00B_T_RENDERABLE,
        .alloc_len = sizeof(n00b_renderable_t),
        .vtable    = &n00b_renderable_vtable,
        .dt_kind   = N00B_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_FLIST] = {
        .name      = "Rlist",
        .typeid    = N00B_T_FLIST,
        .alloc_len = sizeof(flexarray_t),
        .vtable    = &n00b_flexarray_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_RENDER_STYLE] = {
        .name      = "Render_style",
        .typeid    = N00B_T_RENDER_STYLE,
        .alloc_len = sizeof(n00b_render_style_t),
        .vtable    = &n00b_render_style_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_SHA] = {
        .name      = "Hash",
        .typeid    = N00B_T_SHA,
        .alloc_len = sizeof(n00b_sha_t),
        .vtable    = &n00b_sha_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_EXCEPTION] = {
        .name      = "exception",
        .typeid    = N00B_T_EXCEPTION,
        .alloc_len = sizeof(n00b_exception_t),
        .vtable    = &n00b_exception_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_TREE] = {
        .name      = "Tree",
        .typeid    = N00B_T_TREE,
        .alloc_len = sizeof(n00b_tree_node_t),
        .vtable    = &n00b_tree_vtable,
        .dt_kind   = N00B_DT_KIND_list,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_FUNCDEF] = {
        // Non-instantiable.
        .name    = "function_definition",
        .typeid  = N00B_T_FUNCDEF,
        .dt_kind = N00B_DT_KIND_func,
    },
    [N00B_T_REF] = {
        // The idea from the library level behind refs is that they
        // will always be pointers, but perhaps not even to one of our
        // heaps.
        //
        // We need to take this into account if we need to dereference
        // something here. Currently, this is only used for holding
        // non-objects internally.
        //
        // Once we add proper references to the language, we might split
        // out such internal references, IDK.
        .name      = "memref",
        .alloc_len = sizeof(void *),
        .typeid    = N00B_T_REF,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_PTR,
    },
    [N00B_T_TRUE_REF] = {
        // Language-level references, which point to either another object,
        // or someplace IN-HEAP.
        //
        // It's parameterized by what it holds.
        .name      = "ref",
        .alloc_len = sizeof(void *),
        .typeid    = N00B_T_TRUE_REF,
        .dt_kind   = N00B_DT_KIND_type_var,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_PTR,
    },
    // This is just for when internal memory gets
    // exposed to the outside; we'll assign refs to it this type.
    [N00B_T_INTERNAL] = {
        .name      = "internal",
        .alloc_len = sizeof(void *),
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_PTR,
        .by_value  = true,

    },
    [N00B_T_GENERIC] = {
        // This is meant for runtime sum types. It's lightly used
        // internally, and we may want to do something more
        // sophisticated when deciding how to support this in the
        // language proper.
        .name      = "mixed",
        .typeid    = N00B_T_GENERIC,
        .alloc_len = sizeof(void *),
        .vtable    = &n00b_mixed_vtable,
        .dt_kind   = N00B_DT_KIND_type_var,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_KEYWORD] = {
        .name      = "keyword",
        .typeid    = N00B_T_KEYWORD,
        .alloc_len = sizeof(n00b_karg_info_t),
        .dt_kind   = N00B_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_VM] = {
        .name      = "vm",
        .typeid    = N00B_T_VM,
        .alloc_len = sizeof(n00b_vm_t),
        .vtable    = &n00b_vm_vtable,
        .mutable   = true,
    },
    [N00B_T_PARSE_NODE] = {
        .name      = "parse_node",
        .typeid    = N00B_T_PARSE_NODE,
        .alloc_len = sizeof(n00b_pnode_t),
        .vtable    = &n00b_parse_node_vtable,
        .dt_kind   = N00B_DT_KIND_internal,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    // Used to represent the type of single bit objects, which
    // basically only applies to bitfields.
    [N00B_T_BIT] = {
        .name    = "bit",
        .typeid  = N00B_T_BIT,
        .dt_kind = N00B_DT_KIND_internal,
    },
    // Boxes are implemented by having their tsi field point to the
    // primitive type they are boxing. We only support boxing of
    // primitivate value types, because there's no need to box
    // anything else.
    [N00B_T_BOX_BOOL] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_BOOL,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_I8] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_I8,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_BYTE] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_BYTE,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_I32] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_I32,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_CHAR] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_CHAR,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_U32] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_U32,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_INT] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_INT,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_UINT] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_UINT,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_F32] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_F32,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_BOX_F64] = {
        .name      = "box",
        .typeid    = N00B_T_BOX_F64,
        .alloc_len = sizeof(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = &n00b_box_vtable,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
    },
    [N00B_T_HTTP] = {
        .name      = "Http",
        .typeid    = N00B_T_HTTP,
        .alloc_len = sizeof(n00b_basic_http_t),
        .vtable    = &n00b_basic_http_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_PARSER] = {
        .name      = "Parser",
        .typeid    = N00B_T_PARSER,
        .alloc_len = sizeof(n00b_parser_t),
        .vtable    = &n00b_parser_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_GRAMMAR] = {
        .name      = "Grammar",
        .typeid    = N00B_T_GRAMMAR,
        .alloc_len = sizeof(n00b_grammar_t),
        .vtable    = &n00b_grammar_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_TERMINAL] = {
        .name      = "Terminal",
        .typeid    = N00B_T_TERMINAL,
        .alloc_len = sizeof(n00b_terminal_t),
        .vtable    = &n00b_terminal_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_RULESET] = {
        .name      = "Ruleset",
        .typeid    = N00B_T_RULESET,
        .alloc_len = sizeof(n00b_nonterm_t),
        .vtable    = &n00b_nonterm_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_GOPT_PARSER] = {
        .name      = "Getopt_parser",
        .typeid    = N00B_T_GOPT_PARSER,
        .alloc_len = sizeof(n00b_gopt_ctx),
        .vtable    = &n00b_gopt_parser_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_GOPT_COMMAND] = {
        .name      = "Getopt_command",
        .typeid    = N00B_T_GOPT_COMMAND,
        .alloc_len = sizeof(n00b_gopt_cspec),
        .vtable    = &n00b_gopt_command_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_GOPT_OPTION] = {
        .name      = "Getopt_option",
        .typeid    = N00B_T_GOPT_OPTION,
        .alloc_len = sizeof(n00b_goption_t),
        .vtable    = &n00b_gopt_option_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_LOCK] = {
        .name      = "lock",
        .typeid    = N00B_T_LOCK,
        .alloc_len = sizeof(n00b_lock_t),
        .vtable    = &n00b_lock_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_CONDITION] = {
        .name      = "condition",
        .typeid    = N00B_T_CONDITION,
        .alloc_len = sizeof(n00b_condition_t),
        .vtable    = &n00b_condition_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_STREAM] = {
        .name      = "stream",
        .typeid    = N00B_T_STREAM,
        .alloc_len = sizeof(n00b_stream_t),
        .vtable    = &n00b_stream_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_MESSAGE] = {
        .name      = "message",
        .typeid    = N00B_T_MESSAGE,
        .alloc_len = sizeof(n00b_message_t),
        .vtable    = &n00b_message_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_BYTERING] = {
        .name      = "bytering",
        .typeid    = N00B_T_BYTERING,
        .alloc_len = sizeof(n00b_bytering_t),
        .vtable    = &n00b_bytering_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
    [N00B_T_FILE] = {
        .name      = "file",
        .typeid    = N00B_T_FILE,
        .alloc_len = sizeof(n00b_stream_t),
        .vtable    = &n00b_file_vtable,
        .dt_kind   = N00B_DT_KIND_primitive,
        .hash_fn   = HATRACK_DICT_KEY_TYPE_OBJ_PTR,
        .mutable   = true,
    },
};

extern int TMP_DEBUG;
extern int n00b_current_test_case;
extern int n00b_watch_case;

#if defined(N00B_GC_STATS) || defined(N00B_DEBUG)
n00b_obj_t
_n00b_new(n00b_heap_t *heap, char *file, int line, n00b_type_t *type, ...)
#else
n00b_obj_t
_n00b_new(n00b_heap_t *heap, n00b_type_t *type, ...)
#endif
{
    n00b_heap_t *__n00b_saved_heap           = NULL;
    bool         clear_thread_heap_reference = false;

    //    // Always use the internal heap for types.
    if (type->typeid == N00B_T_TYPESPEC) {
        __n00b_saved_heap           = n00b_internal_heap;
        clear_thread_heap_reference = true;
    }

    // Thread heap overrides specified heaps.
    if (heap && !n00b_thread_heap) {
        __n00b_saved_heap           = heap;
        clear_thread_heap_reference = true;
    }

    type = n00b_type_resolve(type);

    n00b_obj_t      obj;
    va_list         args;
    n00b_dt_info_t *tinfo     = n00b_type_get_data_type_info(type);
    uint64_t        alloc_len = tinfo->alloc_len;

    if (!tinfo->vtable) {
        obj = n00b_gc_raw_alloc(alloc_len, N00B_GC_SCAN_ALL);

        if (clear_thread_heap_reference) {
            n00b_thread_heap = NULL;
        }

        if (clear_thread_heap_reference) {
            n00b_pop_heap();
        }
        return obj;
    }

    n00b_vtable_entry init_fn = tinfo->vtable->methods[N00B_BI_CONSTRUCTOR];
    n00b_vtable_entry scan_fn = tinfo->vtable->methods[N00B_BI_GC_MAP];

    obj = n00b_halloc(heap, alloc_len, (n00b_mem_scan_fn)scan_fn);

    n00b_alloc_hdr *hdr = &((n00b_alloc_hdr *)obj)[-1];
    hdr->n00b_obj       = true;
    hdr->type           = type;

#if defined(N00B_GC_STATS) || defined(N00B_DEBUG)
    hdr->alloc_file = file;
    hdr->alloc_line = line;
#endif

    if (tinfo->vtable->methods[N00B_BI_FINALIZER] == NULL) {
        hdr->n00b_finalize = true;
    }

    switch (tinfo->dt_kind) {
    case N00B_DT_KIND_primitive:
    case N00B_DT_KIND_internal:
    case N00B_DT_KIND_list:
    case N00B_DT_KIND_dict:
    case N00B_DT_KIND_tuple:
    case N00B_DT_KIND_object:
    case N00B_DT_KIND_box:
        if (init_fn != NULL) {
            va_start(args, type);
            (*init_fn)(obj, args);
            va_end(args);
        }
        break;
    default:
        if (clear_thread_heap_reference) {
            n00b_thread_heap = NULL;
        }

        N00B_CRAISE(
            "Requested type is non-instantiable or not yet "
            "implemented.");
    }

    if (clear_thread_heap_reference) {
        n00b_pop_heap();
    }

    return obj;
}
n00b_str_t *
n00b_repr_explicit(void *item, n00b_type_t *t)
{
    uint64_t     x = n00b_type_resolve(t)->base_index;
    n00b_repr_fn p;

    p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_REPR];

    if (!p) {
        p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_TO_STR];
        if (!p) {
            return n00b_cstr_format("{}@{:x}",
                                    n00b_new_utf8(n00b_base_type_info[x].name),
                                    n00b_box_u64((uint64_t)item));
        }
    }

    n00b_str_t *result = (*p)(item);

    return n00b_to_utf8(result);
}

n00b_utf8_t *
n00b_object_repr_opt(void *item)
{
    if (!n00b_in_heap(item)) {
        return NULL;
    }

    n00b_type_t *t = n00b_get_my_type(item);

    if (!t) {
        return NULL;
    }

    uint64_t     x = t->base_index;
    n00b_repr_fn p;

    p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_REPR];

    if (!p) {
        return NULL;
    }

    return (*p)(item);
}

n00b_str_t *
n00b_value_obj_repr(n00b_obj_t obj)
{
    n00b_type_t *t = n00b_type_resolve(n00b_get_my_type(obj));

    if (n00b_type_is_box(t)) {
        return n00b_repr_explicit(n00b_unbox_obj(obj).v, n00b_type_unbox(t));
    }

    return n00b_repr_explicit(obj, t);
}

n00b_str_t *
n00b_repr(void *item)
{
    if (!n00b_in_heap(item)) {
        return n00b_str_from_int((int64_t)item);
    }

    bool is_obj;
    bool interior;
    bool ptr = false;

    n00b_basic_memory_info(item, &is_obj, &interior);

    if (!is_obj || interior) {
ptr_fmt:
        ptr  = true;
        item = n00b_box_u64((uint64_t)item);
    }
    n00b_type_t *t = n00b_get_my_type(item);

    if (!t) {
        goto ptr_fmt;
    }
    uint64_t     x = t->base_index;
    n00b_repr_fn p;

    if (!n00b_base_type_info[x].vtable) {
        goto ptr_fmt;
    }

    p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_REPR];

    if (!p) {
        p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_TO_STR];
    }

    if (!p) {
        goto ptr_fmt;
    }

    if (ptr) {
        char buf[34] = {
            0,
        };

        sprintf(buf, "@%p", item);

        return n00b_new_utf8(buf);
    }

    n00b_str_t *result = (*p)(item);

    return n00b_to_utf8(result);
}

n00b_str_t *
n00b_value_obj_to_str(n00b_obj_t obj)
{
    n00b_type_t *t = n00b_get_my_type(obj);

    return n00b_to_str(obj, t);
}

n00b_str_t *
n00b_to_str(void *item, n00b_type_t *t)
{
    n00b_dt_info_t *dt = n00b_type_get_data_type_info(t);
    uint64_t        x  = dt->typeid;
    n00b_repr_fn    p;

    switch (x) {
    case 0:
        return n00b_new_utf8("error");
    case 1:
        return n00b_new_utf8("void");
    default:
        break;
    }

    p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_TO_STR];

    if (!p) {
        p = (n00b_repr_fn)n00b_base_type_info[x].vtable->methods[N00B_BI_REPR];

        if (!p) {
            return n00b_cstr_format("{}@{:x}",
                                    n00b_new_utf8(n00b_base_type_info[x].name),
                                    n00b_box_u64((uint64_t)item));
        }
    }

    return (*p)(item);
}

n00b_obj_t
n00b_copy(n00b_obj_t obj)
{
    if (!n00b_in_heap(obj)) {
        return obj;
    }
    n00b_obj_t   result;
    n00b_mem_ptr ptr = {.v = obj};
    ptr.alloc -= 1;

    // If it's a n00b obj, it looks for a copy constructor,
    // or else returns null.
    //
    // If it's in-heap, it makes a deep copy via
    // automarshal.

    if (ptr.alloc->guard == n00b_gc_guard) {
        n00b_copy_fn fn = (n00b_copy_fn)n00b_vtable(obj)->methods[N00B_BI_COPY];
        if (fn != NULL) {
            result = (*fn)(obj);
            n00b_restore(result);
            return result;
        }
        return NULL;
    }

    if (!n00b_in_heap(obj)) {
        return obj;
    }

    return n00b_autounmarshal(n00b_automarshal(obj));
}

n00b_obj_t
n00b_shallow(n00b_obj_t obj)
{
    if (!n00b_in_heap(obj)) {
        return obj;
    }

    n00b_copy_fn fn = (void *)n00b_vtable(obj)->methods[N00B_BI_SHALLOW_COPY];

    if (fn == NULL) {
        N00B_CRAISE("Object type currently does not have a shallow copy fn.");
    }
    return (*fn)(obj);
}

n00b_obj_t
n00b_copy_object_of_type(n00b_obj_t obj, n00b_type_t *t)
{
    if (n00b_type_is_value_type(t)) {
        return obj;
    }

    n00b_copy_fn ptr = (n00b_copy_fn)n00b_vtable(obj)->methods[N00B_BI_COPY];

    if (ptr == NULL) {
        N00B_CRAISE("Copying for this object type not currently supported.");
    }

    return (*ptr)(obj);
}

n00b_obj_t
n00b_add(n00b_obj_t lhs, n00b_obj_t rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_ADD];

    if (ptr == NULL) {
        N00B_CRAISE("Addition not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

n00b_obj_t
n00b_sub(n00b_obj_t lhs, n00b_obj_t rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_SUB];

    if (ptr == NULL) {
        N00B_CRAISE("Subtraction not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

n00b_obj_t
n00b_mul(n00b_obj_t lhs, n00b_obj_t rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_MUL];

    if (ptr == NULL) {
        N00B_CRAISE("Multiplication not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

n00b_obj_t
n00b_div(n00b_obj_t lhs, n00b_obj_t rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_DIV];

    if (ptr == NULL) {
        N00B_CRAISE("Division not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

n00b_obj_t
n00b_mod(n00b_obj_t lhs, n00b_obj_t rhs)
{
    n00b_binop_fn ptr = (n00b_binop_fn)n00b_vtable(lhs)->methods[N00B_BI_MOD];

    if (ptr == NULL) {
        N00B_CRAISE("Modulus not supported for this object type.");
    }

    return (*ptr)(lhs, rhs);
}

int64_t
n00b_len(n00b_obj_t container)
{
    if (!container) {
        return 0;
    }

    n00b_len_fn ptr = (n00b_len_fn)n00b_vtable(container)->methods[N00B_BI_LEN];

    if (ptr == NULL) {
        N00B_CRAISE("Cannot call len on a non-container.");
    }

    return (*ptr)(container);
}

n00b_obj_t
n00b_index_get(n00b_obj_t container, n00b_obj_t index)
{
    n00b_index_get_fn ptr;

    ptr = (n00b_index_get_fn)n00b_vtable(container)->methods[N00B_BI_INDEX_GET];

    if (ptr == NULL) {
        N00B_CRAISE("No index operation available.");
    }

    return (*ptr)(container, index);
}

void
n00b_index_set(n00b_obj_t container, n00b_obj_t index, n00b_obj_t value)
{
    n00b_index_set_fn ptr;

    ptr = (n00b_index_set_fn)n00b_vtable(container)->methods[N00B_BI_INDEX_SET];

    if (ptr == NULL) {
        N00B_CRAISE("No index assignment operation available.");
    }

    (*ptr)(container, index, value);
}

n00b_obj_t
n00b_slice_get(n00b_obj_t container, int64_t start, int64_t end)
{
    n00b_slice_get_fn ptr;

    ptr = (n00b_slice_get_fn)n00b_vtable(container)->methods[N00B_BI_SLICE_GET];

    if (ptr == NULL) {
        N00B_CRAISE("No slice operation available.");
    }

    return (*ptr)(container, start, end);
}

void
n00b_slice_set(n00b_obj_t container, int64_t start, int64_t end, n00b_obj_t o)
{
    n00b_slice_set_fn ptr;

    ptr = (n00b_slice_set_fn)n00b_vtable(container)->methods[N00B_BI_SLICE_SET];

    if (ptr == NULL) {
        N00B_CRAISE("No slice assignment operation available.");
    }

    (*ptr)(container, start, end, o);
}

bool
n00b_can_coerce(n00b_type_t *t1, n00b_type_t *t2)
{
    if (n00b_types_are_compat(t1, t2, NULL)) {
        return true;
    }

    n00b_dt_info_t    *info = n00b_type_get_data_type_info(t1);
    n00b_vtable_t     *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_can_coerce_fn ptr  = (n00b_can_coerce_fn)vtbl->methods[N00B_BI_COERCIBLE];

    if (ptr == NULL) {
        return false;
    }

    return (*ptr)(t1, t2);
}

n00b_obj_t
n00b_coerce(void *data, n00b_type_t *t1, n00b_type_t *t2)
{
    t1 = n00b_resolve_and_unbox(t1);
    t2 = n00b_resolve_and_unbox(t2);

    n00b_dt_info_t *info = n00b_type_get_data_type_info(t1);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;

    n00b_coerce_fn ptr = (n00b_coerce_fn)vtbl->methods[N00B_BI_COERCE];

    if (ptr == NULL) {
        N00B_CRAISE("Invalid conversion between types.");
    }

    return (*ptr)(data, t2);
}

n00b_obj_t
n00b_coerce_object(const n00b_obj_t obj, n00b_type_t *to_type)
{
    n00b_type_t    *from_type = n00b_get_my_type(obj);
    n00b_dt_info_t *info      = n00b_type_get_data_type_info(from_type);
    uint64_t        value;

    if (!info->by_value) {
        return n00b_coerce(obj, from_type, to_type);
    }

    switch (info->alloc_len) {
    case 8:
        value = (uint64_t) * (uint8_t *)obj;
        break;
    case 32:
        value = (uint64_t) * (uint32_t *)obj;
        break;
    default:
        value = *(uint64_t *)obj;
    }

    value        = (uint64_t)n00b_coerce((void *)value, from_type, to_type);
    info         = n00b_type_get_data_type_info(to_type);
    void *result = n00b_new(to_type);

    if (info->alloc_len == 8) {
        *(uint8_t *)result = (uint8_t)value;
    }
    else {
        if (info->alloc_len == 32) {
            *(uint32_t *)result = (uint32_t)value;
        }
        else {
            *(uint64_t *)result = value;
        }
    }

    return result;
}

bool
n00b_eq(n00b_type_t *t, n00b_obj_t o1, n00b_obj_t o2)
{
    n00b_dt_info_t *info = n00b_type_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_EQ];

    if (!ptr) {
        return o1 == o2;
    }

    return (*ptr)(o1, o2);
}

bool
n00b_lt(n00b_type_t *t, n00b_obj_t o1, n00b_obj_t o2)
{
    n00b_dt_info_t *info = n00b_type_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_LT];

    if (!ptr) {
        return o1 < o2;
    }

    return (*ptr)(o1, o2);
}

bool
n00b_gt(n00b_type_t *t, n00b_obj_t o1, n00b_obj_t o2)
{
    n00b_dt_info_t *info = n00b_type_get_data_type_info(t);
    n00b_vtable_t  *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_cmp_fn     ptr  = (n00b_cmp_fn)vtbl->methods[N00B_BI_GT];

    if (!ptr) {
        return o1 > o2;
    }

    return (*ptr)(o1, o2);
}

n00b_type_t *
n00b_get_item_type(n00b_obj_t obj)
{
    n00b_type_t       *t    = n00b_get_my_type(obj);
    n00b_dt_info_t    *info = n00b_type_get_data_type_info(t);
    n00b_vtable_t     *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_ix_item_ty_fn ptr  = (n00b_ix_item_ty_fn)vtbl->methods[N00B_BI_ITEM_TYPE];

    if (!ptr) {
        if (n00b_type_get_num_params(t) == 1) {
            return n00b_type_get_param(t, 0);
        }

        n00b_utf8_t *err = n00b_cstr_format(
            "Type {} is not an indexible container type.",
            t);
        N00B_RAISE(err);
    }

    return (*ptr)(n00b_get_my_type(t));
}

void *
n00b_get_view(n00b_obj_t obj, int64_t *n_items)
{
    n00b_type_t    *t         = n00b_get_my_type(obj);
    n00b_dt_info_t *info      = n00b_type_get_data_type_info(t);
    n00b_vtable_t  *vtbl      = (n00b_vtable_t *)info->vtable;
    void           *itf       = vtbl->methods[N00B_BI_ITEM_TYPE];
    n00b_view_fn    ptr       = (n00b_view_fn)vtbl->methods[N00B_BI_VIEW];
    uint64_t        size_bits = 0x3;

    // If no item type callback is provided, we just assume 8 bytes
    // are produced.  In fact, I currently am not providing callbacks
    // for list types or dicts, since their views are always 64 bits;
    // probably should change the builtin to use an interface that
    // gives back bits, not types (and delete the internal one-bit
    // type).

    if (itf) {
        n00b_type_t    *item_type = n00b_get_item_type(obj);
        n00b_dt_info_t *base      = n00b_type_get_data_type_info(item_type);

        if (base->typeid == N00B_T_BIT) {
            size_bits = 0x7;
        }
        else {
            size_bits = n00b_int_log2((uint64_t)base->alloc_len);
        }
    }

    uint64_t ret_as_int = (uint64_t)(*ptr)(obj, (uint64_t *)n_items);

    ret_as_int |= size_bits;
    return (void *)ret_as_int;
}

n00b_obj_t
n00b_container_literal(n00b_type_t *t, n00b_list_t *items, n00b_utf8_t *mod)
{
    n00b_dt_info_t       *info = n00b_type_get_data_type_info(t);
    n00b_vtable_t        *vtbl = (n00b_vtable_t *)info->vtable;
    n00b_container_lit_fn ptr;

    ptr = (n00b_container_lit_fn)vtbl->methods[N00B_BI_CONTAINER_LIT];

    if (ptr == NULL) {
        n00b_utf8_t *err = n00b_cstr_format(
            "Improper implementation; no literal fn "
            "defined for type '{}'.",
            n00b_new_utf8(info->name));
        N00B_RAISE(err);
    }

    n00b_obj_t result = (*ptr)(t, items, mod);

    if (result == NULL) {
        n00b_utf8_t *err = n00b_cstr_format(
            "Improper implementation; type '{}' did not instantiate "
            "a literal for the literal modifier '{}'",
            n00b_new_utf8(info->name),
            mod);
        N00B_RAISE(err);
    }

    return result;
}

void
n00b_finalize_allocation(n00b_obj_t object)
{
    n00b_system_finalizer_fn fn;
    return;

    fn = (void *)n00b_vtable(object)->methods[N00B_BI_FINALIZER];
    if (fn == NULL) {
    }
    n00b_assert(fn != NULL);
    (*fn)(object);
}

void
n00b_scan_header_only(uint64_t *bitfield, int n)
{
    *bitfield = N00B_HEADER_SCAN_CONST;
}

void *
n00b_autobox(void *ptr)
{
    if (!n00b_in_heap(ptr)) {
        return n00b_box_u64((int64_t)ptr);
    }

    n00b_alloc_hdr *h = ptr;
    --h;

    if (h->guard != n00b_gc_guard) {
        // It's a pointer to the middle of some data structure.
        // TODO here is to check.
        //
        // to see if it seems to be a pointer to something else,
        // and if so, we should wrap it as a reference.

        void **result = n00b_new(n00b_type_ref());
        *result       = ptr;
        return result;
    }
    else {
        return ptr;
    }
}

n00b_type_t *
n00b_get_my_type(n00b_obj_t user_object)
{
    if (n00b_in_heap(user_object)) {
        n00b_mem_ptr p = (n00b_mem_ptr){.v = user_object};
        p.alloc -= 1;
        if (p.alloc->guard == n00b_gc_guard) {
            return p.alloc->type;
        }
        else {
            return n00b_type_internal();
        }
    }
    if (user_object == NULL) {
        return n00b_type_nil();
    }
    return n00b_type_i64();
}
