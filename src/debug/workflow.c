// Debug workflow.

#include "n00b.h"

static n00b_dict_t      *topic_workflows   = NULL;
static n00b_dict_t      *default_workflow  = NULL;
static n00b_debug_fmt_fn default_formatter = NULL;
pthread_once_t           workflow_setup    = PTHREAD_ONCE_INIT;

static void
init_db_workflow(void)
{
    n00b_gc_register_root(&topic_workflows, 1);
    n00b_gc_register_root(&default_workflow, 1);
    topic_workflows   = n00b_dict(n00b_type_string(), n00b_type_ref());
    default_workflow  = n00b_drule_server(NULL);
    // First add the false branch (index 0) for when the server fails.
    n00b_drule_t *cur = n00b_drule_terminal(default_workflow);
    // Add the true branch, ending if we log to server successfully.
    // This is important, as if there's no branch, we assume that
    // we always want to do both actions.
    n00b_drule_end(default_workflow);
    n00b_drule_end(cur);
}

static inline void
check_db_workflow_inited(void)
{
    pthread_once(&workflow_setup, init_db_workflow);
}

static inline n00b_db_rule_t *
dbr_node(n00b_db_rule_kind kind)
{
    n00b_db_rule_t *result = n00b_gc_alloc_mapped(n00b_db_rule_t,
                                                  N00B_GC_SCAN_ALL);
    result->kind           = kind;
    result->next           = n00b_list(n00b_type_ref());

    return result;
}

static inline n00b_db_rule_t *
dbr_attach(n00b_db_rule_t *cur, n00b_db_rule_t *prev)
{
    if (prev) {
        if (prev->kind == N00B_DB_END) {
            N00B_CRAISE("Can't add to an 'end' node");
        }

        n00b_list_append(prev->next, cur);
    }

    return cur;
}

n00b_db_rule_t *
n00b_drule_terminal(n00b_db_rule_t *prev)
{
    return dbr_attach(dbr_node(N00B_DBR_STDERR), prev);
}

n00b_db_rule_t *
n00b_drule_server(n00b_db_rule_t *prev)
{
    return dbr_attach(dbr_node(N00B_DBR_SERVER), prev);
}

n00b_db_rule_t *
n00b_drule_logfile(n00b_db_rule_t *prev)
{
    return dbr_attach(dbr_node(N00B_DBR_LOGFILE), prev);
}

n00b_db_rule_t *
n00b_drule_test(n00b_db_rule_t *prev, n00b_debug_topic_test cb, void *params)
{
    n00b_db_rule_t *result = dbr_node(N00B_DBR_TEST);
    result->cb.test        = cb;
    result->params         = params;

    return db_attach(result, prev);
}

n00b_db_rule_t *
n00b_drule_set_formatter(n00b_db_rule_t *prev, n00b_debug_fmt_fn cb)
{
    n00b_db_rule_t *result = dbr_node(N00B_DBR_SET_FORMATTER);
    result->cb.formatter   = cb;

    return db_pattach(result, prev);
}

n00b_db_rule_t *
n00b_drule_end(n00b_db_rule_t *prev)
{
    return dbr_attach(dbr_node(N00B_DBR_END), prev);
}

static inline int
handle_stderr(n00b_db_rule_t *cur)
{
}

static inline int
handle_server(n00b_db_rule_t *cur)
{
}

static inline int
handle_logfile(n00b_db_rule_t *cur)
{
}

static inline int
handle_test(n00b_db_rule_t *cur)
{
    // If this is out of bounds, will give us a NULL, which results in
    // ending the workflow.
    cur = n00b_list_get(cur->next, res, NULL);
}

static inline int
handle_formatter(n00b_db_rule_t *cur)
{
}

void
n00b_apply_debug_workflow(n00b_debug_msg_t *msg)
{
    check_db_workflow_inited();
    n00b_db_rule_t *workflow = hatrack_dict_get(topic_workflows,
                                                msg->topic,
                                                NULL);

    if (!workflow) {
        workflow = default_workflow;
    }

    n00b_db_rule_t *cur = workflow;

    while (cur && cur->kind != N00B_DBR_END) {
        switch (cur->kind) {
        case N00B_DBR_STDERR:
            cur = handle_stderr(cur, msg);
            continue;
        case N00B_DBR_SERVER:
            cur = handle_server(cur, msg);
            continue;
        case N00B_DBR_LOGFILE:
            cur = handle_logfile(cur, msg);
            continue;
        case N00B_DBR_TEST:
            cur = handle_test(cur, msg);
            continue;
        case N00B_DBR_SET_FORMATTER:
            cur = handle_formater(cur, msg);
            continue;
        default:
            n00b_unreachable();
        }
    }
}
