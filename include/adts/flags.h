#pragma once
#include "n00b.h"

extern n00b_flags_t *n00b_flags_copy(const n00b_flags_t *);
extern n00b_flags_t *n00b_flags_invert(n00b_flags_t *);
extern n00b_flags_t *n00b_flags_add(n00b_flags_t *, n00b_flags_t *);
extern n00b_flags_t *n00b_flags_sub(n00b_flags_t *, n00b_flags_t *);
extern n00b_flags_t *n00b_flags_test(n00b_flags_t *, n00b_flags_t *);
extern n00b_flags_t *n00b_flags_xor(n00b_flags_t *, n00b_flags_t *);
extern bool         n00b_flags_eq(n00b_flags_t *, n00b_flags_t *);
extern uint64_t     n00b_flags_len(n00b_flags_t *);
extern bool         n00b_flags_index(n00b_flags_t *, int64_t);
extern void         n00b_flags_set_index(n00b_flags_t *, int64_t, bool);
