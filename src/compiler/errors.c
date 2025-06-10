#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    n00b_compile_error_t errorid;
    alignas(8) char *name;
    char *message;
    bool  takes_args;
} error_info_t;

static error_info_t error_info[] = {
    [n00b_err_open_module] = {
        n00b_err_open_module,
        "open_file",
        "Could not open the file «i»«#»«/». Reason: «em»«#»«/»",
        true,
    },
    [n00b_err_lex_stray_cr] = {
        n00b_err_lex_stray_cr,
        "stray_cr",
        "Found carriage return ('\r') without a paired newline ('\n')",
        false,
    },
    [n00b_err_lex_eof_in_comment] = {
        n00b_err_lex_eof_in_comment,
        "eof_in_comment",
        "Found end of file when reading a long (/* */ style) comment.",
        false,
    },
    [n00b_err_lex_invalid_char] = {
        n00b_err_lex_invalid_char,
        "invalid_char",
        "Invalid character outside of strings or char constants",
        false,
    },
    [n00b_err_lex_eof_in_string_lit] = {
        n00b_err_lex_eof_in_string_lit,
        "eof_in_string_lit",
        "Found end of file inside a triple-quote string",
        false,
    },
    [n00b_err_lex_nl_in_string_lit] = {
        n00b_err_lex_nl_in_string_lit,
        "nl_in_string_lit",
        "Missing closing quote (\")",
        false,
    },
    [n00b_err_lex_eof_in_char_lit] = {
        n00b_err_lex_eof_in_char_lit,
        "eof_in_char_lit",
        "Unterminated character literal",
        false,
    },
    [n00b_err_lex_nl_in_char_lit] = {
        n00b_err_lex_nl_in_char_lit,
        "nl_in_char_lit",
        "Unterminated character literal",
        false,
    },
    [n00b_err_lex_extra_in_char_lit] = {
        n00b_err_lex_extra_in_char_lit,
        "extra_in_char_lit",
        "Extra character(s) in character literal",
        false,
    },
    [n00b_err_lex_esc_in_esc] = {
        n00b_err_lex_esc_in_esc,
        "esc_in_esc",
        "Escaped backslashes not allowed when specifying a character with "
        "«i»\\x«/», «i»\\X«/», «i»\\u«/», or «i»\\U«/»",
        false,
    },
    [n00b_err_lex_invalid_float_lit] = {
        n00b_err_lex_invalid_float_lit,
        "invalid_float_lit",
        "Invalid float literal",
        false,
    },
    [n00b_err_lex_float_oflow] = {
        n00b_err_lex_float_oflow,
        "float_oflow",
        "Float value overflows the maximum representable value",
        false,
    },
    [n00b_err_lex_float_uflow] = {
        n00b_err_lex_float_uflow,
        "float_uflow",
        "Float underflow error",
        false,
    },
    [n00b_err_lex_int_oflow] = {
        n00b_err_lex_int_oflow,
        "int_oflow",
        "Integer literal is too large for any data type",
        false,
    },
    [n00b_err_parse_continue_outside_loop] = {
        n00b_err_parse_continue_outside_loop,
        "continue_outside_loop",
        "«em»continue«/» not allowed outside of loop bodies",
        false,
    },
    [n00b_err_parse_break_outside_loop] = {
        n00b_err_parse_break_outside_loop,
        "break_outside_loop",
        "«em»break«/» not allowed outside of loop bodies",
        false,
    },
    [n00b_err_parse_return_outside_func] = {
        n00b_err_parse_return_outside_func,
        "return_outside_func",
        "«em»return«/» not allowed outside of function bodies",
        false,
    },
    [n00b_err_parse_expected_stmt_end] = {
        n00b_err_parse_expected_stmt_end,
        "expected_stmt_end",
        "Expected the end of a statement here",
        false,
    },
    [n00b_err_parse_unexpected_after_expr] = {
        n00b_err_parse_unexpected_after_expr,
        "unexpected_after_expr",
        "Unexpected content after an expression",
        false,
    },
    [n00b_err_parse_expected_brace] = {
        n00b_err_parse_expected_brace,
        "expected_brace",
        "Expected a brace",
        false,
    },
    [n00b_err_parse_expected_range_tok] = {
        n00b_err_parse_expected_range_tok,
        "expected_range_tok",
        "Expected «em»to«/» or «em»:«/» here",
        false,
    },
    [n00b_err_parse_eof] = {
        n00b_err_parse_eof,
        "eof",
        "Unexpected end of file due to unclosed block",
        false,
    },
    [n00b_err_parse_bad_use_uri] = {
        n00b_err_parse_bad_use_uri,
        "bad_use_uri",
        "The URI after «em»use ... from«/» must be a quoted string literal",
        false,
    },
    [n00b_err_parse_id_expected] = {
        n00b_err_parse_id_expected,
        "id_expected",
        "Expected a variable name or other identifier here",
        false,
    },
    [n00b_err_parse_id_member_part] = {
        n00b_err_parse_id_member_part,
        "id_member_part",
        "Expected a (possibly dotted) name here",
        false,
    },
    [n00b_err_parse_not_docable_block] = {
        n00b_err_parse_not_docable_block,
        "not_docable_block",
        "Found documentation in a block that does not use documentation",
        false,
    },
    [n00b_err_parse_for_syntax] = {
        n00b_err_parse_for_syntax,
        "for_syntax",
        "Invalid syntax in «em»for«/» statement",
        false,
    },
    [n00b_err_parse_missing_type_rbrak] = {
        n00b_err_parse_missing_type_rbrak,
        "missing_type_rbrak",
        "Missing right bracket («em»]«/») in type specifier",
        false,
    },
    [n00b_err_parse_bad_tspec] = {
        n00b_err_parse_bad_tspec,
        "bad_tspec",
        "Invalid symbol found in type specifier",
        false,
    },
    [n00b_err_parse_vararg_wasnt_last_thing] = {
        n00b_err_parse_vararg_wasnt_last_thing,
        "vararg_wasnt_last_thing",
        "Variable argument specifier («em»*«/» can only appear in final "
        "function parameter",
        false,
    },
    [n00b_err_parse_fn_param_syntax] = {
        n00b_err_parse_fn_param_syntax,
        "fn_param_syntax",
        "Invalid syntax for a «em»parameter«/» block",
        false,
    },
    [n00b_err_parse_enums_are_toplevel] = {
        n00b_err_parse_enums_are_toplevel,
        "enums_are_toplevel",
        "Enumerations are only allowed at the top-level of a file",
        false,
    },
    [n00b_err_parse_funcs_are_toplevel] = {
        n00b_err_parse_funcs_are_toplevel,
        "funcs_are_toplevel",
        "Functions are only allowed at the top-level of a file",
        false,
    },
    [n00b_err_parse_parameter_is_toplevel] = {
        n00b_err_parse_parameter_is_toplevel,
        "parameter_is_toplevel",
        "Parameter blocks are only allowed at the top-level of a file",
        false,
    },
    [n00b_err_parse_extern_is_toplevel] = {
        n00b_err_parse_extern_is_toplevel,
        "extern_is_toplevel",
        "Extern blocks are only allowed at the top-level of a file",
        false,
    },
    [n00b_err_parse_confspec_is_toplevel] = {
        n00b_err_parse_confspec_is_toplevel,
        "Confspec blocks are only allowed at the top-level of a file",
        "",
        false,
    },
    [n00b_err_parse_bad_confspec_sec_type] = {
        n00b_err_parse_bad_confspec_sec_type,
        "bad_confspec_sec_type",
        "Expected a «i»confspec«/» type («i»named, singleton or root«/»), "
        "but got «em»«#»«/»",
        true,
    },
    [n00b_err_parse_invalid_token_in_sec] = {
        n00b_err_parse_invalid_token_in_sec,
        "invalid_token_in_sec",
        "Invalid symbol in section block",
        false,
    },
    [n00b_err_parse_expected_token] = {
        n00b_err_parse_expected_token,
        "expected_token",
        "Expected a «em»«#»«/» token here",
        true,
    },
    [n00b_err_parse_invalid_sec_part] = {
        n00b_err_parse_invalid_sec_part,
        "invalid_sec_part",
        "Invalid in a section property",
        true,
    },
    [n00b_err_parse_invalid_field_part] = {
        n00b_err_parse_invalid_field_part,
        "invalid_field_part",
        "Invalid in a field property",
        false,
    },
    [n00b_err_parse_no_empty_tuples] = {
        n00b_err_parse_no_empty_tuples,
        "no_empty_tuples",
        "Empty tuples are not allowed",
        false,
    },
    [n00b_err_parse_lit_or_id] = {
        n00b_err_parse_lit_or_id,
        "lit_or_id",
        "Expected either a literal or an identifier",
        false,
    },
    [n00b_err_parse_1_item_tuple] = {
        n00b_err_parse_1_item_tuple,
        "1_item_tuple",
        "Tuples with only one item are not allowed",
        false,
    },
    [n00b_err_parse_decl_kw_x2] = {
        n00b_err_parse_decl_kw_x2,
        "decl_kw_x2",
        "Duplicate declaration keyword",
        false,
    },
    [n00b_err_parse_decl_2_scopes] = {
        n00b_err_parse_decl_2_scopes,
        "decl_2_scopes",
        "Invalid declaration; cannot be both global and local («em»var«/»)",
        false,
    },
    [n00b_err_parse_decl_const_not_const] = {
        n00b_err_parse_decl_const_not_const,
        "const_not_const",
        "Cannot declare variables to be both "
        "«em»once«/» (each instantiation can be set dynamically once) and "
        "«em»const«/» (must have a value that can be fully computed before running).",
        false,
    },
    [n00b_err_parse_case_else_or_end] = {
        n00b_err_parse_case_else_or_end,
        "case_else_or_end",
        "Expected either a new case, an «em»else«/» block, or an ending brace",
        false,
    },
    [n00b_err_parse_case_body_start] = {
        n00b_err_parse_case_body_start,
        "case_body_start",
        "Invalid start for a case body",
        false,
    },
    [n00b_err_parse_empty_enum] = {
        n00b_err_parse_empty_enum,
        "empty_enum",
        "Enumeration cannot be empty",
        false,
    },
    [n00b_err_parse_enum_item] = {
        n00b_err_parse_enum_item,
        "enum_item",
        "Invalid enumeration item",
        false,
    },
    [n00b_err_parse_need_simple_lit] = {
        n00b_err_parse_need_simple_lit,
        "need_simple_lit",
        "Expected a basic literal value",
        false,
    },
    [n00b_err_parse_need_string_lit] = {
        n00b_err_parse_need_string_lit,
        "need_string_lit",
        "Expected a string literal",
        false,
    },
    [n00b_err_parse_need_bool_lit] = {
        n00b_err_parse_need_bool_lit,
        "need_bool_lit",
        "Expected a boolean literal",
        false,
    },
    [n00b_err_parse_formal_expect_id] = {
        n00b_err_parse_formal_expect_id,
        "formal_expect_id",
        "Expect an ID for a formal parameter",
        false,
    },
    [n00b_err_parse_bad_extern_field] = {
        n00b_err_parse_bad_extern_field,
        "bad_extern_field",
        "Bad «em»extern«/» field",
        false,
    },
    [n00b_err_parse_extern_sig_needed] = {
        n00b_err_parse_extern_sig_needed,
        "extern_sig_needed",
        "Signature needed for «em»extern«/» block",
        false,
    },
    [n00b_err_parse_extern_bad_hold_param] = {
        n00b_err_parse_extern_bad_hold_param,
        "extern_bad_hold_param",
        "Invalid value for parameter's «em»hold«/» property",
        false,
    },
    [n00b_err_parse_extern_bad_alloc_param] = {
        n00b_err_parse_extern_bad_alloc_param,
        "extern_bad_alloc_param",
        "Invalid value for parameter's «em»alloc«/» property",
        false,
    },
    [n00b_err_parse_extern_bad_prop] = {
        n00b_err_parse_extern_bad_prop,
        "extern_bad_prop",
        "Invalid property",
        false,
    },
    [n00b_err_parse_extern_dup] = {
        n00b_err_parse_extern_dup,
        "extern_dup",
        "The field «em»«#»«/» cannot appear twice in one «i»extern«/» block.",
        true,
    },
    [n00b_err_parse_extern_need_local] = {
        n00b_err_parse_extern_need_local,
        "extern_need_local",
        "Extern blocks currently define foreign functions only, and require a "
        "«em»local«/» property to define the n00b signature.",
        false,
    },
    [n00b_err_parse_enum_value_type] = {
        n00b_err_parse_enum_value_type,
        "enum_value_type",
        "Enum values must only be integers or strings",
        false,
    },
    [n00b_err_parse_csig_id] = {
        n00b_err_parse_csig_id,
        "csig_id",
        "Expected identifier here for extern signature",
        false,
    },
    [n00b_err_parse_bad_ctype_id] = {
        n00b_err_parse_bad_ctype_id,
        "bad_ctype_id",
        "Invalid «em»ctype«/» identifier",
        false,
    },
    [n00b_err_parse_mod_param_no_const] = {
        n00b_err_parse_mod_param_no_const,
        "mod_param_no_const",
        "«em»const«/» variables may not be used in parameters",
        false,
    },
    [n00b_err_parse_bad_param_start] = {
        n00b_err_parse_bad_param_start,
        "bad_param_start",
        "Invalid start to a parameter block; expected a variable or attribute",
        false,
    },
    [n00b_err_parse_param_def_and_callback] = {
        n00b_err_parse_param_def_and_callback,
        "param_def_and_callback",
        "Parameters cannot contain both «em»default«/» and «em»callback«/»"
        "properties",
        false,
    },
    [n00b_err_parse_param_dupe_prop] = {
        n00b_err_parse_param_dupe_prop,
        "param_dupe_prop",
        "Duplicate parameter property",
        false,
    },
    [n00b_err_parse_param_invalid_prop] = {
        n00b_err_parse_param_invalid_prop,
        "param_invalid_prop",
        "Invalid name for a parameter property",
        false,
    },
    [n00b_err_parse_bad_expression_start] = {
        n00b_err_parse_bad_expression_start,
        "bad_expression_start",
        "Invalid start to an expression",
        false,
    },
    [n00b_err_parse_missing_expression] = {
        n00b_err_parse_missing_expression,
        "missing_expression",
        "Expecting an expression here",
        false,
    },
    [n00b_err_parse_no_lit_mod_match] = {
        n00b_err_parse_no_lit_mod_match,
        "no_lit_mod_match",
        "Could not find a handler for the literal modifier «em»«#»«/» "
        "for literals using «i»«#»«/» syntax.",
        true,
    },
    [n00b_err_parse_invalid_lit_char] = {
        n00b_err_parse_invalid_lit_char,
        "invalid_lit_char",
        "Found a character in this literal that is invalid.",
        false,
    },
    [n00b_err_parse_lit_overflow] = {
        n00b_err_parse_lit_overflow,
        "lit_overflow",
        "Literal value is too large for the data type.",
        false,
    },
    [n00b_err_parse_lit_underflow] = {
        n00b_err_parse_lit_underflow,
        "lit_underflow",
        "Value is too small to be represented in this data type.",
        false,
    },
    [n00b_err_parse_lit_odd_hex] = {
        n00b_err_parse_lit_odd_hex,
        "lit_odd_hex",
        "Hex literals need an even number of digits (one digit is 1/2 a byte).",
        false,
    },
    [n00b_err_parse_lit_invalid_neg] = {
        n00b_err_parse_lit_invalid_neg,
        "lit_invalid_neg",
        "Declared type may not have a negative value.",
        false,
    },
    [n00b_err_parse_for_assign_vars] = {
        n00b_err_parse_for_assign_vars,
        "for_assign_vars",
        "Too many assignment variables in «em»for«/» loop. Must have one in "
        "most cases, and two when iterating over a dictionary type.",
        false,
    },
    [n00b_err_parse_lit_bad_flags] = {
        n00b_err_parse_lit_bad_flags,
        "lit_bad_flags",
        "Flag literals must currently start with «em»0x«/» and contain only "
        "hex characters after that.",
        false,
    },
    [n00b_err_hex_eos] = {
        n00b_err_hex_eos,
        "hex_unfinished",
        "Hex literal was not completed before reaching the end of the string.",
        false,
    },
    [n00b_err_hex_missing] = {
        n00b_err_hex_missing,
        "hex_missing",
        "Escape sequence indicated a hex literal, but no hex digits followed.",
        false,
    },
    [n00b_err_hex_x] = {
        n00b_err_hex_x,
        "invalid_hex",
        "«i»\\x«/» escapes in strings must be followed by two hex digits.",
        false,
    },
    [n00b_err_hex_u] = {
        n00b_err_hex_u,
        "invalid_hex",
        "«i»\\u«/» escapes in strings must be followed by four hex digits.",
        false,
    },
    [n00b_err_hex_U] = {
        n00b_err_hex_U,
        "invalid_hex",
        "«i»\\U«/» escapes in strings must be followed by four hex digits.",
        false,
    },
    [n00b_err_invalid_redeclaration] = {
        n00b_err_invalid_redeclaration,
        "invalid_redeclaration",
        "Re-declaration of «em»«#»«/» is not allowed here; "
        "previous declaration of "
        "«#» was here: «i»«#»«/»",
        true,
    },
    [n00b_err_omit_string_enum_value] = {
        n00b_err_omit_string_enum_value,
        "omit_string_enum_value",
        "Cannot omit values for enumerations with string values.",
        false,
    },
    [n00b_err_invalid_enum_lit_type] = {
        n00b_err_invalid_enum_lit_type,
        "invalid_enum_lit_type",
        "Enumerations must contain either integer values or string values."
        " No other values are permitted.",
        false,
    },
    [n00b_err_enum_string_int_mix] = {
        n00b_err_enum_string_int_mix,
        "enum_string_int_mix",
        "Cannot mix string and integer values in one enumeration.",
        false,
    },
    [n00b_err_dupe_enum] = {
        n00b_err_dupe_enum,
        "dupe_enum",
        "Duplicate value in the same «em»enum«/» is not allowed.",
        false,
    },
    [n00b_err_unk_primitive_type] = {
        n00b_err_unk_primitive_type,
        "unk_primitive_type",
        "Type name [=em=][=#=][=/=] is not a known primitive type.",
        true,
    },
    [n00b_err_unk_param_type] = {
        n00b_err_unk_param_type,
        "unk_param_type",
        "Type name is not a known parameterized type.",
        false,
    },
    [n00b_err_no_logring_yet] = {
        n00b_err_no_logring_yet,
        "no_logring_yet",
        "Log rings are not yet implemented.",
        false,
    },
    [n00b_err_no_params_to_hold] = {
        n00b_err_no_params_to_hold,
        "no_params_to_hold",
        "Hold values can't be specified for an imported function without "
        "any parameters.",
        false,
    },
    [n00b_warn_dupe_hold] = {
        n00b_warn_dupe_hold,
        "dupe_hold",
        "The «em»hold«/» property is already specified for this parameter. ",
        false,
    },
    [n00b_warn_dupe_alloc] = {
        n00b_warn_dupe_alloc,
        "dupe_alloc",
        "The «em»alloc«/» property is already specified for this parameter.",
        false,
    },
    [n00b_err_bad_hold_name] = {
        n00b_err_bad_hold_name,
        "bad_hold_name",
        "Parameter name specified for the «em»hold«/» property here was not "
        "listed as a local parameter name.",
        false,
    },
    [n00b_err_bad_alloc_name] = {
        n00b_err_bad_alloc_name,
        "bad_alloc_name",
        "Parameter name specified for the «em»alloc«/» property here was not "
        "listed as a local parameter name.",
        false,
    },
    [n00b_info_dupe_import] = {
        n00b_info_dupe_import,
        "dupe_import",
        "Multiple calls to «em»use«/» with the exact same package. "
        "Each statement runs the module top-level code each time "
        "execution reaches the «em»use«/» statement.",
        false,
    },
    [n00b_warn_dupe_require] = {
        n00b_warn_dupe_require,
        "dupe_require",
        "Duplicate entry in spec for «em»required«/» subsections.",
        false,
    },
    [n00b_warn_dupe_allow] = {
        n00b_warn_dupe_allow,
        "dupe_allow",
        "Duplicate entry in spec for «em»allowed«/» subsections.",
        false,
    },
    [n00b_err_dupe_property] = {
        n00b_err_dupe_property,
        "dupe_property",
        "Duplicate field property provided here.",
        false,
    },
    [n00b_err_missing_type] = {
        n00b_err_missing_type,
        "missing_type",
        "Field specification is missing the required «em»type«/» property.",
        false,
    },
    [n00b_err_default_inconsistent_field] = {
        n00b_err_default_inconsistent_field,
        "inconsistent_field",
        "Type of provided default value («em»«#»«/») is not compatable "
        "with the specified field type («em»«#»«/»)",
        true,
    },
    [n00b_err_validator_inconsistent_field] = {
        n00b_err_validator_inconsistent_field,
        "inconsistent_field",
        "Type of provided validator («em»«#»«/») is not compatable "
        "with the specified field type («em»«#»«/»). "
        "Validators must take a string with the full attribute path, and "
        "an object of the appropriate type, and return a string, which "
        "is either an error message, or the empty string if valid.",
        true,
    },
    [n00b_err_range_inconsistent_field] = {
        n00b_err_range_inconsistent_field,
        "inconsistent_field",
        "Range constraints currently can only be applied to fields that are "
        "integers, but the field was specified to be of type «em»«#»«/».",
        true,
    },
    [n00b_err_choice_inconsistent_field] = {
        n00b_err_range_inconsistent_field,
        "inconsistent_field",
        "Choice constraints currently only be applied to fields that are "
        "strings or integers, and the choice field must consist of a list "
        "of items of the proper type.",
        false,
    },
    [n00b_err_type_ptr_range] = {
        n00b_err_type_ptr_range,
        "type_ptr_range",
        "Type supplied from another field cannot have a range constraint.",
        false,
    },
    [n00b_err_type_ptr_choice] = {
        n00b_err_type_ptr_choice,
        "type_ptr_choice",
        "Type supplied from another field cannot have a choice constraint.",
        false,
    },
    [n00b_err_req_and_default] = {
        n00b_err_req_and_default,
        "req_and_default",
        "Fields cannot both be «em»require«/» and «em»default«/»; "
        "the former means the user must supply the field whenever defining "
        "the section; the later fills in a value when the user doesn't "
        "provide one.",
        false,
    },
    [n00b_warn_require_allow] = {
        n00b_warn_require_allow,
        "require_allow",
        "It's redundant to put a subsection on both the «em»required«/» "
        "and «em»allowed«/» lists.",
        false,
    },
    [n00b_err_spec_bool_required] = {
        n00b_err_spec_bool_required,
        "spec_bool_required",
        "Specification field requires a boolean value.",
        false,
    },
    [n00b_err_spec_callback_required] = {
        n00b_err_spec_callback_required,
        "spec_callback_required",
        "Specification field requires a callback literal.",
        false,
    },
    [n00b_warn_dupe_exclusion] = {
        n00b_warn_dupe_exclusion,
        "dupe_exclusion",
        "Redundant entry in field spec for «em»exclusion«/» (same field"
        "is excluded multiple times)",
        false,
    },
    [n00b_err_dupe_spec_field] = {
        n00b_err_dupe_spec_field,
        "dupe_spec_field",
        "Field inside section specification has already been specified.",
        false,
    },
    [n00b_err_dupe_root_section] = {
        n00b_err_dupe_root_section,
        "dupe_root_section",
        "Configuration section root section additions currently "
        "may only appear once in a module.",
        false,
    },
    [n00b_err_dupe_section] = {
        n00b_err_dupe_section,
        "dupe_section",
        "Multiple specifications for the same section are not allowed.",
        false,
    },
    [n00b_err_dupe_confspec] = {
        n00b_err_dupe_confspec,
        "dupe_confspec",
        "Modules may only have a single «em»confspec«/» section.",
        false,
    },
    [n00b_err_dupe_param] = {
        n00b_err_dupe_param,
        "dupe_param",
        "Multiple parameter specifications for the same parameter are not allowed in one module.",
        false,
    },
    [n00b_err_const_param] = {
        n00b_err_const_param,
        "const_param",
        "Module parameters may not be «em»const«/» variables.",
        false,
    },
    [n00b_err_malformed_url] = {
        n00b_err_malformed_url,
        "malformed_url",
        "URL for module path is invalid.",
        false,
    },
    [n00b_warn_no_tls] = {
        n00b_warn_no_tls,
        "no_tls",
        "URL for module path is insecure.",
        false,
    },
    [n00b_err_search_path] = {
        n00b_err_search_path,
        "search_path",
        "Could not find the module «em»«#»«/» in the N00b search path.",
        true,
    },
    [n00b_err_invalid_path] = {
        n00b_err_invalid_path,
        "invalid_path",
        "Invalid characters in module spec.",
        false,
    },
    [n00b_info_recursive_use] = {
        n00b_info_recursive_use,
        "recursive_use",
        "This «em»use«/» creates a cyclic import. Items imported from here "
        "will be available, but if there are analysis conflicts in redeclared "
        "symbols, the errors could end up confusing.",
        false,
    },
    [n00b_err_self_recursive_use] = {
        n00b_err_self_recursive_use,
        "self_recursive_use",

        "«em»use«/» statements are not allowed to directly import the current "
        "module.",
        false,
    },
    [n00b_err_redecl_kind] = {
        n00b_err_redecl_kind,
        "redecl_kind",
        "Global symbol «em»«#»«/» was previously declared as a «i»«#»«/», so "
        "cannot be redeclared as a «i»«#»«/». Previous declaration was: "
        "«b»«#»«/»",
        true,
    },
    [n00b_err_no_redecl] = {
        n00b_err_no_redecl,
        "no_redecl",
        "Redeclaration of «i»«#»«/» «em»«#»«/» is not allowed. Previous "
        "definition's location was: «b»«#»«/»",
        true,
    },
    [n00b_err_redecl_neq_generics] = {
        n00b_err_redecl_neq_generics,
        "redecl_neq_generics",
        "Redeclaration of «em»«#»«/» uses «#» type when "
        "re-declared in a different module (redeclarations that name a "
        "type, must name the exact same time as in other modules). "
        "Previous type was: «em»«#»«/» vs. current type: «em»«#»«/»."
        "Previous definition's location was: «b»«#»«/»",
        true,
    },
    [n00b_err_spec_redef_section] = {
        n00b_err_spec_redef_section,
        "redef_section",
        "Redefinition of «i»confspec«/» sections is not allowed. You can "
        "add data to the «i»root«/» section, but no named sections may "
        "appear twice in any program. Previous declaration of "
        "section «em»«#»«/» was: «b»«#»«/»",
        true,
    },
    [n00b_err_spec_redef_field] = {
        n00b_err_spec_redef_field,
        "redef_field",
        "Redefinition of «i»confspec«/» fields are not allowed. You can "
        "add new fields to the «i»root«/» section, but no new ones. "
        "Previous declaration of field «em»«#»«/» was: «b»«#»«/»",
        true,
    },
    [n00b_err_spec_locked] = {
        n00b_err_spec_locked,
        "spec_locked",
        "The configuration file spec has been programatically locked, "
        "and as such, cannot be changed here.",
        false,
    },
    [n00b_err_dupe_validator] = {
        n00b_err_dupe_validator,
        "dupe_validator",
        "The root section already has a validator; currently only a single "
        "validator is supported.",
        false,
    },
    [n00b_err_decl_mismatch] = {
        n00b_err_decl_mismatch,
        "decl_mismatch",
        "Right-hand side of assignment has a type «em»(«#»)«/» that is not "
        "compatable with the declared type of the left-hand side «em»(«#»)«/». "
        "Previous declaration location is: «i»«#»«/»",
        true,
    },
    [n00b_err_inconsistent_type] = {
        n00b_err_inconsistent_type,
        "inconsistent_type",
        "The type here «em»(«#»)«/» is not compatable with the "
        "declared type of this variable «em»(«#»)«/». "
        "Declaration location is: «i»«#»«/»",
        true,
    },
    [n00b_err_inconsistent_infer_type] = {
        n00b_err_inconsistent_infer_type,
        "inconsistent_type",
        "The previous type, «em»«#»«/», is not compatable with the "
        "type used in this context («em»«#»«/»). ",
        true,
    },
    [n00b_err_inconsistent_item_type] = {
        n00b_err_inconsistent_item_type,
        "inconsistent_item_type",
        "Item types must be either identical to the first item type in "
        " the «#», or must be automatically convertable to that type. "
        "First item type was «em»«#»«/», but this one is «em»«#»«/»",
        true,
    },
    [n00b_err_decl_mask] = {
        n00b_err_decl_mask,
        "decl_mask",
        "This variable cannot be declared in the «em»«#»«/» scope because "
        "there is already a «#»-level declaration in the same module at: «i»«#»",
        true,
    },
    [n00b_warn_attr_mask] = {
        n00b_warn_attr_mask,
        "attr_mask",
        "This module-level declaration shares a name with an attribute."
        "The module definition will be used throughout this module, "
        "and the attribute will not be available.",
        false,
    },
    [n00b_err_attr_mask] = {
        n00b_err_attr_mask,
        "attr_mask",
        "This global declaration is invalid, because it has the same "
        "name as an existing attribute.",
        false,
    },
    [n00b_err_label_target] = {
        n00b_err_label_target,
        "label_target",
        "The label target «em»«#»«/» for this «i»«#»«/» statement is not an "
        "enclosing loop.",
        true,
    },
    [n00b_err_fn_not_found] = {
        n00b_err_fn_not_found,
        "fn_not_found",
        "Could not find an implementation for the function «em»«#»«/».",
        true,
    },
    [n00b_err_num_params] = {
        n00b_err_num_params,
        "num_params",
        "Wrong number of parameters in call to function «em»«#»«/». "
        "Call site used «i»«#»«/» parameters, but the implementation of "
        "that function requires «i»«#»«/» parameters.",
        true,
    },
    [n00b_err_calling_non_fn] = {
        n00b_err_calling_non_fn,
        "calling_non_fn",
        "Cannot call «em»«#»«/»; it is a «i»«#»«/» not a function.",
        true,
    },
    [n00b_err_spec_needs_field] = {
        n00b_err_spec_needs_field,
        "spec_needs_field",
        "Attribute specification says «em»«#»«/» must be a «i»«#»«/». It cannot "
        "be used as a field here.",
        true,
    },
    [n00b_err_field_not_spec] = {
        n00b_err_field_not_spec,
        "field_not_spec",
        "Attribute «em»«#»«/» does not follow the specticiation. The component "
        "«em»«#»«/» is expected to refer to a field, not a "
        "configuration section.",
        true,
    },
    [n00b_err_undefined_section] = {
        n00b_err_undefined_section,
        "undefined_section",
        "The config file spec doesn't support a section named «em»«#»«/».",
        true,
    },
    [n00b_err_section_not_allowed] = {
        n00b_err_section_not_allowed,
        "section_not_allowed",
        "The config file spec does not allow «em»«#»«/» to be a subsection "
        "here.",
        true,
    },
    [n00b_err_slice_on_dict] = {
        n00b_err_slice_on_dict,
        "slice_on_dict",
        "The slice operator is not supported for data types using dictionary "
        "syntax (e.g., dictionaries or sets).",
        false,
    },
    [n00b_err_bad_slice_ix] = {
        n00b_err_bad_slice_ix,
        "bad_slice_ix",
        "The slice operator only supports integer indices.",
        false,
    },
    [n00b_err_dupe_label] = {
        n00b_err_dupe_label,
        "dupe_label",
        "The loop label «em»«#»«/» cannot be used in multiple nested loops. "
        "Note that, in the future, this constraint might apply per-function "
        "context as well.",
        true,
    },
    [n00b_err_iter_name_conflict] = {
        n00b_err_iter_name_conflict,
        "iter_name_conflict",
        "Loop iteration over a dictionary requires two different loop "
        "variable names. Rename one of them.",
        false,
    },
    [n00b_warn_shadowed_var] = {
        n00b_warn_shadowed_var,
        "shadowed_var",
        "Declaration of «em»«#»«/» shadows a «i»«#»«/» that would otherwise "
        "be in scope here. Shadow value's first location: «i»«#»«/»",
        true,
    },
    [n00b_err_dict_one_var_for] = {
        n00b_err_dict_one_var_for,
        "dict_one_var_for",
        "When using «em»for«/» loops to iterate over a dictionary, "
        "you need to define two variables, one to hold the «i»key«/», "
        "the other to hold the associated value.",
        false,
    },
    [n00b_err_future_dynamic_typecheck] = {
        n00b_err_future_dynamic_typecheck,
        "future_dynamic_typecheck",
        "There isn't enough syntactic context at this point for the system "
        "to properly keep track of type info. Usually this happens when "
        "using containers, when we cannot determine, for instance, dict vs. "
        "set vs. list. In the future, this will be fine; in some cases "
        "we will keep more type info, and in others, we will convert to "
        "a runtime check. But for now, please add a type annotation that "
        "at least unambiguously specifies a container type.",
        false,
    },
    [n00b_err_iterate_on_non_container] = {
        n00b_err_iterate_on_non_container,
        "iterate_on_non_container",
        "Cannot iterate over this value, as it is not a container type "
        "(current type is «em»«#»«/»)",
        true,
    },
    [n00b_err_unary_minus_type] = {
        n00b_err_unary_minus_type,
        "unary_minus_type",
        "Unary minus currently only allowed on signed int types.",
        false,
    },
    [n00b_err_cannot_cmp] = {
        n00b_err_cannot_cmp,
        "cannot_cmp",
        "The two sides of the comparison have incompatible types: "
        "«em»«#»«/» vs «em»«#»«/»",
        true,
    },
    [n00b_err_range_type] = {
        n00b_err_range_type,
        "range_type",
        "Ranges must consist of two int values.",
        false,
    },
    [n00b_err_switch_case_type] = {
        n00b_err_switch_case_type,
        "switch_case_type",
        "This switch branch has a type («em»«#»«/») that doesn't match"
        "The type of the object being switched on («em»«#»«/»)",
        true,
    },
    [n00b_err_concrete_typeof] = {
        n00b_err_concrete_typeof,
        "concrete_typeof",
        "«em»typeof«/» requires the expression to be of a variable type, "
        "but in this context it is always a «em»«#»",
        true,
    },
    [n00b_warn_type_overlap] = {
        n00b_warn_type_overlap,
        "type_overlap",
        "This case in the «em»typeof«/» statement has a type «em»«#»«/» "
        "that overlaps with a previous case type: «em»«#»«/»",
        true,
    },
    [n00b_warn_empty_case] = {
        n00b_warn_empty_case,
        "empty_case",
        "This case statement body is empty; it will «em»not«/» fall through. "
        "Case conditions can be comma-separated if needed.",
        false,
    },
    [n00b_err_dead_branch] = {
        n00b_err_dead_branch,
        "dead_branch",
        "This type case («em»«#»«/» is not compatable with the constraints "
        "for the variable you're testing (i.e., this is not a subtype)",
        true,
    },
    [n00b_err_no_ret] = {
        n00b_err_no_ret,
        "no_ret",
        "Function was declared with a return type «em»«#»«/», but no "
        "values were returned.",
        true,
    },
    [n00b_err_use_no_def] = {
        n00b_err_use_no_def,
        "use_no_def",
        "Variable is used, but not ever set.",
        false,
    },
    [n00b_err_declared_incompat] = {
        n00b_err_declared_incompat,
        "declared_incompat",
        "The declared type («em»«#»«/») is not compatable with the type as "
        "used in the function («em»«#»«/»)",
        true,
    },
    [n00b_err_too_general] = {
        n00b_err_too_general,
        "too_general",
        "The declared type («em»«#»«/») is more generic than the implementation "
        "requires («em»«#»«/»). Please use the more specific type (or remove "
        "the declaration)",
        true,
    },
    [n00b_warn_unused_param] = {
        n00b_warn_unused_param,
        "unused_param",
        "The parameter «em»«#»«/» is declared, but not used.",
        true,
    },
    [n00b_warn_def_without_use] = {
        n00b_warn_def_without_use,
        "def_without_use",
        "Variable «em»«#»«/» is explicitly declared, but not used.",
        true,
    },
    [n00b_err_call_type_err] = {
        n00b_err_call_type_err,
        "call_type_err",
        "Type inferred at call site («em»«#»«/») is not compatiable with "
        "the implementated type («em»«#»«/»).",
        true,
    },
    [n00b_err_single_def] = {
        n00b_err_single_def,
        "single_def",
        "Variable declared using «em»«#»«/» may only be assigned in a single "
        "location. The first declaration is: «i»«#»«/»",
        true,
    },
    [n00b_warn_unused_decl] = {
        n00b_warn_unused_decl,
        "unused_decl",
        "Declared variable «em»«#»«/» is unused.",
        true,
    },
    [n00b_err_global_remote_def] = {
        n00b_err_global_remote_def,
        "global_remote_def",
        "Global variable «em»«#»«/» can only be set in the module that first "
        "declares it. The symbol's origin is: «i»«#»",
        true,
    },
    [n00b_err_global_remote_unused] = {
        n00b_err_global_remote_unused,
        "global_remote_unused",
        "Global variable «em»«#»«/» is imported via the «i»global«/» keyword, "
        "but is not used here.",
        true,
    },
    [n00b_info_unused_global_decl] = {
        n00b_info_unused_global_decl,
        "unused_global_decl",
        "Global variable is unused.",
        true,
    },
    [n00b_global_def_without_use] = {
        n00b_global_def_without_use,
        "def_without_use",
        "This module is the first to declare the global variable«em»«#»«/», "
        "but it is not explicitly initialized. It will be set to a default "
        "value at the beginning of the program.",
        true,
    },
    [n00b_warn_dead_code] = {
        n00b_warn_dead_code,
        "dead_code",
        "This code below this line is not reachable.",
        false,
    },
    [n00b_cfg_use_no_def] = {
        n00b_cfg_use_no_def,
        "use_no_def",
        "The variable «em»«#»«/» is not defined here, and cannot be used "
        "without a previous assignment.",
        true,
    },
    [n00b_cfg_use_possible_def] = {
        n00b_cfg_use_possible_def,
        "use_possible_def",
        "It is possible for this use of «em»«#»«/» to occur without a previous "
        "assignment to the variable. While program logic might prevent it, "
        "the analysis doesn't see it, so please set a default value in an "
        "encompassing scope.",
        true,
    },
    [n00b_cfg_return_coverage] = {
        n00b_cfg_return_coverage,
        "return_coverage",
        "This function has control paths where no return value is set.",
        false,
    },
    [n00b_err_const_not_provided] = {
        n00b_err_const_not_provided,
        "const_not_provided",
        "«em»«#»«/» is declared «i»const«/», but no value was provided.",
        true,
    },
    [n00b_err_augmented_assign_to_slice] = {
        n00b_err_augmented_assign_to_slice,
        "augmented_assign_to_slice",
        "Only regular assignment to a slice is currently allowed.",
        false,
    },
    [n00b_warn_cant_export] = {
        n00b_warn_cant_export,
        "cant_export",
        "Cannot export this function, because there is already a "
        "global with the same name.",
        false,
    },
    [n00b_err_assigned_void] = {
        n00b_err_assigned_void,
        "assigned_void",
        "Cannot assign the results of a function that returns «em»void«/» "
        "(void explicitly is reserved to indicate the function does not use "
        "return values.",
        false,
    },
    [n00b_err_callback_no_match] = {
        n00b_err_callback_no_match,
        "callback_no_match",
        "Callback does not have a matching declaration. It requires either a "
        "n00b function, or an «em»extern«/» declaration.",
        false,
    },
    [n00b_err_callback_bad_target] = {
        n00b_err_callback_bad_target,
        "callback_bad_target",
        "Callback matches a symbol that is defined, but not as a callable "
        "function. First definition is here: «#»",
        true,
    },
    [n00b_err_callback_type_mismatch] = {
        n00b_err_callback_type_mismatch,
        "callback_type_mismatch",
        "Declared callback type is not compatable with the implementation. "
        "The callback is of type («em»«#»«/»), but the actual type is "
        "«em»«#»«/»). Declaration is here: «#»",
        true,
    },
    [n00b_err_tup_ix] = {
        n00b_err_tup_ix,
        "up_ix",
        "Tuple indices currently must be a literal int value.",
        false,
    },
    [n00b_err_tup_ix_bounds] = {
        n00b_err_tup_ix_bounds,
        "tup_ix_bounds",
        "Tuple index is out of bounds for this tuple of type «em»«#»«/».",
        true,
    },
    [n00b_warn_may_wrap] = {
        n00b_warn_may_wrap,
        "may_wrap",
        "Automatic integer conversion from unsigned to signed of the same size "
        "can turn large positive values negative.",
        false,
    },
    [n00b_internal_type_error] = {
        n00b_internal_type_error,
        "internal_type_error",
        "Could not type check due to an internal error.",
        false,
    },
    [n00b_err_concrete_index] = {
        n00b_err_concrete_index,
        "concrete_index",
        "Currently, n00b does not generate polymorphic code for indexing "
        "operations; it must be able to figure out the type of the index "
        "statically.",
        false,
    },
    [n00b_err_non_dict_index_type] = {
        n00b_err_non_dict_index_type,
        "non_dict_index_type",
        "Only dictionaries can currently use the index operator with non-"
        "integer types.",
        false,
    },
    [n00b_err_invalid_ip] = {
        n00b_err_invalid_ip,
        "invalid_ip",
        "Literal is not a valid IP address.",
        false,
    },
    [n00b_err_last] = {
        n00b_err_last,
        "last",
        "If you see this error, the compiler writer messed up bad",
        false,
    },
    [n00b_err_invalid_dt_spec] = {
        n00b_err_invalid_dt_spec,
        "invalid_dt_spec",
        "Invalid literal for type «em»Datetime",
        false,
    },
    [n00b_err_invalid_date_spec] = {
        n00b_err_invalid_date_spec,
        "invalid_date_spec",
        "Invalid literal for type «em»Date",
        false,
    },
    [n00b_err_invalid_time_spec] = {
        n00b_err_invalid_time_spec,
        "invalid_time_spec",
        "Invalid literal for type «em»Time",
        false,
    },
    [n00b_err_invalid_size_lit] = {
        n00b_err_invalid_size_lit,
        "invalid_size_lit",
        "Invalid literal for type «em»Size",
        false,
    },
    [n00b_err_invalid_duration_lit] = {
        n00b_err_invalid_duration_lit,
        "invalid_duration_lit",
        "Invalid literal for type «em»Duration",
        false,
    },
    [n00b_spec_disallowed_field] = {
        n00b_spec_disallowed_field,
        "disallowed_field",
        "The «em»«#»«/» does not support a field named «em»«#»«/».",
        true,
    },
    [n00b_spec_mutex_field] = {
        n00b_spec_mutex_field,
        "mutex_field",
        "In «em»«#»«/», the field «em»«#»«/» cannot appear in this "
        "section when the field «em»«#»«/» also appears (set at: «#»).",
        true,
    },
    [n00b_spec_missing_ptr] = {
        n00b_spec_missing_ptr,
        "missing_ptr",
        "In «em»«#»«/», the field «em»«#»«/» is supposed to get its type from "
        "the value of field «em»«#»«/», but the later was not provided. "
        "«#»",
        true,
    },
    [n00b_spec_invalid_type_ptr] = {
        n00b_spec_invalid_type_ptr,
        "invalid_type_ptr",
        "In «em»«#»«/», the field «em»«#»«/» is supposed to get its type "
        "from the value of field «em»«#»«/», but that field is not a type "
        "specification; it is an object of type «em»«#» (per «#»)«/»",
        true,
    },
    [n00b_spec_ptr_typecheck] = {
        n00b_spec_ptr_typecheck,
        "ptr_typecheck",
        "In «em»«#»«/», the field «em»«#»«/» is supposed to get its type from "
        "the value of field «em»«#»«/», but the type was actually «em»«#»«/» not "
        "«em»«#»«/» as per «i»«#»«/».",
        true,
    },
    [n00b_spec_field_typecheck] = {
        n00b_spec_field_typecheck,
        "field_typecheck",
        "In «em»«#»«/», the field «em»«#»«/» is supposed to be of type "
        "«em»«#»«/», but it is of type «em»«#»«/»«#».",
        true,
    },
    [n00b_spec_missing_field] = {
        n00b_spec_missing_field,
        "missing_field",
        "The section «#» requires a field named «em»«#»«/», but it is not present.",
        true,
    },
    [n00b_spec_out_of_range] = {
        n00b_spec_out_of_range,
        "out_of_range",
        "Invalid value «em»«#»«/» for the field «em»«#»«/». "
        "The allowed range is from «em»«#»«/» to «em»«#»«/»",
        true,
    },
    [n00b_spec_bad_choice] = {
        n00b_spec_bad_choice,
        "bad_choice",
        "Invalid choice «em»«#»«/» for the field «em»«#»«/». "
        "Allowed choices are: «em»«#»«/»",
        true,
    },
    [n00b_spec_invalid_value] = {
        n00b_spec_invalid_value,
        "invalid_value",
        "The value «em»«#»«/» is invalid for field «em»«#»«/»: «i»«#»",
        true,
    },
    [n00b_spec_disallowed_section] = {
        n00b_spec_disallowed_section,
        "disallowed_section",
        "(spec location) In «em»«#»«/», the section «em»«#»«/» is not an allowed "
        "section type",
        true,
    },
    [n00b_spec_unknown_section] = {
        n00b_spec_unknown_section,
        "unknown_section",
        "(spec location) In «em»«#»«/», attempted to use a section «em»«#»«/» "
        "that is an unknown section type.",
        true,
    },
    [n00b_spec_missing_require] = {
        n00b_spec_missing_require,
        "missing_require",
        "(spec location) In «em»«#»«/», a section of type «em»«#»«/» is "
        "required, but no section with that name was found.",
        true,
    },
    [n00b_spec_blueprint_fields] = {
        n00b_spec_blueprint_fields,
        "blueprint_fields",
        "In «em»«#»«/», fields are not allowed in a toplevel blueprint. "
        "The confspec denoted this section was a section «i»blueprint«/» («#»), "
        "which means it can only be instantiated with section instances. "
        "Yet, «em»«#»«/» is set as a field per «i»«#»«/».",
        true,
    },
#ifdef N00B_DEV
    [n00b_err_void_print] = {
        n00b_err_void_print,
        "void_print",
        "Development «em»print«/» statement must not take a void value.",
        false,
    },
#endif
};

