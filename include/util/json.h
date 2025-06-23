#pragma once
#include "n00b.h"

// Second arg is optional; if not-null, the pointed to address will
// receive a NULL if there are no errors, or a list of errors.
//
// When the second argument is provided, error inputs, when the error
// is in int parsing, will still return a value, but zeroed out.
//
// In all other cases, NULL is returned on error.

extern void          *n00b_json_parse(n00b_string_t *, n00b_list_t **);
extern n00b_string_t *n00b_to_json(void *);
