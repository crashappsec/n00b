// Debug workflow.

#include "n00b.h"
#define SERVER_HOST_ENV     "N00B_DEBUG_HOST"
#define SERVER_PORT_ENV     "N00B_DEBUG_PORT"
#define DEFAULT_SERVER_HOST "127.0.0.1"
#define SERVER_LOGFILE_ENV  "N00B_DEBUG_LOG"

#define DEFAULT_SERVER_PORT 7877
#define RETRY_STARTING_MS   500

static void           attempt_connection(void);
static void           attempt_log_open(void);
static n00b_string_t *simple_fmt(n00b_debug_msg_t *);

static n00b_dict_t      *topic_workflows   = NULL;
static n00b_db_rule_t   *default_workflow  = NULL;
static n00b_channel_t   *server_connection = NULL;
static n00b_channel_t   *debug_logfile     = NULL;
static n00b_debug_fmt_fn default_formatter = simple_fmt;
static pthread_once_t    server_attempted  = PTHREAD_ONCE_INIT;
static pthread_once_t    logfile_attempted = PTHREAD_ONCE_INIT;

pthread_once_t workflow_setup = PTHREAD_ONCE_INIT;

void
n00b_set_default_debug_flow(n00b_db_rule_t *rule)
{
    default_workflow = rule;
}

void
n00b_set_debug_topic_flow(n00b_string_t *topic, n00b_db_rule_t *rule)
{
    hatrack_dict_put(topic_workflows, topic, rule);
}

static void
init_db_workflow(void)
{
    n00b_gc_register_root(&topic_workflows, 1);
    n00b_gc_register_root(&default_workflow, 1);
    n00b_gc_register_root(&server_connection, 1);
    n00b_gc_register_root(&debug_logfile, 1);

    topic_workflows     = n00b_dict(n00b_type_string(), n00b_type_ref());
    default_workflow    = n00b_drule_server(NULL);
    // First add the false branch (index 0) for when the server fails.
    // Try a logfile next (if the env var is set).
    n00b_db_rule_t *log = n00b_drule_logfile(default_workflow);
    // Add the true branch, ending if we log to server successfully.
    // If we don't, and only leave one branch, the system assumes you
    // don't care about the answer and always want to evaluate the
    // next rule.
    n00b_drule_end(default_workflow);
    // If the log file env var isn't there, its false branch should
    // log to the terminal.
    n00b_drule_terminal(log);
    // Again, add a true branch to the log file option. No need to do it
    // for the terminal logger since it doesn't have a successor.
    n00b_drule_end(log);
}

