#pragma once

#include "n00b.h"

#ifdef N00B_USE_INTERNAL_API
extern n00b_scope_t  *n00b_new_scope(n00b_scope_t *, n00b_scope_kind);
extern n00b_symbol_t *n00b_declare_symbol(n00b_module_t *,
                                        n00b_scope_t *,
                                        n00b_utf8_t *,
                                        n00b_tree_node_t *,
                                        n00b_symbol_kind,
                                        bool *,
                                        bool);
extern n00b_symbol_t *n00b_lookup_symbol(n00b_scope_t *, n00b_utf8_t *);
extern bool          n00b_merge_symbols(n00b_module_t *,
                                       n00b_symbol_t *,
                                       n00b_symbol_t *);
extern n00b_grid_t   *n00b_format_scope(n00b_scope_t *);
extern void          n00b_shadow_check(n00b_module_t *,
                                      n00b_symbol_t *,
                                      n00b_scope_t *);
extern n00b_symbol_t *n00b_symbol_lookup(n00b_scope_t *,
                                       n00b_scope_t *,
                                       n00b_scope_t *,
                                       n00b_scope_t *,
                                       n00b_utf8_t *);
extern n00b_symbol_t *n00b_add_inferred_symbol(n00b_module_t *,
                                             n00b_scope_t *,
                                             n00b_utf8_t *,
                                             n00b_tree_node_t *);
extern n00b_symbol_t *n00b_add_or_replace_symbol(n00b_module_t *,
                                               n00b_scope_t *,
                                               n00b_utf8_t *,
                                               n00b_tree_node_t *);
extern n00b_utf8_t   *n00b_sym_get_best_ref_loc(n00b_symbol_t *);
extern n00b_scope_t  *n00b_scope_copy(n00b_scope_t *);

static inline bool
n00b_sym_is_declared_const(n00b_symbol_t *sym)
{
    return (sym->flags & N00B_F_DECLARED_CONST) != 0;
}

static inline n00b_type_t *
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

static inline n00b_utf8_t *
n00b_sym_kind_name(n00b_symbol_t *sym)
{
    return n00b_new_utf8(n00b_symbol_kind_names[sym->kind]);
}

#endif
