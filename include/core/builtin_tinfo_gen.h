#pragma once
#include "n00b/base.h"

// Note: This file is generated; do not edit it. Instead, run:
//    ncpp gen > include/core/builtin_tinfo_gen.h
//
// This should be run from the top level directory of the repo, 
// when you've made changes to the builtin C types.

static inline n00b_ntype_t
n00b_t_error(void)
{
    return 0;
}

static inline bool
n00b_t_is_error(n00b_ntype_t type_id)
{
    return type_id == 0;
}

static inline n00b_ntype_t
n00b_t_void(void)
{
    return 1;
}

static inline bool
n00b_t_is_void(n00b_ntype_t type_id)
{
    return type_id == 1;
}

static inline n00b_ntype_t
n00b_t_nil(void)
{
    return 2;
}

static inline bool
n00b_t_is_nil(n00b_ntype_t type_id)
{
    return type_id == 2;
}

static inline n00b_ntype_t
n00b_t_typespec(void)
{
    return 3;
}

static inline bool
n00b_t_is_typespec(n00b_ntype_t type_id)
{
    return type_id == 3;
}

static inline n00b_ntype_t
n00b_t_internal_tlist(void)
{
    return 4;
}

static inline bool
n00b_t_is_internal_tlist(n00b_ntype_t type_id)
{
    return type_id == 4;
}

static inline n00b_ntype_t
n00b_t_bool(void)
{
    return 5;
}

static inline bool
n00b_t_is_bool(n00b_ntype_t type_id)
{
    return type_id == 5;
}

static inline n00b_ntype_t
n00b_t_i8(void)
{
    return 6;
}

static inline bool
n00b_t_is_i8(n00b_ntype_t type_id)
{
    return type_id == 6;
}

static inline n00b_ntype_t
n00b_t_byte(void)
{
    return 7;
}

static inline bool
n00b_t_is_byte(n00b_ntype_t type_id)
{
    return type_id == 7;
}

static inline n00b_ntype_t
n00b_t_i32(void)
{
    return 8;
}

static inline bool
n00b_t_is_i32(n00b_ntype_t type_id)
{
    return type_id == 8;
}

static inline n00b_ntype_t
n00b_t_char(void)
{
    return 9;
}

static inline bool
n00b_t_is_char(n00b_ntype_t type_id)
{
    return type_id == 9;
}

static inline n00b_ntype_t
n00b_t_u32(void)
{
    return 10;
}

static inline bool
n00b_t_is_u32(n00b_ntype_t type_id)
{
    return type_id == 10;
}

static inline n00b_ntype_t
n00b_t_int(void)
{
    return 11;
}

static inline bool
n00b_t_is_int(n00b_ntype_t type_id)
{
    return type_id == 11;
}

static inline n00b_ntype_t
n00b_t_uint(void)
{
    return 12;
}

static inline bool
n00b_t_is_uint(n00b_ntype_t type_id)
{
    return type_id == 12;
}

static inline n00b_ntype_t
n00b_t_f32(void)
{
    return 13;
}

static inline bool
n00b_t_is_f32(n00b_ntype_t type_id)
{
    return type_id == 13;
}

static inline n00b_ntype_t
n00b_t_float(void)
{
    return 14;
}

static inline bool
n00b_t_is_float(n00b_ntype_t type_id)
{
    return type_id == 14;
}

static inline n00b_ntype_t
n00b_t_string(void)
{
    return 15;
}

static inline bool
n00b_t_is_string(n00b_ntype_t type_id)
{
    return type_id == 15;
}

static inline n00b_ntype_t
n00b_t_table(void)
{
    return 16;
}

static inline bool
n00b_t_is_table(n00b_ntype_t type_id)
{
    return type_id == 16;
}

static inline n00b_ntype_t
n00b_t_buffer(void)
{
    return 17;
}

static inline bool
n00b_t_is_buffer(n00b_ntype_t type_id)
{
    return type_id == 17;
}

static inline n00b_ntype_t
n00b_t_address(void)
{
    return 18;
}

static inline bool
n00b_t_is_address(n00b_ntype_t type_id)
{
    return type_id == 18;
}

static inline n00b_ntype_t
n00b_t_duration(void)
{
    return 19;
}

static inline bool
n00b_t_is_duration(n00b_ntype_t type_id)
{
    return type_id == 19;
}

static inline n00b_ntype_t
n00b_t_size(void)
{
    return 20;
}

static inline bool
n00b_t_is_size(n00b_ntype_t type_id)
{
    return type_id == 20;
}

static inline n00b_ntype_t
n00b_t_datetime(void)
{
    return 21;
}

static inline bool
n00b_t_is_datetime(n00b_ntype_t type_id)
{
    return type_id == 21;
}

static inline n00b_ntype_t
n00b_t_date(void)
{
    return 22;
}