n00b_string_t *
n00b_err_code_to_str(n00b_compile_error_t code)
{
    error_info_t *info = (error_info_t *)&error_info[code];
    return n00b_cstring(info->name);
}

n00b_string_t *
n00b_format_error_message(n00b_compile_error *one_err, bool add_code_name)
{
    // This formats JUST the error message. We look it up in the error
    // info table, and apply styling and arguments, if appropriate.

    error_info_t  *info = (error_info_t *)&error_info[one_err->code];
    n00b_string_t *result;

    if (info->takes_args) {
        n00b_list_t *l = n00b_list(n00b_type_string());

        for (int i = 0; i < one_err->num_args; i++) {
            n00b_list_append(l, one_err->msg_parameters[i]);
        }

        result = n00b_format_arg_list(n00b_cstring(info->message), l);
    }
    else {
        result = n00b_crich(info->message);
    }

    if (add_code_name) {
        n00b_string_t *code = n00b_cformat(" («em»«#»«/»)",
                                           n00b_cstring(info->name));
        result              = n00b_string_concat(result, code);
    }

    return result;
}

static n00b_string_t *error_constant = NULL;
static n00b_string_t *warn_constant  = NULL;
static n00b_string_t *info_constant  = NULL;

static inline n00b_string_t *
format_severity(n00b_compile_error *err)
{
    switch (err->severity) {
    case n00b_err_severity_warning:
        return warn_constant;
    case n00b_err_severity_info:
        return info_constant;
    default:
        return error_constant;
    }
}

