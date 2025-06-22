#define N00B_USE_INTERNAL_API

// NCPP links against this file to read the type names for auto-generating
// code. When NCPP is running, we want the vtables set to NULL, so we don't
// need to link against the world for something we don't even use.
//
// Ergo, vref() really isn't about taking the address of the vtable, it's
// about leaving it out altogether when we're building ncpp.
//
// Same deal with the type size.
#if defined(N00B_NCPP)
#include "core/builtin_ctypes.h"
#define vref(x) NULL
#define tsize(x) 0
#else
#include "n00b.h"
#define vref(x) (&x)
#define tsize(x) sizeof(x)
#endif


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
        .vtable   = vref(n00b_type_nil_vtable),
        .dt_kind  = N00B_DT_KIND_primitive,
        .by_value = true,
    },
    [N00B_T_TYPESPEC] = {
        .name      = "typespec",
        .typeid    = N00B_T_TYPESPEC,
        .alloc_len = tsize(n00b_type_t),
        .vtable    = vref(n00b_type_spec_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
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
        .name       = "bool",
        .typeid     = N00B_T_BOOL,
        .alloc_len  = 1,
        .vtable     = vref(n00b_bool_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .int_bits   = 1,
        .promote_to = N00B_T_I8,
    },
    [N00B_T_I8] = {
        .name       = "i8",
        .typeid     = N00B_T_I8,
        .alloc_len  = 1,
        .vtable     = vref(n00b_i8_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .int_bits   = 8,
        .sign       = true,
        .promote_to = N00B_T_I32,
    },
    [N00B_T_BYTE] = {
        .name       = "byte",
        .typeid     = N00B_T_BYTE,
        .alloc_len  = 1,
        .vtable     = vref(n00b_u8_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .int_bits   = 8,
        .promote_to = N00B_T_U32,
    },
    [N00B_T_I32] = {
        .name       = "i32",
        .typeid     = N00B_T_I32,
        .alloc_len  = 4,
        .vtable     = vref(n00b_i32_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .sign       = true,
        .int_bits   = 32,
        .promote_to = N00B_T_INT,
    },
    [N00B_T_CHAR] = {
        .name       = "char",
        .typeid     = N00B_T_CHAR,
        .alloc_len  = 4,
        .vtable     = vref(n00b_u32_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .int_bits   = 24,
        .promote_to = N00B_T_U32,
    },
    [N00B_T_U32] = {
        .name       = "u32",
        .typeid     = N00B_T_U32,
        .alloc_len  = 4,
        .vtable     = vref(n00b_u32_type),
        .dt_kind    = N00B_DT_KIND_primitive,
        .by_value   = true,
        .int_bits   = 32,
        .promote_to = N00B_T_UINT,
    },
    [N00B_T_INT] = {
        .name      = "int",
        .typeid    = N00B_T_INT,
        .alloc_len = 8,
        .vtable    = vref(n00b_i64_type),
        .dt_kind   = N00B_DT_KIND_primitive,
        .by_value  = true,
        .sign      = true,
        .int_bits  = 64,
    },
    [N00B_T_UINT] = {
        .name      = "uint",
        .typeid    = N00B_T_UINT,
        .alloc_len = 8,
        .vtable    = vref(n00b_u64_type),
        .dt_kind   = N00B_DT_KIND_primitive,
        .by_value  = true,
        .int_bits  = 64,
    },
    [N00B_T_F32] = {
        .name      = "f32",
        .typeid    = N00B_T_F32,
        .alloc_len = 4,
        .vtable    = vref(n00b_float_type),
        .dt_kind   = N00B_DT_KIND_primitive,
        .by_value  = true,
    },
    [N00B_T_F64] = {
        .name      = "float",
        .typeid    = N00B_T_F64,
        .alloc_len = 8,
        .vtable    = vref(n00b_float_type),
        .dt_kind   = N00B_DT_KIND_primitive,
        .by_value  = true,
    },
    [N00B_T_STRING] = {
        .name      = "string",
        .typeid    = N00B_T_STRING,
        .alloc_len = tsize(n00b_string_t),
        .vtable    = vref(n00b_string_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = false,
    },
    [N00B_T_BUFFER] = {
        .name      = "buffer",
        .typeid    = N00B_T_BUFFER,
        .alloc_len = tsize(n00b_buf_t),
        .vtable    = vref(n00b_buffer_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_LIST] = {
        .name      = "list",
        .typeid    = N00B_T_LIST,
        .alloc_len = tsize(n00b_list_t),
        .vtable    = vref(n00b_list_vtable),
        .dt_kind   = N00B_DT_KIND_list,
        .mutable   = true,
    },
    [N00B_T_TUPLE] = {
        .name      = "tuple",
        .typeid    = N00B_T_TUPLE,
        .alloc_len = tsize(n00b_tuple_t),
        .vtable    = vref(n00b_tuple_vtable),
        .dt_kind   = N00B_DT_KIND_tuple,
        .mutable   = true,
    },
    [N00B_T_DICT] = {
        .name      = "dict",
        .typeid    = N00B_T_DICT,
        .alloc_len = tsize(n00b_dict_t),
        .vtable    = vref(n00b_dict_vtable),
        .dt_kind   = N00B_DT_KIND_dict,
        .mutable   = true,
    },
    [N00B_T_SET] = {
        .name      = "set",
        .typeid    = N00B_T_SET,
        .alloc_len = tsize(n00b_set_t),
        .vtable    = vref(n00b_set_vtable),
        .dt_kind   = N00B_DT_KIND_dict,
        .mutable   = true,
    },
    [N00B_T_IPV4] = {
        .name      = "Address",
        .typeid    = N00B_T_IPV4,
        .vtable    = vref(n00b_ipaddr_vtable),
        .alloc_len = tsize(n00b_net_addr_t),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_DURATION] = {
        .name      = "Duration",
        .typeid    = N00B_T_DURATION,
        .vtable    = vref(n00b_duration_vtable),
        .alloc_len = tsize(struct timespec),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_SIZE] = {
        .name      = "Size",
        .typeid    = N00B_T_SIZE,
        .vtable    = vref(n00b_size_vtable),
        .alloc_len = tsize(n00b_size_t),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_DATETIME] = {
        .name      = "Datetime",
        .typeid    = N00B_T_DATETIME,
        .vtable    = vref(n00b_datetime_vtable),
        .alloc_len = tsize(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_DATE] = {
        .name      = "Date",
        .typeid    = N00B_T_DATE,
        .vtable    = vref(n00b_date_vtable),
        .alloc_len = tsize(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_TIME] = {
        .name      = "Time",
        .typeid    = N00B_T_TIME,
        .vtable    = vref(n00b_time_vtable),
        .alloc_len = tsize(n00b_date_time_t),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_URL] = {
        .name    = "Url",
        .typeid  = N00B_T_URL,
        .dt_kind = N00B_DT_KIND_primitive,
    },
    [N00B_T_FLAGS] = {
        .name      = "Flags",
        .typeid    = N00B_T_FLAGS,
        .alloc_len = tsize(n00b_flags_t),
        .vtable    = vref(n00b_flags_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_CALLBACK] = {
        .name      = "callback",
        .typeid    = N00B_T_CALLBACK,
        .alloc_len = tsize(n00b_zcallback_t),
        .vtable    = vref(n00b_callback_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_SHA] = {
        .name      = "Hash",
        .typeid    = N00B_T_SHA,
        .alloc_len = tsize(n00b_sha_t),
        .vtable    = vref(n00b_sha_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_EXCEPTION] = {
        .name      = "exception",
        .typeid    = N00B_T_EXCEPTION,
        .alloc_len = tsize(n00b_exception_t),
        .vtable    = vref(n00b_exception_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_TREE] = {
        .name      = "Tree",
        .typeid    = N00B_T_TREE,
        .alloc_len = tsize(n00b_tree_node_t),
        .vtable    = vref(n00b_tree_vtable),
        .dt_kind   = N00B_DT_KIND_list,
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
        .alloc_len = tsize(void *),
        .typeid    = N00B_T_REF,
        .dt_kind   = N00B_DT_KIND_primitive,
    },
    [N00B_T_TRUE_REF] = {
        // Language-level references, which point to either another object,
        // or someplace IN-HEAP.
        //
        // It's parameterized by what it holds.
        .name      = "ref",
        .alloc_len = tsize(void *),
        .typeid    = N00B_T_TRUE_REF,
        .dt_kind   = N00B_DT_KIND_type_var,
    },
    // This is just for when internal memory gets
    // exposed to the outside; we'll assign refs to it this type.
    [N00B_T_INTERNAL] = {
        .name      = "internal",
        .alloc_len = tsize(void *),
        .dt_kind   = N00B_DT_KIND_primitive,
        .by_value  = true,

    },
    // This shouldn't be instantiated via new(); it is only a type
    // so we can test arbitrary values.
    [N00B_T_VA_LIST] = {
        .name      = "va_list",
        .alloc_len = -1,
        .dt_kind   = N00B_DT_KIND_internal,
    },
    [N00B_T_GENERIC] = {
        // This is meant for runtime sum types. It's lightly used
        // internally, and we may want to do something more
        // sophisticated when deciding how to support this in the
        // language proper.
        .name      = "mixed",
        .typeid    = N00B_T_GENERIC,
        .alloc_len = tsize(void *),
        .vtable    = vref(n00b_mixed_vtable),
        .dt_kind   = N00B_DT_KIND_type_var,
    },
    [N00B_T_KEYWORD] = {
        .name      = "keyword",
        .typeid    = N00B_T_KEYWORD,
        .alloc_len = tsize(n00b_karg_info_t),
        .dt_kind   = N00B_DT_KIND_internal,
    },
    [N00B_T_VM] = {
        .name      = "vm",
        .typeid    = N00B_T_VM,
        .alloc_len = tsize(n00b_vm_t),
        .vtable    = vref(n00b_vm_vtable),
        .mutable   = true,
    },
    [N00B_T_PARSE_NODE] = {
        .name      = "parse_node",
        .typeid    = N00B_T_PARSE_NODE,
        .alloc_len = tsize(n00b_pnode_t),
        .vtable    = vref(n00b_parse_node_vtable),
        .dt_kind   = N00B_DT_KIND_internal,
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
        .name      = "bool_box",
        .typeid    = N00B_T_BOX_BOOL,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_BOOL,
    },
    [N00B_T_BOX_I8] = {
        .name      = "i8_box",
        .typeid    = N00B_T_BOX_I8,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_I8,
    },
    [N00B_T_BOX_BYTE] = {
        .name      = "u8_box",
        .typeid    = N00B_T_BOX_BYTE,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_BYTE,
    },
    [N00B_T_BOX_I32] = {
        .name      = "i32_box",
        .typeid    = N00B_T_BOX_I32,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_I32,
    },
    [N00B_T_BOX_CHAR] = {
        .name      = "char_box",
        .typeid    = N00B_T_BOX_CHAR,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_CHAR,
    },
    [N00B_T_BOX_U32] = {
        .name      = "u32_box",
        .typeid    = N00B_T_BOX_U32,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_U32,
    },
    [N00B_T_BOX_INT] = {
        .name      = "int_box",
        .typeid    = N00B_T_BOX_INT,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_INT,
    },
    [N00B_T_BOX_UINT] = {
        .name      = "uint_box",
        .typeid    = N00B_T_BOX_UINT,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_UINT,
    },
    [N00B_T_BOX_F32] = {
        .name      = "f32_box",
        .typeid    = N00B_T_BOX_F32,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_F32,
    },
    [N00B_T_BOX_F64] = {
        .name      = "f64_box",
        .typeid    = N00B_T_BOX_F64,
        .alloc_len = tsize(n00b_box_t),
        .dt_kind   = N00B_DT_KIND_box,
        .vtable    = vref(n00b_box_vtable),
        .box_id    = N00B_T_F64,
    },
    [N00B_T_HTTP] = {
        .name      = "Http",
        .typeid    = N00B_T_HTTP,
        .alloc_len = tsize(n00b_basic_http_t),
        .vtable    = vref(n00b_basic_http_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_PARSER] = {
        .name      = "Parser",
        .typeid    = N00B_T_PARSER,
        .alloc_len = tsize(n00b_parser_t),
        .vtable    = vref(n00b_parser_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_GRAMMAR] = {
        .name      = "Grammar",
        .typeid    = N00B_T_GRAMMAR,
        .alloc_len = tsize(n00b_grammar_t),
        .vtable    = vref(n00b_grammar_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_TERMINAL] = {
        .name      = "Terminal",
        .typeid    = N00B_T_TERMINAL,
        .alloc_len = tsize(n00b_terminal_t),
        .vtable    = vref(n00b_terminal_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_RULESET] = {
        .name      = "Ruleset",
        .typeid    = N00B_T_RULESET,
        .alloc_len = tsize(n00b_nonterm_t),
        .vtable    = vref(n00b_nonterm_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_GOPT_PARSER] = {
        .name      = "Getopt_parser",
        .typeid    = N00B_T_GOPT_PARSER,
        .alloc_len = tsize(n00b_gopt_ctx),
        .vtable    = vref(n00b_gopt_parser_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_GOPT_COMMAND] = {
        .name      = "Getopt_command",
        .typeid    = N00B_T_GOPT_COMMAND,
        .alloc_len = tsize(n00b_gopt_cspec),
        .vtable    = vref(n00b_gopt_command_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,

        .mutable = true,
    },
    [N00B_T_GOPT_OPTION] = {
        .name      = "Getopt_option",
        .typeid    = N00B_T_GOPT_OPTION,
        .alloc_len = tsize(n00b_goption_t),
        .vtable    = vref(n00b_gopt_option_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_MUTEX] = {
        .name      = "mutex",
        .typeid    = N00B_T_MUTEX,
        .alloc_len = tsize(n00b_mutex_t),
        .vtable    = vref(n00b_mutex_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_RW_LOCK] = {
        .name      = "rw_lock",
        .typeid    = N00B_T_RW_LOCK,
        .alloc_len = tsize(n00b_rwlock_t),
        .vtable    = vref(n00b_rwlock_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_CONDITION] = {
        .name      = "condition",
        .typeid    = N00B_T_CONDITION,
        .alloc_len = tsize(n00b_condition_t),
        .vtable    = vref(n00b_condition_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    // not alloc'd this way, right now.
    [N00B_T_STREAM] = {
        .name      = "stream",
        .typeid    = N00B_T_STREAM,
        .alloc_len = N00B_VARIABLE_ALLOC_SZ,
        .vtable    = vref(n00b_stream_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_BYTERING] = {
        .name      = "bytering",
        .typeid    = N00B_T_BYTERING,
        .alloc_len = tsize(n00b_bytering_t),
        .vtable    = vref(n00b_bytering_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_TEXT_ELEMENT] = {
        .name      = "Text_element",
        .typeid    = N00B_T_TEXT_ELEMENT,
        .alloc_len = tsize(n00b_text_element_t),
        .vtable    = vref(n00b_text_element_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_BOX_PROPS] = {
        .name      = "Box_props",
        .typeid    = N00B_T_BOX_PROPS,
        .alloc_len = tsize(n00b_box_props_t),
        .vtable    = vref(n00b_box_props_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_THEME] = {
        .name      = "theme",
        .typeid    = N00B_T_THEME,
        .alloc_len = tsize(n00b_theme_t),
        .vtable    = vref(n00b_theme_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_TABLE] = {
        .name      = "table",
        .typeid    = N00B_T_TABLE,
        .alloc_len = tsize(n00b_table_t),
        .vtable    = vref(n00b_table_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,

    },
    [N00B_T_REGEX] = {
        .name      = "regex",
        .typeid    = N00B_T_REGEX,
        .alloc_len = tsize(n00b_regex_t),
        .vtable    = vref(n00b_regex_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_SESSION] = {
        .name      = "session",
        .typeid    = N00B_T_SESSION,
        .alloc_len = tsize(n00b_session_t),
        .vtable    = vref(n00b_session_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_SESSION_STATE] = {
        .name      = "Session_state",
        .typeid    = N00B_T_SESSION_STATE,
        .alloc_len = tsize(n00b_list_t),
        .vtable    = vref(n00b_session_state_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = true,
    },
    [N00B_T_SESSION_TRIGGER] = {
        .name      = "Trigger",
        .typeid    = N00B_T_SESSION_TRIGGER,
        .alloc_len = tsize(n00b_trigger_t),
        .vtable    = vref(n00b_session_trigger_vtable),
        .dt_kind   = N00B_DT_KIND_primitive,
        .mutable   = false,
    },
};