static inline bool
n00b_t_is_date(n00b_ntype_t type_id)
{
    return type_id == 22;
}

static inline n00b_ntype_t
n00b_t_time(void)
{
    return 23;
}

static inline bool
n00b_t_is_time(n00b_ntype_t type_id)
{
    return type_id == 23;
}

static inline n00b_ntype_t
n00b_t_url(void)
{
    return 24;
}

static inline bool
n00b_t_is_url(n00b_ntype_t type_id)
{
    return type_id == 24;
}

static inline n00b_ntype_t
n00b_t_flags(void)
{
    return 25;
}

static inline bool
n00b_t_is_flags(n00b_ntype_t type_id)
{
    return type_id == 25;
}

static inline n00b_ntype_t
n00b_t_hash(void)
{
    return 26;
}

static inline bool
n00b_t_is_hash(n00b_ntype_t type_id)
{
    return type_id == 26;
}

static inline n00b_ntype_t
n00b_t_exception(void)
{
    return 27;
}

static inline bool
n00b_t_is_exception(n00b_ntype_t type_id)
{
    return type_id == 27;
}

static inline n00b_ntype_t
n00b_t_callback(void)
{
    return 28;
}

static inline bool
n00b_t_is_callback(n00b_ntype_t type_id)
{
    return type_id == 28;
}

static inline n00b_ntype_t
n00b_t_memref(void)
{
    return 29;
}

static inline bool
n00b_t_is_memref(n00b_ntype_t type_id)
{
    return type_id == 29;
}

static inline n00b_ntype_t
n00b_t_keyword(void)
{
    return 30;
}

static inline bool
n00b_t_is_keyword(n00b_ntype_t type_id)
{
    return type_id == 30;
}

static inline n00b_ntype_t
n00b_t_vm(void)
{
    return 31;
}

static inline bool
n00b_t_is_vm(n00b_ntype_t type_id)
{
    return type_id == 31;
}

static inline n00b_ntype_t
n00b_t_parse_node(void)
{
    return 32;
}

static inline bool
n00b_t_is_parse_node(n00b_ntype_t type_id)
{
    return type_id == 32;
}

static inline n00b_ntype_t
n00b_t_bit(void)
{
    return 33;
}

static inline bool
n00b_t_is_bit(n00b_ntype_t type_id)
{
    return type_id == 33;
}

static inline n00b_ntype_t
n00b_t_bool_box(void)
{
    return 34;
}

static inline bool
n00b_t_is_bool_box(n00b_ntype_t type_id)
{
    return type_id == 34;
}

static inline n00b_ntype_t
n00b_t_i8_box(void)
{
    return 35;
}

static inline bool
n00b_t_is_i8_box(n00b_ntype_t type_id)
{
    return type_id == 35;
}

static inline n00b_ntype_t
n00b_t_u8_box(void)
{
    return 36;
}

static inline bool
n00b_t_is_u8_box(n00b_ntype_t type_id)
{
    return type_id == 36;
}

static inline n00b_ntype_t
n00b_t_i32_box(void)
{
    return 37;
}

static inline bool
n00b_t_is_i32_box(n00b_ntype_t type_id)
{
    return type_id == 37;
}

static inline n00b_ntype_t
n00b_t_char_box(void)
{
    return 38;
}

static inline bool
n00b_t_is_char_box(n00b_ntype_t type_id)
{
    return type_id == 38;
}

static inline n00b_ntype_t
n00b_t_u32_box(void)
{
    return 39;
}

static inline bool
n00b_t_is_u32_box(n00b_ntype_t type_id)
{
    return type_id == 39;
}

static inline n00b_ntype_t
n00b_t_int_box(void)
{
    return 40;
}

static inline bool
n00b_t_is_int_box(n00b_ntype_t type_id)
{
    return type_id == 40;
}

static inline n00b_ntype_t
n00b_t_uint_box(void)
{
    return 41;
}

static inline bool
n00b_t_is_uint_box(n00b_ntype_t type_id)
{
    return type_id == 41;
}

static inline n00b_ntype_t
n00b_t_f32_box(void)
{
    return 42;
}

static inline bool
n00b_t_is_f32_box(n00b_ntype_t type_id)
{
    return type_id == 42;
}

static inline n00b_ntype_t
n00b_t_f64_box(void)
{
    return 43;
}

static inline bool
n00b_t_is_f64_box(n00b_ntype_t type_id)
{
    return type_id == 43;
}

static inline n00b_ntype_t
n00b_t_http(void)
{
    return 44;
}

static inline bool
n00b_t_is_http(n00b_ntype_t type_id)
{
    return type_id == 44;
}

static inline n00b_ntype_t
n00b_t_parser(void)
{
    return 45;
}

static inline bool
n00b_t_is_parser(n00b_ntype_t type_id)
{
    return type_id == 45;
}

