#define N00B_USE_INTERNAL_API
#include "n00b.h"

static pthread_once_t  debugging_init        = PTHREAD_ONCE_INIT;
static bool            skip_debugging        = false;
static bool            allow_unknown_topics  = true;
static bool            debug_autosubscribe   = true;
static n00b_utf8_t    *n00b_debug_namespace  = NULL;
static n00b_stream_t  *n00b_debug_proxy      = NULL;
static n00b_stream_t  *n00b_debug_connection = NULL;
static n00b_stream_t  *n00b_stderr_cache     = NULL;
static n00b_dict_t    *n00b_debug_topics     = NULL;
static n00b_utf8_t    *n00b_debug_prefix     = NULL;
static bool            local_only            = false;
static bool            show_time             = false;
static bool            show_date             = false;
static bool            raw_address           = false;
static bool            hex_for_values        = true;
static bool            unquote_strings       = true;
static char           *type_format           = "([i]{}[/])";
static char           *topic_format          = ":[atomic lime]{}";
static char           *dt_format             = "@{}[/]";
static char           *payload_sep           = ": ";
static _Atomic int64_t outstanding_dmsgs     = 0;

static n00b_utf8_t *
n00b_format_metadata(n00b_message_t *msg)
{
    n00b_utf8_t *output   = n00b_debug_prefix;
    n00b_utf8_t *type_str = NULL;
    n00b_utf8_t *time_str = NULL;
    n00b_utf8_t *topic_str;
    bool         is_obj        = false;
    bool         is_interior   = false;
    bool         pass_by_value = false;
    void        *value         = msg->payload;

    if (n00b_in_heap(value)) {
        n00b_basic_memory_info(value, &is_obj, &is_interior);
    }
    else {
        pass_by_value = true;
    }

    if (type_format) {
        if (pass_by_value) {
            type_str = n00b_cstr_format(type_format,
                                        n00b_new_utf8("value"));
        }
        else {
            if (is_interior) {
                type_str = n00b_cstr_format(type_format,
                                            n00b_new_utf8("interior"));
            }
            else {
                if (is_obj) {
                    n00b_type_t *t = n00b_get_my_type(value);
                    if (!unquote_strings || !n00b_type_is_string(t)) {
                        type_str = n00b_cstr_format(type_format, t);
                    }
                }
                else {
                    type_str = n00b_cstr_format(type_format,
                                                n00b_new_utf8("raw"));
                }
            }
        }
    }

    if (dt_format) {
        n00b_date_time_t *time = NULL;

        if (show_date) {
            if (show_time) {
                time = n00b_current_date_time();
            }
            else {
                time = n00b_current_date();
            }
        }
        else {
            time = n00b_current_time();
        }

        if (time) {
            time_str = n00b_cstr_format(dt_format, time);
        }
    }

    if (topic_format) {
        topic_str = n00b_cstr_format(topic_format, msg->topic);
    }
    else {
        topic_str = msg->topic;
    }

    if (type_str) {
        output = n00b_str_concat(output, type_str);
    }
    if (time_str) {
        output = n00b_str_concat(output, time_str);
    }

    output = n00b_str_concat(output, topic_str);
    output = n00b_str_concat(output, n00b_new_utf8(payload_sep));

    return n00b_to_utf8(output);
}

void
n00b_allow_unknown_debug_topics(void)
{
    allow_unknown_topics = true;
}

void
n00b_disallow_unknown_debug_topics(void)
{
    allow_unknown_topics = false;
}

void
n00b_disable_debugging(void)
{
    skip_debugging = true;
}

void
n00b_disable_debug_server(void)
{
    local_only = true;
}

