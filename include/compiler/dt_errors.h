#pragma once
#include "n00b.h"

typedef enum {
    n00b_err_open_module,
    n00b_err_location,
    n00b_err_lex_stray_cr,
    n00b_err_lex_eof_in_comment,
    n00b_err_lex_invalid_char,
    n00b_err_lex_eof_in_str_lit,
    n00b_err_lex_nl_in_str_lit,
    n00b_err_lex_eof_in_char_lit,
    n00b_err_lex_nl_in_char_lit,
    n00b_err_lex_extra_in_char_lit,
    n00b_err_lex_esc_in_esc,
    n00b_err_lex_invalid_float_lit,
    n00b_err_lex_float_oflow,
    n00b_err_lex_float_uflow,
    n00b_err_lex_int_oflow,
    n00b_err_parse_continue_outside_loop,
    n00b_err_parse_break_outside_loop,
    n00b_err_parse_return_outside_func,
    n00b_err_parse_expected_stmt_end,
    n00b_err_parse_unexpected_after_expr,
    n00b_err_parse_expected_brace,
    n00b_err_parse_expected_range_tok,
    n00b_err_parse_eof,
    n00b_err_parse_bad_use_uri,
    n00b_err_parse_id_expected,
    n00b_err_parse_id_member_part,
    n00b_err_parse_not_docable_block,
    n00b_err_parse_for_syntax,
    n00b_err_parse_missing_type_rbrak,
    n00b_err_parse_bad_tspec,
    n00b_err_parse_vararg_wasnt_last_thing,
    n00b_err_parse_fn_param_syntax,
    n00b_err_parse_enums_are_toplevel,
    n00b_err_parse_funcs_are_toplevel,
    n00b_err_parse_parameter_is_toplevel,
    n00b_err_parse_extern_is_toplevel,
    n00b_err_parse_confspec_is_toplevel,
    n00b_err_parse_bad_confspec_sec_type,
    n00b_err_parse_invalid_token_in_sec,
    n00b_err_parse_expected_token,
    n00b_err_parse_invalid_sec_part,
    n00b_err_parse_invalid_field_part,
    n00b_err_parse_no_empty_tuples,
    n00b_err_parse_lit_or_id,
    n00b_err_parse_1_item_tuple,
    n00b_err_parse_decl_kw_x2,
    n00b_err_parse_decl_2_scopes,
    n00b_err_parse_decl_const_not_const,
    n00b_err_parse_case_else_or_end,
    n00b_err_parse_case_body_start,
    n00b_err_parse_empty_enum,
    n00b_err_parse_enum_item,
    n00b_err_parse_need_simple_lit,
    n00b_err_parse_need_str_lit,
    n00b_err_parse_need_bool_lit,
    n00b_err_parse_formal_expect_id,
    n00b_err_parse_bad_extern_field,
    n00b_err_parse_extern_sig_needed,
    n00b_err_parse_extern_bad_hold_param,
    n00b_err_parse_extern_bad_alloc_param,
    n00b_err_parse_extern_bad_prop,
    n00b_err_parse_extern_dup,
    n00b_err_parse_extern_need_local,
    n00b_err_parse_enum_value_type,
    n00b_err_parse_csig_id,
    n00b_err_parse_bad_ctype_id,
    n00b_err_parse_mod_param_no_const,
    n00b_err_parse_bad_param_start,
    n00b_err_parse_param_def_and_callback,
    n00b_err_parse_param_dupe_prop,
    n00b_err_parse_param_invalid_prop,
    n00b_err_parse_bad_expression_start,
    n00b_err_parse_missing_expression,
    n00b_err_parse_no_lit_mod_match,
    n00b_err_parse_invalid_lit_char,
    n00b_err_parse_lit_overflow,
    n00b_err_parse_lit_underflow,
    n00b_err_parse_lit_odd_hex,
    n00b_err_parse_lit_invalid_neg,
    n00b_err_parse_for_assign_vars,
    n00b_err_parse_lit_bad_flags,
    n00b_err_invalid_redeclaration,
    n00b_err_omit_string_enum_value,
    n00b_err_invalid_enum_lit_type,
    n00b_err_enum_str_int_mix,
    n00b_err_dupe_enum,
    n00b_err_unk_primitive_type,
    n00b_err_unk_param_type,
    n00b_err_no_logring_yet,
    n00b_err_no_params_to_hold,
    n00b_warn_dupe_hold,
    n00b_warn_dupe_alloc,
    n00b_err_bad_hold_name,
    n00b_err_bad_alloc_name,
    n00b_info_dupe_import,
    n00b_warn_dupe_require,
    n00b_warn_dupe_allow,
    n00b_err_dupe_property,
    n00b_err_missing_type,
    n00b_err_default_inconsistent_field,
    n00b_err_validator_inconsistent_field,
    n00b_err_range_inconsistent_field,
    n00b_err_choice_inconsistent_field,
    n00b_err_type_ptr_range,
    n00b_err_type_ptr_choice,
    n00b_err_req_and_default,
    n00b_warn_require_allow,
    n00b_err_spec_bool_required,
    n00b_err_spec_callback_required,
    n00b_warn_dupe_exclusion,
    n00b_err_dupe_spec_field,
    n00b_err_dupe_root_section,
    n00b_err_dupe_section,
    n00b_err_dupe_confspec,
    n00b_err_dupe_param,
    n00b_err_const_param,
    n00b_err_malformed_url,
    n00b_warn_no_tls,
    n00b_err_search_path,
    n00b_err_invalid_path,
    n00b_info_recursive_use,
    n00b_err_self_recursive_use,
    n00b_err_redecl_kind,
    n00b_err_no_redecl,
    n00b_err_redecl_neq_generics,
    n00b_err_spec_redef_section,
    n00b_err_spec_redef_field,
    n00b_err_spec_locked,
    n00b_err_dupe_validator,
    n00b_err_decl_mismatch,
    n00b_err_inconsistent_type,
    n00b_err_inconsistent_infer_type,
    n00b_err_inconsistent_item_type,
    n00b_err_decl_mask,
    n00b_warn_attr_mask,
    n00b_err_attr_mask,
    n00b_err_label_target,
    n00b_err_fn_not_found,
    n00b_err_num_params,
    n00b_err_calling_non_fn,
    n00b_err_spec_needs_field,
    n00b_err_field_not_spec,
    n00b_err_field_not_allowed,
    n00b_err_undefined_section,
    n00b_err_section_not_allowed,
    n00b_err_slice_on_dict,
    n00b_err_bad_slice_ix,
    n00b_err_dupe_label,
    n00b_err_iter_name_conflict,
    n00b_err_dict_one_var_for,
    n00b_err_future_dynamic_typecheck,
    n00b_err_iterate_on_non_container,
    n00b_warn_shadowed_var,
    n00b_err_unary_minus_type,
    n00b_err_cannot_cmp,
    n00b_err_range_type,
    n00b_err_switch_case_type,
    n00b_err_concrete_typeof,
    n00b_warn_type_overlap,
    n00b_warn_empty_case,
    n00b_err_dead_branch,
    n00b_err_no_ret,
    n00b_err_use_no_def,
    n00b_err_declared_incompat,
    n00b_err_too_general,
    n00b_warn_unused_param,
    n00b_warn_def_without_use,
    n00b_err_call_type_err,
    n00b_err_single_def,
    n00b_warn_unused_decl,
    n00b_err_global_remote_def,
    n00b_err_global_remote_unused,
    n00b_info_unused_global_decl,
    n00b_global_def_without_use,
    n00b_warn_dead_code,
    n00b_cfg_use_no_def,
    n00b_cfg_use_possible_def,
    n00b_cfg_return_coverage,
    n00b_cfg_no_return,
    n00b_err_const_not_provided,
    n00b_err_augmented_assign_to_slice,
    n00b_warn_cant_export,
    n00b_err_assigned_void,
    n00b_err_callback_no_match,
    n00b_err_callback_bad_target,
    n00b_err_callback_type_mismatch,
    n00b_err_tup_ix,
    n00b_err_tup_ix_bounds,
    n00b_warn_may_wrap,
    n00b_internal_type_error,
    n00b_err_concrete_index,
    n00b_err_non_dict_index_type,
    n00b_err_invalid_ip,
    n00b_err_invalid_dt_spec,
    n00b_err_invalid_date_spec,
    n00b_err_invalid_time_spec,
    n00b_err_invalid_size_lit,
    n00b_err_invalid_duration_lit,
    n00b_spec_disallowed_field,
    n00b_spec_mutex_field,
    n00b_spec_missing_ptr,
    n00b_spec_invalid_type_ptr,
    n00b_spec_ptr_typecheck,
    n00b_spec_field_typecheck,
    n00b_spec_missing_field,
    n00b_spec_disallowed_section,
    n00b_spec_unknown_section,
    n00b_spec_missing_require,
    n00b_spec_out_of_range,
    n00b_spec_bad_choice,
    n00b_spec_invalid_value,
    n00b_spec_blueprint_fields,
#ifdef N00B_DEV
    n00b_err_void_print,
#endif
    n00b_err_last,
} n00b_compile_error_t;

#define n00b_err_no_error n00b_err_last

typedef enum {
    n00b_err_severity_error,
    n00b_err_severity_warning,
    n00b_err_severity_info,
} n00b_err_severity_t;

typedef struct {
    n00b_compile_error_t code;
    // These may turn into a tagged union or transparent pointer with
    // a phase indicator, depending on whether we think we need
    // additional context.
    //
    // the `msg_parameters` field we allocate when we have data we
    // want to put into the error message via substitution. We use $1
    // .. $n, and the formatter will assume the right number of array
    // elements are there based on the values it sees.

    n00b_utf8_t        *loc;
    n00b_str_t         *long_info;
    int32_t            num_args;
    n00b_err_severity_t severity;
    n00b_str_t         *msg_parameters[];
} n00b_compile_error;