n00b_table_t *
n00b_format_runtime_errors(n00b_list_t *errors)
{
    int n = n00b_list_len(errors);
    if (!n) {
        return NULL;
    }

    n00b_table_t *table = n00b_table("columns", 3);

    for (int i = 0; i < n; i++) {
        n00b_compile_error *err = n00b_list_get(errors, i, NULL);
        n00b_string_t      *msg = n00b_format_error_message(err, true);

        n00b_table_add_cell(table, format_severity(err));
        n00b_table_add_cell(table, err->loc);
        n00b_table_add_cell(table, msg);
    }

    n00b_table_end(table);

    return table;
}

static void
n00b_format_module_errors(n00b_module_t *ctx, n00b_table_t *table)
{
    if (error_constant == NULL) {
        error_constant = n00b_crich("«red»error:«/»");
        warn_constant  = n00b_crich("«yellow»warning:«/»");
        info_constant  = n00b_crich("«green»info:«/»");
        n00b_gc_register_root(&error_constant, 1);
        n00b_gc_register_root(&warn_constant, 1);
        n00b_gc_register_root(&info_constant, 1);
    }

    int64_t n = n00b_list_len(ctx->ct->errors);

    if (n == 0) {
        return;
    }

    for (int i = 0; i < n; i++) {
        n00b_compile_error *err = n00b_list_get(ctx->ct->errors, i, NULL);
        n00b_table_add_cell(table, format_severity(err));
        n00b_table_add_cell(table, err->loc);
        n00b_table_add_cell(table, n00b_format_error_message(err, true));
    }
}

