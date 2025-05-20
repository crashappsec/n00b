#pragma once
#include "n00b.h"

// Action:
// 1) Test
// 2) GROUP
// 2) Server
// 3) Log
// 4) End
//
// Actions have values that are inputs to the next step.
//

typedef struct n00b_debug_msg_t n00b_debug_msg_t;

typedef int (*n00b_debug_topic_test)(n00b_debug_msg_t *, int, void *);
typedef void *(*n00b_debug_fmt_fn)(n00b_debug_msg_t *);

typedef enum {
    N00B_DBR_STDERR,
    N00B_DBR_SERVER,
    N00B_DBR_LOGFILE,
    N00B_DBR_TEST,
    N00B_DBR_SET_FORMATTER,
    N00B_DBR_END,
} n00b_db_rule_kind;

typedef struct {
    n00b_db_rule_kind kind;
    union {
        n00b_debug_topic_test test;
        n00b_debug_fmt_fn     formatter;
    } cb;
    void        *params;
    n00b_list_t *next;
} n00b_db_rule_t;

struct n00b_debug_msg_t {
    n00b_string_t    *topic;
    n00b_date_time_t *timestamp;
    n00b_db_rule_t   *workflow;
    void             *remote_address;
    void             *payload;
    n00b_debug_fmt_fn formatter;
    int               last_output;
    pid_t             pid;
};

typedef struct n00b_wire_dmsg_t {
    n00b_string_t    *topic;
    n00b_date_time_t *timestamp;
    void             *payload;
    pid_t             pid;
} n00b_wire_dmsg_t;

extern void n00b_set_debug_topic_flow(n00b_string_t *,
                                      n00b_db_rule_t *);
extern void n00b_set_default_debug_flow(n00b_db_rule_t *);

// All of these start with the previous node to link to, and return
// the new node.
extern n00b_db_rule_t *n00b_drule_terminal(n00b_db_rule_t *);
extern n00b_db_rule_t *n00b_drule_server(n00b_db_rule_t *);
extern n00b_db_rule_t *n00b_drule_logfile(n00b_db_rule_t *);
extern n00b_db_rule_t *n00b_drule_test(n00b_db_rule_t *,
                                       n00b_debug_topic_test,
                                       void *);
extern n00b_db_rule_t *n00b_drule_set_formatter(n00b_db_rule_t *,
                                                n00b_debug_fmt_fn);
extern n00b_db_rule_t *n00b_drule_end(n00b_db_rule_t *);

#ifdef N00B_USE_INTERNAL_API
extern n00b_db_rule_t *n00b_get_topic_workflow(n00b_string_t *);
extern void            n00b_apply_debug_workflow(n00b_debug_msg_t *);
bool                   n00b_local_process_is_server;
#endif