static inline n00b_ntype_t
n00b_t_grammar(void)
{
    return 46;
}

static inline bool
n00b_t_is_grammar(n00b_ntype_t type_id)
{
    return type_id == 46;
}

static inline n00b_ntype_t
n00b_t_terminal(void)
{
    return 47;
}

static inline bool
n00b_t_is_terminal(n00b_ntype_t type_id)
{
    return type_id == 47;
}

static inline n00b_ntype_t
n00b_t_ruleset(void)
{
    return 48;
}

static inline bool
n00b_t_is_ruleset(n00b_ntype_t type_id)
{
    return type_id == 48;
}

static inline n00b_ntype_t
n00b_t_getopt_parser(void)
{
    return 49;
}

static inline bool
n00b_t_is_getopt_parser(n00b_ntype_t type_id)
{
    return type_id == 49;
}

static inline n00b_ntype_t
n00b_t_getopt_command(void)
{
    return 50;
}

static inline bool
n00b_t_is_getopt_command(n00b_ntype_t type_id)
{
    return type_id == 50;
}

static inline n00b_ntype_t
n00b_t_getopt_option(void)
{
    return 51;
}

static inline bool
n00b_t_is_getopt_option(n00b_ntype_t type_id)
{
    return type_id == 51;
}

static inline n00b_ntype_t
n00b_t_mutex(void)
{
    return 52;
}

static inline bool
n00b_t_is_mutex(n00b_ntype_t type_id)
{
    return type_id == 52;
}

static inline n00b_ntype_t
n00b_t_rw_lock(void)
{
    return 53;
}

static inline bool
n00b_t_is_rw_lock(n00b_ntype_t type_id)
{
    return type_id == 53;
}

static inline n00b_ntype_t
n00b_t_condition(void)
{
    return 54;
}

static inline bool
n00b_t_is_condition(n00b_ntype_t type_id)
{
    return type_id == 54;
}

static inline n00b_ntype_t
n00b_t_stream(void)
{
    return 55;
}

static inline bool
n00b_t_is_stream(n00b_ntype_t type_id)
{
    return type_id == 55;
}

static inline n00b_ntype_t
n00b_t_bytering(void)
{
    return 56;
}

static inline bool
n00b_t_is_bytering(n00b_ntype_t type_id)
{
    return type_id == 56;
}

static inline n00b_ntype_t
n00b_t_text_element(void)
{
    return 57;
}

static inline bool
n00b_t_is_text_element(n00b_ntype_t type_id)
{
    return type_id == 57;
}

static inline n00b_ntype_t
n00b_t_box_props(void)
{
    return 58;
}

static inline bool
n00b_t_is_box_props(n00b_ntype_t type_id)
{
    return type_id == 58;
}

static inline n00b_ntype_t
n00b_t_theme(void)
{
    return 59;
}

static inline bool
n00b_t_is_theme(n00b_ntype_t type_id)
{
    return type_id == 59;
}

static inline n00b_ntype_t
n00b_t_regex(void)
{
    return 60;
}

static inline bool
n00b_t_is_regex(n00b_ntype_t type_id)
{
    return type_id == 60;
}

static inline n00b_ntype_t
n00b_t_session(void)
{
    return 61;
}

static inline bool
n00b_t_is_session(n00b_ntype_t type_id)
{
    return type_id == 61;
}

static inline n00b_ntype_t
n00b_t_session_state(void)
{
    return 62;
}

static inline bool
n00b_t_is_session_state(n00b_ntype_t type_id)
{
    return type_id == 62;
}

static inline n00b_ntype_t
n00b_t_trigger(void)
{
    return 63;
}

static inline bool
n00b_t_is_trigger(n00b_ntype_t type_id)
{
    return type_id == 63;
}

static inline n00b_ntype_t
n00b_t_internal(void)
{
    return 64;
}

static inline bool
n00b_t_is_internal(n00b_ntype_t type_id)
{
    return type_id == 64;
}

static inline n00b_ntype_t
n00b_t_va_list(void)
{
    return 65;
}

static inline bool
n00b_t_is_va_list(n00b_ntype_t type_id)
{
    return type_id == 65;
}

static inline bool
n00b_t_is_list(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 67, NULL);
}
static inline bool
n00b_t_is_tuple(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 68, NULL);
}
static inline bool
n00b_t_is_dict(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 69, NULL);
}
static inline bool
n00b_t_is_set(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 70, NULL);
}
static inline bool
n00b_t_is_tree(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 71, NULL);
}
static inline bool
n00b_t_is_function_definition(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 72, NULL);
}
static inline bool
n00b_t_is_ref(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 73, NULL);
}
static inline bool
n00b_t_is_mixed(n00b_ntype_t typeid)
{
    return _n00b_has_base_container_type(typeid, 74, NULL);
}
