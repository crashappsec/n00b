#pragma once

#include "n00b.h"
extern void        n00b_universe_init(n00b_type_universe_t *);
extern n00b_type_t *n00b_universe_get(n00b_type_universe_t *, n00b_type_hash_t);
extern bool        n00b_universe_put(n00b_type_universe_t *, n00b_type_t *);
extern bool        n00b_universe_add(n00b_type_universe_t *, n00b_type_t *);
extern n00b_type_t *n00b_universe_attempt_to_add(n00b_type_universe_t *, n00b_type_t *);
extern void        n00b_universe_forward(n00b_type_universe_t *,
                                        n00b_type_t *,
                                        n00b_type_t *);
