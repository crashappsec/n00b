#pragma once
#include "n00b.h"

typedef struct n00b_cfg_node_t n00b_cfg_node_t;

typedef enum {
    n00b_cfg_block_entrance,
    n00b_cfg_block_exit,
    n00b_cfg_node_branch,
    n00b_cfg_use,  // Use outside of any assignment or call.
    n00b_cfg_def,  // Def on the left, the the list of uses on the RHS
    n00b_cfg_call, // A set of uses passed to a call.
    n00b_cfg_jump
} n00b_cfg_node_type;

typedef struct {
    n00b_cfg_node_t *next_node;
    n00b_cfg_node_t *exit_node;
    n00b_list_t     *inbound_links;
    n00b_list_t     *to_merge;
} n00b_cfg_block_enter_info_t;

typedef struct {
    n00b_cfg_node_t *next_node;
    n00b_cfg_node_t *entry_node;
    n00b_list_t     *inbound_links;
    n00b_list_t     *to_merge;
} n00b_cfg_block_exit_info_t;

typedef struct {
    n00b_cfg_node_t *dead_code;
    n00b_cfg_node_t *target;
} n00b_cfg_jump_info_t;

typedef struct {
    n00b_cfg_node_t  *exit_node;
    n00b_cfg_node_t **branch_targets;
    n00b_string_t      *label; // For loops
    int64_t          num_branches;
    int64_t          next_to_process;
} n00b_cfg_branch_info_t;

typedef struct {
    n00b_cfg_node_t *next_node;
    n00b_symbol_t   *dst_symbol;
    n00b_list_t     *deps; // all symbols influencing the def
} n00b_cfg_flow_info_t;

typedef struct {
    n00b_cfg_node_t *last_def;
    n00b_cfg_node_t *last_use;
} n00b_cfg_status_t;

struct n00b_cfg_node_t {
    n00b_tree_node_t *reference_location;
    n00b_cfg_node_t  *parent;
    n00b_dict_t      *starting_liveness_info;
    n00b_list_t      *starting_sometimes;
    n00b_dict_t      *liveness_info;
    n00b_list_t      *sometimes_live;

    union {
        n00b_cfg_block_enter_info_t block_entrance;
        n00b_cfg_block_exit_info_t  block_exit;
        n00b_cfg_branch_info_t      branches;
        n00b_cfg_flow_info_t        flow; // Used for any def / use activity.
        n00b_cfg_jump_info_t        jump;
    } contents;

    n00b_cfg_node_type kind;
    unsigned int      use_without_def : 1;
    unsigned int      reached         : 1;
};