n00b_table_t *
n00b_format_errors(n00b_compile_ctx *cctx)
{
    if (!cctx) {
        return NULL;
    }

    n00b_table_t *table       = n00b_table("columns",
                                     n00b_ka(3),
                                     "style",
                                     n00b_ka(N00B_TABLE_SIMPLE));
    int           n           = 0;
    uint64_t      num_modules = 0;

    n00b_table_next_column_fit(table);
    n00b_table_next_column_fit(table);
    n00b_table_next_column_flex(table, 1);

    hatrack_dict_item_t *view = hatrack_dict_items_sort(cctx->module_cache,
                                                        &num_modules);

    for (unsigned int i = 0; i < num_modules; i++) {
        n00b_module_t *ctx = view[i].value;
        if (ctx->ct->errors != NULL) {
            n += n00b_list_len(ctx->ct->errors);
            n00b_format_module_errors(ctx, table);
        }
    }

    if (!n) {
        return NULL;
    }

    return table;
}

n00b_list_t *
n00b_compile_extract_all_error_codes(n00b_compile_ctx *cctx)
{
    n00b_list_t         *result      = n00b_list(n00b_type_ref());
    uint64_t             num_modules = 0;
    hatrack_dict_item_t *view;

    view = hatrack_dict_items_sort(cctx->module_cache, &num_modules);

    for (unsigned int i = 0; i < num_modules; i++) {
        n00b_module_t *ctx = view[i].value;

        if (!ctx->ct) {
            continue;
        }
        if (ctx->ct->errors != NULL) {
            int n = n00b_list_len(ctx->ct->errors);
            for (int j = 0; j < n; j++) {
                n00b_compile_error *err = n00b_list_get(ctx->ct->errors, j, NULL);

                n00b_list_append(result, (void *)(uint64_t)err->code);
            }
        }
    }

    return result;
}

