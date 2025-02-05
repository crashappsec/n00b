#pragma once
#include "n00b.h"

typedef struct n00b_spec_field_t {
    union {
        n00b_type_t *type;
        n00b_string_t *type_pointer; // Isn't this redundant now?
    } tinfo;

    // The 'stashed options' field is for holding data specific to the
    // builtin validation routine we run. In our first pass, it will
    // start out as either a partially evaluated literal (for choices)
    // or as a raw parse node to evaluate later (for range nodes).
    //
    // We defer compile-time evaluation when extracting symbols until
    // we have everything explicitly declared available to us, so that
    // we can allow for as much constant folding as possible.  This
    // even includes loading exported symbols from dependent modules.

    n00b_tree_node_t *declaration_node;
    n00b_string_t      *location_string;
    void            *stashed_options;
    n00b_string_t      *name;
    n00b_string_t      *short_doc;
    n00b_string_t      *long_doc;
    n00b_string_t      *deferred_type_field; // The field that contains our type.
    void            *default_value;
    void            *validator;
    n00b_set_t       *exclusions;
    unsigned int     user_def_ok       : 1;  // This shouldn't be here???
    unsigned int     hidden            : 1;
    unsigned int     required          : 1;
    unsigned int     lock_on_write     : 1;
    unsigned int     default_provided  : 1;
    unsigned int     validate_range    : 1;
    unsigned int     validate_choice   : 1;
    unsigned int     have_type_pointer : 1;
} n00b_spec_field_t;

typedef struct {
    n00b_tree_node_t *declaration_node;
    n00b_string_t      *location_string;
    n00b_string_t      *name;
    n00b_string_t      *short_doc;
    n00b_string_t      *long_doc;
    n00b_dict_t      *fields;
    n00b_set_t       *allowed_sections;
    n00b_set_t       *required_sections;
    void            *validator;
    unsigned int     singleton   : 1;
    unsigned int     user_def_ok : 1;
    unsigned int     hidden      : 1;
    unsigned int     cycle       : 1;
} n00b_spec_section_t;

typedef struct n00b_spec_t {
    n00b_tree_node_t    *declaration_node;
    n00b_string_t         *short_doc;
    n00b_string_t         *long_doc;
    n00b_spec_section_t *root_section;
    n00b_dict_t         *section_specs;
    unsigned int        locked : 1;
    unsigned int        in_use : 1;
} n00b_spec_t;

typedef enum {
    n00b_attr_invalid,
    n00b_attr_field,
    n00b_attr_user_def_field,
    n00b_attr_object_type,
    n00b_attr_singleton,
    n00b_attr_instance
} n00b_attr_status_t;

typedef enum {
    n00b_attr_no_error,
    n00b_attr_err_sec_under_field,
    n00b_attr_err_field_not_allowed,
    n00b_attr_err_no_such_sec,
    n00b_attr_err_sec_not_allowed,
} n00b_attr_error_t;

typedef struct {
    n00b_string_t *err_arg;
    union {
        n00b_spec_section_t *sec_info;
        n00b_spec_field_t   *field_info;
    } info;
    n00b_attr_error_t  err;
    n00b_attr_status_t kind;
} n00b_attr_info_t;
