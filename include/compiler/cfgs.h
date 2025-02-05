#pragma once
#include "n00b.h"

extern n00b_cfg_node_t *n00b_cfg_enter_block(n00b_cfg_node_t *,
                                             n00b_tree_node_t *);
extern n00b_cfg_node_t *n00b_cfg_exit_block(n00b_cfg_node_t *,
                                            n00b_cfg_node_t *,
                                            n00b_tree_node_t *);
extern n00b_cfg_node_t *n00b_cfg_block_new_branch_node(n00b_cfg_node_t *,
                                                       int,
                                                       n00b_string_t *,
                                                       n00b_tree_node_t *);
extern n00b_cfg_node_t *n00b_cfg_add_return(n00b_cfg_node_t *,
                                            n00b_tree_node_t *,
                                            n00b_cfg_node_t *);
extern n00b_cfg_node_t *n00b_cfg_add_continue(n00b_cfg_node_t *,
                                              n00b_tree_node_t *,
                                              n00b_string_t *);
extern n00b_cfg_node_t *n00b_cfg_add_break(n00b_cfg_node_t *,
                                           n00b_tree_node_t *,
                                           n00b_string_t *);
extern n00b_cfg_node_t *n00b_cfg_add_def(n00b_cfg_node_t *,
                                         n00b_tree_node_t *,
                                         n00b_symbol_t *,
                                         n00b_list_t *);
extern n00b_cfg_node_t *n00b_cfg_add_call(n00b_cfg_node_t *,
                                          n00b_tree_node_t *,
                                          n00b_symbol_t *,
                                          n00b_list_t *);
extern n00b_cfg_node_t *n00b_cfg_add_use(n00b_cfg_node_t *,
                                         n00b_tree_node_t *,
                                         n00b_symbol_t *);
extern n00b_table_t    *n00b_cfg_repr(n00b_cfg_node_t *);
extern void             n00b_cfg_analyze(n00b_module_t *, n00b_dict_t *);

static inline n00b_cfg_node_t *
n00b_cfg_exit_node(n00b_cfg_node_t *block_entry)
{
    return block_entry->contents.block_entrance.exit_node;
}