static void
n00b_debug_output_cb(void *ignore, n00b_message_t *msg, void *thunk)
{
    n00b_push_heap(n00b_internal_heap);
    n00b_utf8_t *output    = n00b_format_metadata(msg);
    n00b_utf8_t *formatted = NULL;

    if (!n00b_in_heap(msg->payload)) {
        if (hex_for_values) {
            formatted = n00b_cstr_format("{:x}", msg->payload);
        }
        else {
            formatted = n00b_cstr_format("{}", msg->payload);
        }
    }
    else {
        if (n00b_has_repr(msg->payload)) {
            if (unquote_strings) {
                n00b_type_t *t = n00b_get_my_type(msg->payload);
                if (n00b_type_is_string(t)) {
                    formatted = msg->payload;
                }
                else {
                    formatted = n00b_repr(msg->payload);
                }
            }
            else {
                formatted = n00b_repr(msg->payload);
            }
        }
        else {
            cprintf("WTF: %s\n", n00b_get_my_type(msg->payload));
            if (raw_address) {
                formatted = n00b_cstr_format("->@{:x}", msg->payload);
            }
            else {
                formatted = n00b_hex_debug_repr(msg->payload);
                formatted = n00b_str_concat(n00b_get_newline_const(),
                                            formatted);
            }
        }
    }

    if (!formatted) {
        formatted = n00b_hex_debug_repr(msg->payload);
        formatted = n00b_str_concat(n00b_get_newline_const(), formatted);
    }

    // If there's no nl in the thing we're outputting, or if the NL is already
    // at the front, don't add one to the front. Otherwise, do.
    if (n00b_str_find(formatted, n00b_get_newline_const()) > 0) {
        formatted = n00b_str_concat(n00b_get_newline_const(), formatted);
    }
    if (!n00b_str_ends_with(formatted, n00b_get_newline_const())) {
        formatted = n00b_str_concat(formatted, n00b_get_newline_const());
    }

    output = n00b_str_concat(output, formatted);

    n00b_write(n00b_stderr(), output);

    atomic_fetch_add(&outstanding_dmsgs, -1);
    n00b_pop_heap();
}

static void
handle_debug_socket_close(n00b_stream_t *i1, void *conn, void *i2)
{
    n00b_debug_connection = n00b_callback_open(
        (n00b_io_callback_fn)n00b_debug_output_cb,
        NULL);
    n00b_stderr_cache = n00b_fd_open(fileno(stderr));

    n00b_io_set_repr(n00b_debug_connection, n00b_new_utf8("[debug cb]"));

    if (conn) {
        fprintf(stderr, "Debug server closed -- debuging to stderr.\n");
    }
    else {
        n00b_utf8_t *s = n00b_cstr_format("[em]Debugging to stderr.\n");
        n00b_write(n00b_stderr_cache, s);
    }
    n00b_io_subscribe_to_writes(n00b_debug_proxy, n00b_debug_connection, NULL);
}

static inline void
n00b_establish_debug_fd(void)
{
    if (local_only) {
        handle_debug_socket_close(NULL, NULL, NULL);
        return;
    }
    // TODO -- socket option.
    n00b_utf8_t *addr     = n00b_get_env(n00b_new_utf8("N00B_DEBUG_ADDRESS"));
    n00b_utf8_t *str_port = n00b_get_env(n00b_new_utf8("N00B_DEBUG_PORT"));
    uint16_t     int_port = N00B_DEFAULT_DEBUG_PORT;

    if (!addr) {
        addr = n00b_new_utf8("0.0.0.0");
    }

    if (str_port) {
        int64_t parsed_port;
        if (n00b_parse_int64(str_port, &parsed_port)) {
            if (parsed_port > 0 && parsed_port < 0x10000) {
                int_port = (uint16_t)parsed_port;
            }
        }
    }

    n00b_net_addr_t *ip = n00b_new(n00b_type_ip(),
                                   n00b_kw("address",
                                           addr,
                                           "port",
                                           n00b_ka(int_port)));

    n00b_debug_connection = n00b_connect(ip);

    n00b_utf8_t *s = NULL;

    if (!n00b_debug_connection) {
        handle_debug_socket_close(NULL, NULL, NULL);
    }
    else {
        n00b_add_marshaling(n00b_debug_connection);
        n00b_ignore_unseen_errors(n00b_debug_connection);
        n00b_stream_t *cb = n00b_callback_open(handle_debug_socket_close,
                                               n00b_debug_connection);
        n00b_io_set_repr(cb, n00b_new_utf8("[debug close cb]"));
        n00b_io_subscribe_to_close(n00b_debug_connection, cb);
        n00b_io_subscribe_to_writes(n00b_debug_proxy,
                                    n00b_debug_connection,
                                    NULL);
        s = n00b_cstr_format(
            "[h2]Connected to debug logging server at {}.[/]\n",
            ip);
    }
    if (s) {
        n00b_write(n00b_stderr(), s);
    }
}

void
n00b_restart_debugging(void)
{
    n00b_debug_proxy      = n00b_new_subscription_proxy();
    n00b_debug_connection = NULL;
    n00b_establish_debug_fd();

    n00b_io_set_repr(n00b_debug_proxy, n00b_new_utf8("[debug proxy]"));
}

