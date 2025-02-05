// The debug logging server. Currently simple; it's a TODO to
// integrate n00b to allow for filtering.

#include "n00b.h"

static n00b_stream_t *n00b_debug_log_file = NULL;
static n00b_stream_t *n00b_debug_cb       = NULL;
static n00b_stream_t *n00b_debug_output   = NULL;
static n00b_stream_t *n00b_debug_listener = NULL;
static n00b_stream_t *n00b_debug_stdin    = NULL;

static inline void
setup_log_file(n00b_string_t *fname)
{
    if (!fname) {
        return;
    }
    int fd = open(fname->data,
                  O_CREAT | O_CLOEXEC | O_RDWR | O_TRUNC,
                  S_IRWXU | S_IROTH | S_IRGRP);

    if (fd < 0) {
        fd = open(fname->data, O_CLOEXEC | O_RDWR | O_TRUNC);
    }

    if (fd < 0) {
        n00b_printf("«red»WARNING:«/» Could not open debug file «em»«#»«/».",
                    fname);
        return;
    }

    n00b_debug_log_file = n00b_fd_open(fd);
}

static void
process_debug_msg(n00b_stream_t *conn, void *raw, void *aux)
{
    if (!n00b_type_is_message(n00b_get_my_type(raw))) {
        n00b_stream_t *t = n00b_add_debug_topic("__bad_debug_message__");
        raw              = n00b_new(n00b_type_message(), t, raw);
    }

    n00b_message_t *msg = raw;

    n00b_debug(msg->topic, msg->payload);

    if (n00b_debug_log_file) {
        n00b_write(n00b_debug_log_file, msg->payload);
    }
}

static n00b_string_t *ctrl_c = NULL;
static n00b_string_t *q_key  = NULL;

static void
logger_handle_stdin(n00b_stream_t *p1, void *m, void *aux)
{
    n00b_buf_t    *b = m;
    n00b_string_t *s = n00b_utf8(b->data, b->byte_len);

    n00b_printf("«em2» On stdin: «#»", s);

    if (!ctrl_c) {
        n00b_gc_register_root(&ctrl_c, 1);
        ctrl_c = n00b_cstring("\x03");
        q_key  = n00b_cstring("q");
    }

    if (n00b_string_starts_with(s, ctrl_c)) {
        n00b_printf("«em»Shutting down«/» due to «em»Ctrl-C«/».");
        n00b_exit(0);
    }

    if (n00b_string_starts_with(s, q_key)) {
        n00b_printf("«em»Shutting down«/» («em»Q«/» pressed).");
        n00b_exit(0);
    }
}

static void
connection_closer(n00b_stream_t *ignore, void *conn, void *unused)
{
    n00b_stream_t     *c      = conn;
    n00b_ev2_cookie_t *cookie = c->cookie;
    n00b_string_t     *cmsg   = n00b_cformat(
        "«green»Connection from «#» «em2»CLOSED.«/»\n",
        cookie->aux);

    n00b_write(n00b_debug_output, cmsg);
}

static void
log_accept(n00b_stream_t *conn)
{
    n00b_ev2_cookie_t *cookie = conn->cookie;
    n00b_string_t     *cmsg   = n00b_cformat("«em6»Connection from «i»«#»",
                                       cookie->aux);
    n00b_print(cmsg);

    n00b_io_subscribe_to_close(conn,
                               n00b_callback_open(connection_closer,
                                                  conn));
}

static void
exit_gracefully(n00b_stream_t *e, int64_t signal, void *aux)
{
    n00b_printf("«em»Shutting down«/» due to signal: «em»«#»",
                n00b_get_signal_name(signal));
    n00b_exit(-1);
}

int
main(int argc, char *argv[], char *envp[])
{
    n00b_init(argc, argv, envp);

    n00b_gc_register_root(&n00b_debug_log_file, 1);
    n00b_gc_register_root(&n00b_debug_cb, 1);
    n00b_gc_register_root(&n00b_debug_output, 1);
    n00b_gc_register_root(&n00b_debug_listener, 1);
    n00b_gc_register_root(&n00b_debug_stdin, 1);

    n00b_terminal_app_setup();
    n00b_disable_debug_server();

    n00b_eprintf("«#»",
                 n00b_call_out(n00b_cstring("Congrats, you now have a n00b  "
                                            "debug logging server.")));

    n00b_string_t *addr      = n00b_get_env(n00b_cstring("N00B_DEBUG_ADDRESS"));
    n00b_string_t *str_port  = n00b_get_env(n00b_cstring("N00B_DEBUG_PORT"));
    n00b_string_t *debug_log = n00b_get_env(n00b_cstring("N00B_DEBUG_LOG_FILE"));
    uint16_t       int_port  = N00B_DEFAULT_DEBUG_PORT;

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

    n00b_io_register_signal_handler(SIGQUIT, (void *)exit_gracefully);
    n00b_io_register_signal_handler(SIGTERM, (void *)exit_gracefully);

    setup_log_file(debug_log);
    n00b_debug_cb = n00b_callback_open(process_debug_msg, NULL);
    n00b_io_set_repr(n00b_debug_cb, n00b_cstring("msg processor cb"));

    n00b_debug_output = n00b_fd_open(fileno(stderr));
    n00b_debug_stdin  = n00b_fd_open(fileno(stdin));

    n00b_stream_t *input_cb = n00b_callback_open(logger_handle_stdin,
                                                 NULL);

    n00b_io_subscribe_to_reads(n00b_debug_stdin, input_cb, NULL);

    // TODO: setup IO and signals and so-on.

    n00b_printf("«em1»Binding to address:«/» «em»«#»:«#»«/»",
                addr,
                (int64_t)int_port);

    n00b_net_addr_t *ip = n00b_new(n00b_type_ip(),
                                   n00b_kw("address",
                                           addr,
                                           "port",
                                           n00b_ka(int_port)));

    n00b_debug_listener = n00b_io_listener(ip,
                                           n00b_debug_cb,
                                           n00b_new_unpickler,
                                           NULL,
                                           log_accept);

    n00b_thread_exit(0);
}