void
n00b_err_set_gc_bits(uint64_t *bitmap, n00b_compile_error *alloc)
{
    n00b_mark_raw_to_addr(bitmap, alloc, &alloc->msg_parameters);
}

n00b_compile_error *
n00b_new_error(int nargs)
{
    return n00b_gc_flex_alloc(n00b_compile_error,
                              void *,
                              nargs,
                              N00B_GC_SCAN_ALL);
}

#define COMMON_ERR_BASE()                                           \
    va_list arg_counter;                                            \
    int     num_args = 0;                                           \
                                                                    \
    va_copy(arg_counter, args);                                     \
    while (va_arg(arg_counter, void *) != NULL) {                   \
        num_args++;                                                 \
    }                                                               \
    va_end(arg_counter);                                            \
                                                                    \
    n00b_compile_error *err = n00b_new_error(num_args);             \
    err->code               = code;                                 \
                                                                    \
    if (num_args) {                                                 \
        for (int i = 0; i < num_args; i++) {                        \
            err->msg_parameters[i] = va_arg(args, n00b_string_t *); \
        }                                                           \
        err->num_args = num_args;                                   \
    }                                                               \
                                                                    \
    n00b_list_append(err_list, err)

n00b_compile_error *
n00b_base_runtime_error(n00b_list_t         *err_list,
                        n00b_compile_error_t code,
                        n00b_string_t       *location,
                        va_list              args)
{
    COMMON_ERR_BASE();

    err->severity = n00b_err_severity_error;
    err->loc      = location;

    return err;
}