n00b_db_rule_t *
n00b_get_topic_workflow(n00b_string_t *topic)
{
    pthread_once(&workflow_setup, init_db_workflow);
    n00b_db_rule_t *result = hatrack_dict_get(topic_workflows, topic, NULL);

    if (result) {
        return result;
    }

    return default_workflow;
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
        if (prev->kind == N00B_DBR_END) {
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

    return dbr_attach(result, prev);
}

n00b_db_rule_t *
n00b_drule_set_formatter(n00b_db_rule_t *prev, n00b_debug_fmt_fn cb)
{
    n00b_db_rule_t *result = dbr_node(N00B_DBR_SET_FORMATTER);
    result->cb.formatter   = cb;

    return dbr_attach(result, prev);
}

n00b_db_rule_t *
n00b_drule_end(n00b_db_rule_t *prev)
{
    return dbr_attach(dbr_node(N00B_DBR_END), prev);
}

static inline n00b_buf_t *
format_message(n00b_debug_msg_t *msg)
{
    n00b_string_t *s = (*msg->formatter)(msg);

    if (s->data[s->u8_bytes - 1] != '\n') {
        s = n00b_string_concat(s, n00b_cached_newline());
    }

    s = n00b_cstring(n00b_rich_to_ansi(s, NULL));
    return n00b_string_to_buffer(s);
}

static inline int
handle_stderr(n00b_debug_msg_t *msg)
{
    n00b_buf_t *to_write = format_message(msg);

    n00b_channel_write(n00b_chan_stderr(), to_write);

    return 1;
}

static inline n00b_net_addr_t *
get_debug_server_addr(void)
{
    n00b_string_t *hoststr = n00b_get_env(n00b_cstring(SERVER_HOST_ENV));
    n00b_string_t *portstr = n00b_get_env(n00b_cstring(SERVER_PORT_ENV));
    int64_t        port    = DEFAULT_SERVER_PORT;

    if (!hoststr) {
        hoststr = n00b_cstring(DEFAULT_SERVER_HOST);
    }

    if (portstr) {
        int64_t parsed;
        if (n00b_parse_int64(portstr, &parsed)
            && parsed > 0 && parsed < 1 << 17) {
            port = parsed;
        }
    }

    return n00b_new(n00b_type_net_addr(),
                    n00b_kw("address", hoststr, "port", port));
}

static void
on_server_close(n00b_channel_t *c, void *ignored)
{
    // Start w/ half a second of sleep. Double each time, up to 8
    // total attempts

    int sleep_ms = RETRY_STARTING_MS;

    for (int i = 0; i < 8; i++) {
        n00b_sleep_ms(sleep_ms);
        attempt_connection();
        if (server_connection) {
            return;
        }
        sleep_ms <<= 1;
    }
}

static void
on_log_close(n00b_channel_t *c, void *ignored)
{
    // Maybe log to stdout too, but no need for exponential back-off,
    // or waiting at all.
    attempt_log_open();
}

static void
attempt_connection(void)
{
    server_connection = n00b_channel_connect(get_debug_server_addr());

    if (server_connection) {
        n00b_channel_subscribe_close(
            server_connection,
            n00b_new_callback_channel(on_server_close, NULL));
    }
}

static void
attempt_log_open(void)
{
    n00b_string_t *fname = n00b_get_env(n00b_cstring(SERVER_LOGFILE_ENV));

    debug_logfile = n00b_channel_open_file(fname,
                                           "write_only",
                                           n00b_ka(true),
                                           "allow_file_creation",
                                           n00b_ka(true),
                                           "writes_always_append",
                                           n00b_ka(true));

    if (debug_logfile) {
        n00b_channel_subscribe_close(
            server_connection,
            n00b_new_callback_channel(on_log_close, NULL));
    }
}

static inline int
handle_server(n00b_debug_msg_t *msg)
{
    if (!server_connection) {
        attempt_connection();
        pthread_once(&server_attempted, attempt_connection);
        if (!server_connection) {
            return 0;
        }
    }

    n00b_buf_t *to_write = format_message(msg);
    n00b_channel_write(server_connection, to_write);
    return 1;
}

static inline int
handle_logfile(n00b_debug_msg_t *msg)
{
    if (!debug_logfile && n00b_get_env(n00b_cstring(SERVER_LOGFILE_ENV))) {
        pthread_once(&logfile_attempted, attempt_log_open);
    }

    if (!debug_logfile) {
        return 0;
    }

    n00b_buf_t *to_write = format_message(msg);
    n00b_channel_write(debug_logfile, to_write);
    return 1;
}

static inline int
handle_test(n00b_db_rule_t *cur, n00b_debug_msg_t *msg)
{
    return (*cur->cb.test)(msg, msg->last_output, cur->params);
}

static inline int
handle_formatter(n00b_db_rule_t *cur, n00b_debug_msg_t *msg)
{
    msg->formatter = cur->cb.formatter;
    return 0;
}

static n00b_string_t *
simple_fmt(n00b_debug_msg_t *msg)
{
    return n00b_cformat(
        "[|em5|][[|#|]][|/|] @"
        "[|b|][|#|]:[|/|]\n"
        "[|#|]\n[|em5|][/[|#|]]",
        msg->topic,
        msg->timestamp,
        msg->payload,
        msg->topic);
}

void
n00b_apply_debug_workflow(n00b_debug_msg_t *msg)
{
    n00b_db_rule_t *workflow = hatrack_dict_get(topic_workflows,
                                                msg->topic,
                                                NULL);

    if (!workflow) {
        workflow = default_workflow;
    }

    // Install the default formatter unless a workflow object changes it.
    if (!msg->formatter) {
        msg->formatter = default_formatter;
    }

    n00b_db_rule_t *cur = workflow;
    int             ix;

    while (cur && cur->kind != N00B_DBR_END) {
        switch (cur->kind) {
        case N00B_DBR_STDERR:
            ix = handle_stderr(msg);
            break;
        case N00B_DBR_SERVER:
            ix = handle_server(msg);
            break;
        case N00B_DBR_LOGFILE:
            ix = handle_logfile(msg);
            break;
        case N00B_DBR_TEST:
            ix = handle_test(cur, msg);
            break;
        case N00B_DBR_SET_FORMATTER:
            ix = handle_formatter(cur, msg);
            break;
        default:
            n00b_unreachable();
        }

        if (n00b_list_len(cur->next) == 1) {
            cur = n00b_list_get(cur->next, 0, NULL);
        }
        else {
            // If the index is out of bounds, it is the same as
            // hitting a DBR_END node.
            cur = n00b_list_get(cur->next, ix, NULL);
        }
    }
}
