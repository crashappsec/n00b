#pragma once
#include "n00b.h"

// These implementations live in core/init.c
// The API is here for clarity.
extern n00b_string_t *n00b_get_env(n00b_string_t *);
extern void           n00b_set_env(n00b_string_t *, n00b_string_t *);
extern bool           n00b_remove_env(n00b_string_t *);
extern n00b_dict_t   *n00b_environment(void);
