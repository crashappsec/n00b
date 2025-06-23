#pragma once

#include "n00b.h"

#ifdef N00B_USE_INTERNAL_API
static inline bool
n00b_sym_is_declared_const(n00b_symbol_t *sym)
{
    return (sym->flags & N00B_F_DECLARED_CONST) != 0;
}

static inline n00b_ntype_t
n00b_get_sym_type(n00b_symbol_t *sym)
{
    return sym->type;
}

static inline bool
n00b_type_is_declared(n00b_symbol_t *sym)
{
    return (bool)(sym->flags & N00B_F_TYPE_IS_DECLARED);
}

extern char *n00b_symbol_kind_names[N00B_SK_NUM_SYM_KINDS];

static inline n00b_string_t *
n00b_sym_kind_name(n00b_symbol_t *sym)
{
    return n00b_cstring(n00b_symbol_kind_names[sym->kind]);
}

#endif