n00b_compile_error *
n00b_base_add_error(n00b_list_t         *err_list,
                    n00b_compile_error_t code,
                    n00b_string_t       *loc,
                    n00b_err_severity_t  severity,
                    va_list              args)
{
    COMMON_ERR_BASE();

    err->loc      = loc;
    err->severity = severity;

    return err;
}

n00b_compile_error *
_n00b_error_from_token(n00b_module_t       *ctx,
                       n00b_compile_error_t code,
                       n00b_token_t        *tok,
                       ...)
{
    n00b_compile_error *result;

    va_list args;
    va_start(args, tok);
    result = n00b_base_add_error(ctx->ct->errors,
                                 code,
                                 n00b_token_get_location_str(tok),
                                 n00b_err_severity_error,
                                 args);
    va_end(args);

    ctx->ct->fatal_errors = 1;

    return result;
}

#define n00b_base_err_decl(func_name, severity_value)                           \
    n00b_compile_error *                                                        \
    func_name(n00b_module_t       *ctx,                                         \
              n00b_compile_error_t code,                                        \
              n00b_tree_node_t    *node,                                        \
              ...)                                                              \
    {                                                                           \
        n00b_compile_error *result;                                             \
        n00b_pnode_t       *pnode = n00b_tree_get_contents(node);               \
                                                                                \
        va_list args;                                                           \
        va_start(args, node);                                                   \
        result = n00b_base_add_error(ctx->ct->errors,                           \
                                     code,                                      \
                                     n00b_token_get_location_str(pnode->token), \
                                     severity_value,                            \
                                     args);                                     \
        va_end(args);                                                           \
                                                                                \
        if (severity_value == n00b_err_severity_error) {                        \
            ctx->ct->fatal_errors = 1;                                          \
        }                                                                       \
                                                                                \
        return result;                                                          \
    }

n00b_base_err_decl(_n00b_add_error, n00b_err_severity_error);
n00b_base_err_decl(_n00b_add_warning, n00b_err_severity_warning);
n00b_base_err_decl(_n00b_add_info, n00b_err_severity_info);

void
_n00b_module_load_error(n00b_module_t       *ctx,
                        n00b_compile_error_t code,
                        ...)
{
    va_list args;

    va_start(args, code);
    n00b_base_add_error(ctx->ct->errors,
                        code,
                        NULL,
                        n00b_err_severity_error,
                        args);
    ctx->ct->fatal_errors = 1;
    va_end(args);
}

void
_n00b_module_load_warn(n00b_module_t       *ctx,
                       n00b_compile_error_t code,
                       ...)
{
    va_list args;

    va_start(args, code);
    n00b_base_add_error(ctx->ct->errors,
                        code,
                        NULL,
                        n00b_err_severity_warning,
                        args);
    va_end(args);
}
