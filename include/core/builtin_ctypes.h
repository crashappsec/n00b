#pragma once

#include "core/dt_objects.h"

#define N00B_VARIABLE_ALLOC_SZ (~0U)

// "Primitive" (non-parameterized) types all come first.
typedef enum : int64_t {
    N00B_T_ERROR = 0,
    N00B_T_VOID,
    N00B_T_NIL,
    N00B_T_TYPESPEC,
    N00B_T_INTERNAL_TLIST,
    N00B_T_BOOL,
    N00B_T_I8,
    N00B_T_BYTE,
    N00B_T_I32,
    N00B_T_CHAR,
    N00B_T_U32,
    N00B_T_INT,
    N00B_T_UINT,
    N00B_T_F32,
    N00B_T_F64,
    N00B_T_STRING,
    N00B_T_TABLE,
    N00B_T_BUFFER,
    N00B_T_IPV4,
    N00B_T_DURATION,
    N00B_T_SIZE,
    N00B_T_DATETIME,
    N00B_T_DATE,
    N00B_T_TIME,
    N00B_T_URL,
    N00B_T_FLAGS,
    N00B_T_SHA,
    N00B_T_EXCEPTION,
    N00B_T_CALLBACK,
    N00B_T_REF,      // A managed pointer to a non-typed mem object.
    N00B_T_KEYWORD,  // Keyword arg object for internal use.
    N00B_T_VM,
    N00B_T_PARSE_NODE,
    N00B_T_BIT,
    N00B_T_BOX_BOOL,
    N00B_T_BOX_I8,
    N00B_T_BOX_BYTE,
    N00B_T_BOX_I32,
    N00B_T_BOX_CHAR,
    N00B_T_BOX_U32,
    N00B_T_BOX_INT,
    N00B_T_BOX_UINT,
    N00B_T_BOX_F32,
    N00B_T_BOX_F64,
    N00B_T_HTTP,
    N00B_T_PARSER,
    N00B_T_GRAMMAR,
    N00B_T_TERMINAL,
    N00B_T_RULESET,
    N00B_T_GOPT_PARSER,
    N00B_T_GOPT_COMMAND,
    N00B_T_GOPT_OPTION,
    N00B_T_MUTEX,
    N00B_T_RW_LOCK,
    N00B_T_CONDITION,
    N00B_T_STREAM,
    N00B_T_BYTERING,
    N00B_T_TEXT_ELEMENT,
    N00B_T_BOX_PROPS,
    N00B_T_THEME,
    N00B_T_REGEX,
    N00B_T_SESSION,
    N00B_T_SESSION_STATE,
    N00B_T_SESSION_TRIGGER,
    N00B_T_INTERNAL, // Some internal datastructure we don't intend to expose.
    N00B_T_VA_LIST,
    N00B_T_NUM_PRIMITIVE_BUILTINS,
    N00B_T_LIST,
    N00B_T_TUPLE,
    N00B_T_DICT,
    N00B_T_SET,
    N00B_T_TREE,
    N00B_T_FUNCDEF,
    N00B_T_TRUE_REF, // A managed pointer.
    N00B_T_GENERIC,  // If instantiated, instantiates a 'mixed' object.
    N00B_NUM_BUILTIN_DTS,
} n00b_builtin_t;

#ifdef N00B_USE_INTERNAL_API
extern const n00b_vtable_t n00b_type_nil_vtable;
extern const n00b_vtable_t n00b_i8_type;
extern const n00b_vtable_t n00b_u8_type;
extern const n00b_vtable_t n00b_i32_type;
extern const n00b_vtable_t n00b_u32_type;
extern const n00b_vtable_t n00b_i64_type;
extern const n00b_vtable_t n00b_u64_type;
extern const n00b_vtable_t n00b_bool_type;
extern const n00b_vtable_t n00b_float_type;
extern const n00b_vtable_t n00b_buffer_vtable;
extern const n00b_vtable_t n00b_grid_vtable;
extern const n00b_vtable_t n00b_dict_vtable;
extern const n00b_vtable_t n00b_set_vtable;
extern const n00b_vtable_t n00b_list_vtable;
extern const n00b_vtable_t n00b_sha_vtable;
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
extern const n00b_vtable_t n00b_mutex_vtable;
extern const n00b_vtable_t n00b_rwlock_vtable;
extern const n00b_vtable_t n00b_condition_vtable;
extern const n00b_vtable_t n00b_stream_vtable;
extern const n00b_vtable_t n00b_bytering_vtable;
extern const n00b_vtable_t n00b_message_vtable;
extern const n00b_vtable_t n00b_text_element_vtable;
extern const n00b_vtable_t n00b_box_props_vtable;
extern const n00b_vtable_t n00b_theme_vtable;
extern const n00b_vtable_t n00b_string_vtable;
extern const n00b_vtable_t n00b_table_vtable;
extern const n00b_vtable_t n00b_regex_vtable;
extern const n00b_vtable_t n00b_session_vtable;
extern const n00b_vtable_t n00b_session_state_vtable;
extern const n00b_vtable_t n00b_session_trigger_vtable;
#endif
