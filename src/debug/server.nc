#define N00B_USE_INTERNAL_API
#include "n00b.h"

#if defined(N00B_DEBUG_SERVER)

static n00b_condition_t exit_condition;
static n00b_list_t     *active_connections;

static void *
received_msg(n00b_wire_dmsg_t *msg, n00b_net_addr_t *addr)
{
    n00b_debug_msg_t *local = n00b_gc_alloc_mapped(n00b_debug_msg_t,
                                                   N00B_GC_SCAN_ALL);

    local->topic          = msg->topic;
    local->timestamp      = msg->timestamp;
    local->payload        = msg->payload;
    local->remote_address = addr;

    n00b_apply_debug_workflow(local);

    return NULL;
}

static void *
sock_close(n00b_stream_t *stream, n00b_net_addr_t *addr)
{
    n00b_eprintf("Connection from [|em1|][|#|] [|em3|]CLOSED[|/|][|p|]", addr);
    n00b_list_remove_item(active_connections, stream);

    return NULL;
}

void *
sock_debug(n00b_buf_t *b, void *ignore)
{
    n00b_print(b);
    return NULL;
}

static void *
accept_debug(n00b_stream_t *stream, void *ignore)
{
    n00b_filter_add(stream, n00b_filter_unmarshal(false));
    n00b_net_addr_t *addr = n00b_stream_net_address(stream);

    n00b_stream_t *rcv_cb   = n00b_new_callback_stream((void *)received_msg,
                                                     addr);
    n00b_stream_t *close_cb = n00b_new_callback_stream((void *)sock_close,
                                                       addr);
    n00b_stream_subscribe_read(stream, rcv_cb, false);
    n00b_stream_subscribe_close(stream, close_cb);
    n00b_list_append(active_connections, stream);

    n00b_eprintf("Received connection from [|em1|][|#|]", addr);

    return NULL;
}

static volatile bool is_shutdown = false;

static void *
user_input(n00b_buf_t *buf, void *ingored)
{
    int            n      = buf->byte_len;
    n00b_stream_t *stream = NULL;

    for (int i = 0; i < n; i++) {
        switch (buf->data[i]) {
        case '\e':
            while ((stream = n00b_private_list_pop(active_connections)) != 0) {
                n00b_close(stream);
            }
            is_shutdown = true;

            /*
            n00b_lock_acquire(&exit_condition);
            n00b_condition_notify_all(&exit_condition);
            n00b_lock_release(&exit_condition);
            */
            return NULL;

        case '!':
            n00b_show_streams();
            continue;
        default:
            continue;
        }
    }

    return NULL;
}

n00b_stream_t *
n00b_start_log_listener(void)
{
    n00b_string_t *addr     = n00b_get_env(n00b_cstring(N00B_ENV_DBG_ADDR));
    n00b_string_t *str_port = n00b_get_env(n00b_cstring(N00B_ENV_DBG_PORT));
    uint16_t       int_port = N00B_DEFAULT_DEBUG_PORT;

    // in workflow.c
    n00b_local_process_is_server = true;

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

    n00b_stream_t *result = NULL;

    N00B_TRY
    {
        n00b_net_addr_t *inet_addr = n00b_new(n00b_type_net_addr(),
                                              address : addr, port : int_port);
        result                     = n00b_create_listener(inet_addr);
    }
    N00B_EXCEPT
    {
    }
    N00B_TRY_END;

    if (!result) {
        return NULL;
    }

    n00b_stream_t *accept_cb = n00b_new_callback_stream(accept_debug, NULL);
    n00b_stream_subscribe_read(result, accept_cb, false);

    return result;
}

extern n00b_condition_t n00b_io_exit_request;
int
n00b_debug_entry(int argc, char **argv, char **envp)
{
    n00b_terminal_app_setup();
    n00b_gc_register_root(&active_connections, 1);
    active_connections = n00b_list(n00b_type_stream());

    n00b_condition_init(&exit_condition);
    n00b_lock_acquire(&exit_condition);

    n00b_stream_t *listener = n00b_start_log_listener();

    if (!listener) {
        n00b_eprintf(
            "«red»error:«/» Could not start listener:«/» "
            "Address already in use.");
        n00b_exit(-1);
    }

    n00b_net_addr_t *addr = n00b_stream_net_address(listener);

    n00b_eprintf("N00b debug server running on «em»«#»:«#»«/»«p» ",
                 addr,
                 (int64_t)n00b_get_net_addr_port(addr));
    n00b_eprintf("Press «i»'ESC'«/» to quit the server.«p»");
    n00b_eprintf("Press «i»'!'«/» to see active I/O subscriptions.");

    n00b_stream_subscribe_read(n00b_stdin(),
                               n00b_new_callback_stream(user_input, NULL),
                               false);

    while (!is_shutdown) {
        n00b_sleep_ms(100);
    }

    n00b_eprint(n00b_crich("«em»Shutting down debugging server."));
    n00b_close(listener);
    n00b_exit(0);
}

#endif