void static n00b_launch_debugging(void)
{
    n00b_push_heap(n00b_internal_heap);
    n00b_gc_register_root(&n00b_debug_namespace, 1);
    n00b_gc_register_root(&n00b_debug_proxy, 1);
    n00b_gc_register_root(&n00b_debug_connection, 1);
    n00b_gc_register_root(&n00b_stderr_cache, 1);
    n00b_gc_register_root(&n00b_debug_topics, 1);
    n00b_gc_register_root(&n00b_debug_prefix, 1);
    n00b_debug_namespace = n00b_new_utf8("debug");
    n00b_debug_prefix    = n00b_cstr_format("[cyan]debug[/]");
    n00b_debug_topics    = n00b_dict(n00b_type_utf8(), n00b_type_ref());
    n00b_debug_proxy     = n00b_new_subscription_proxy();
    n00b_establish_debug_fd();

    n00b_io_set_repr(n00b_debug_proxy, n00b_new_utf8("[debug proxy]"));
    n00b_pop_heap();
}

void
n00b_enable_debugging(void)
{
    skip_debugging = false;
    pthread_once(&debugging_init, n00b_launch_debugging);
}

static n00b_stream_t *
n00b_get_debug_topic(void *topic_info, bool create)
{
    pthread_once(&debugging_init, n00b_launch_debugging);

    n00b_utf8_t   *tname = NULL;
    n00b_stream_t *topic;

    if (!n00b_in_heap(topic_info)) {
        tname = n00b_new_utf8(topic_info);
    }
    else {
        n00b_type_t *t = n00b_get_my_type(topic_info);

        if (n00b_type_is_string(t)) {
            tname = topic_info;
        }
        else {
            N00B_CRAISE("First parameter to n00b_debug must be a topic name.");
        }
    }

    n00b_object_sanity_check(tname);

    topic = hatrack_dict_get(n00b_debug_topics, tname, NULL);

    if (!topic) {
        if (!create && !allow_unknown_topics) {
            N00B_CRAISE("Cannot publish debug message to unregistered topic.");
        }

        topic = n00b_get_topic(tname, n00b_debug_namespace);

        if (debug_autosubscribe) {
            n00b_topic_subscribe(topic, n00b_debug_proxy);
        }

        if (!hatrack_dict_add(n00b_debug_topics, tname, topic)) {
            topic = hatrack_dict_get(n00b_debug_topics, tname, NULL);
        }
    }

    return topic;
}

n00b_stream_t *
n00b_add_debug_topic(void *topic_info)
{
    return n00b_get_debug_topic(topic_info, true);
}

// Always have at least one value. Note that if anything other than
// the first value is null, that value (and subsequent values) will
// not print.
void
_n00b_debug(void *tname, void *v, ...)
{
    if (skip_debugging) {
        return;
    }

    pthread_once(&debugging_init, n00b_launch_debugging);

    n00b_push_heap(n00b_internal_heap);
    n00b_stream_t *topic = n00b_get_debug_topic(tname, false);

    va_list values;
    va_start(values, v);
    if (!v) {
        v = n00b_box_u64(0ull);
    }

    do {
        n00b_topic_post(topic, v);
        atomic_fetch_add(&outstanding_dmsgs, 1);
        v = va_arg(values, void *);
    } while (v);
    n00b_pop_heap();
}

void
n00b_debug_object(void *tname, void *obj)
{
    if (!n00b_in_heap(obj)) {
        n00b_debug(tname, n00b_cstr_format("{:x}", (uint64_t)obj));
    }
    else {
        n00b_buf_t *b = n00b_automarshal(obj);
        n00b_debug(tname,
                   n00b_str_concat(n00b_get_newline_const(),
                                   n00b_hex_debug_repr(b->data)));
    }
}

void
_n00b_debug_internal_subscribe(char *topic, ...)
{
    va_list args;
    va_start(args, topic);

    pthread_once(&debugging_init, n00b_launch_debugging);

    while (topic) {
        n00b_topic_subscribe(n00b_get_topic(n00b_new_utf8(topic),
                                            n00b_debug_namespace),
                             n00b_debug_proxy);
        topic = va_arg(args, char *);
    }
    va_end(args);
}

void
n00b_debug_start_shutdown(void)
{
    skip_debugging = true;
}

bool
n00b_is_debug_shutdown_queue_processed(void)
{
    return atomic_read(&outstanding_dmsgs) == 0;
}
