#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define n00b_internal_heap n00b_default_heap

static pthread_once_t  debugging_init        = PTHREAD_ONCE_INIT;
static bool            skip_debugging        = false;
static bool            allow_unknown_topics  = true;
static bool            debug_autosubscribe   = true;
static n00b_string_t  *n00b_debug_namespace  = NULL;
static n00b_stream_t  *n00b_debug_proxy      = NULL;
static n00b_stream_t  *n00b_debug_connection = NULL;
static n00b_stream_t  *n00b_stderr_cache     = NULL;
static n00b_dict_t    *n00b_debug_topics     = NULL;
static n00b_string_t  *n00b_debug_prefix     = NULL;
static bool            local_only            = false;
static bool            show_time             = true;
static bool            show_date             = true;
static bool            hex_for_values        = true;
static bool            unquote_strings       = true;
static const char     *type_format           = "«blue»«i»(«#»«/i»)";
static const char     *topic_format          = ":[|green|][|#|]";
static const char     *dt_format             = "@[|#|]";
static const char     *payload_sep           = ": ";
static _Atomic int64_t outstanding_dmsgs     = 0;

static n00b_string_t *
n00b_format_metadata(n00b_message_t *msg)
{
    n00b_string_t *output   = n00b_debug_prefix;
    n00b_string_t *type_str = NULL;
    n00b_string_t *time_str = NULL;
    n00b_string_t *topic_str;
    bool           is_obj        = false;
    bool           is_interior   = false;
    bool           pass_by_value = false;
    void          *value         = msg->payload;

    if (n00b_in_any_heap(value)) {
        n00b_basic_memory_info(value, &is_obj, &is_interior);
    }
    else {
        pass_by_value = true;
    }

    if (type_format) {
        if (pass_by_value) {
            type_str = n00b_cformat(type_format, n00b_cstring("value"));
        }
        else {
            if (is_interior) {
                type_str = n00b_cformat(type_format, n00b_cstring("interior"));
            }
            else {
                if (is_obj) {
                    n00b_type_t *t = n00b_get_my_type(value);
                    if (!unquote_strings || !n00b_type_is_string(t)) {
                        type_str = n00b_cformat(type_format, t);
                    }
                }
                else {
                    type_str = n00b_cformat(type_format, n00b_cstring("raw"));
                }
            }
        }
    }

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
        if (show_time) {
            time = n00b_current_time();
        }
    }

    if (time) {
        time_str = n00b_cformat(dt_format, time);
    }

    topic_str = n00b_cformat(topic_format, msg->topic);

    if (type_str) {
        output = n00b_string_concat(output, type_str);
    }
    if (time_str) {
        output = n00b_string_concat(output, time_str);
    }

    output = n00b_string_concat(output, topic_str);
    return n00b_string_concat(output, n00b_cstring(payload_sep));
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
    n00b_string_t *output    = n00b_format_metadata(msg);
    n00b_string_t *formatted = NULL;

    if (!n00b_in_any_heap(msg->payload)) {
        if (hex_for_values) {
            formatted = n00b_cformat("«#:x»", msg->payload);
        }
        else {
            formatted = n00b_cformat("«#»", msg->payload);
        }
    }
    else {
        if (unquote_strings) {
            formatted = n00b_cformat("«#»", msg->payload);
        }
        else {
            formatted = n00b_cformat("«#:l»", msg->payload);
        }
    }

    // If there's no nl in the thing we're outputting, or if the NL is already
    // at the front, don't add one to the front. Otherwise, do.
    if (n00b_string_find(formatted, n00b_cached_newline()) > 0) {
        formatted = n00b_string_concat(n00b_cached_newline(), formatted);
    }
    if (!n00b_string_ends_with(formatted, n00b_cached_newline())) {
        formatted = n00b_string_concat(formatted, n00b_cached_newline());
    }

    output = n00b_string_concat(output, formatted);

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

    n00b_io_set_repr(n00b_debug_connection, n00b_cstring("[debug cb]"));

    if (conn) {
        fprintf(stderr, "Debug server closed -- debuging to stderr.\n");
    }
    else {
        n00b_string_t *s = n00b_cformat("«em2»Debugging to stderr.\n");
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
    n00b_string_t *addr     = n00b_get_env(n00b_cstring("N00B_DEBUG_ADDRESS"));
    n00b_string_t *str_port = n00b_get_env(n00b_cstring("N00B_DEBUG_PORT"));
    uint16_t       int_port = N00B_DEFAULT_DEBUG_PORT;

    if (!addr) {
        addr = n00b_cstring("0.0.0.0");
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

    n00b_string_t *s = NULL;

    if (!n00b_debug_connection) {
        handle_debug_socket_close(NULL, NULL, NULL);
    }
    else {
        n00b_add_marshaling(n00b_debug_connection);
        n00b_ignore_unseen_errors(n00b_debug_connection);
        n00b_stream_t *cb = n00b_callback_open(handle_debug_socket_close,
                                               n00b_debug_connection);
        n00b_io_set_repr(cb, n00b_cstring("[debug close cb]"));
        n00b_io_subscribe_to_close(n00b_debug_connection, cb);
        n00b_io_subscribe_to_writes(n00b_debug_proxy,
                                    n00b_debug_connection,
                                    NULL);
        s = n00b_cformat(
            "«em2»Connected to debug logging server at «#».«/»\n",
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

    n00b_io_set_repr(n00b_debug_proxy, n00b_cstring("[debug proxy]"));
}

static void
n00b_launch_debugging(void)
{
    n00b_push_heap(n00b_internal_heap);
    n00b_gc_register_root(&n00b_debug_namespace, 1);
    n00b_gc_register_root(&n00b_debug_proxy, 1);
    n00b_gc_register_root(&n00b_debug_connection, 1);
    n00b_gc_register_root(&n00b_stderr_cache, 1);
    n00b_gc_register_root(&n00b_debug_topics, 1);
    n00b_gc_register_root(&n00b_debug_prefix, 1);
    n00b_debug_namespace = n00b_cstring("debug");
    n00b_debug_prefix    = n00b_cformat("«blue»debug");
    n00b_debug_topics    = n00b_dict(n00b_type_string(), n00b_type_ref());
    n00b_debug_proxy     = n00b_new_subscription_proxy();
    n00b_establish_debug_fd();

    n00b_io_set_repr(n00b_debug_proxy, n00b_cstring("[debug proxy]"));
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

    n00b_string_t *tname = NULL;
    n00b_stream_t *topic;

    if (!n00b_in_any_heap(topic_info)) {
        tname = n00b_cstring(topic_info);
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
    printf("sup\n");
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

    if (n00b_in_any_heap(v)) {
        n00b_alloc_hdr *h = n00b_find_allocation_record(v);
        h->alloc_file     = n00b_backtrace_utf8()->data;
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
    if (!n00b_in_any_heap(obj)) {
        n00b_debug(tname, n00b_cformat("«#:o»}", (uint64_t)obj));
    }
    else {
        n00b_buf_t *b = n00b_automarshal(obj);
        n00b_debug(tname,
                   n00b_string_concat(n00b_cached_newline(),
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
        n00b_topic_subscribe(n00b_get_topic(n00b_cstring(topic),
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
