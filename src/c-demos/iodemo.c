#include "n00b.h"

#if 0
void
test_timer(n00b_timer_t *t, n00b_duration_t *time, void *param)
{
    n00b_timer_t *t2 = param;
    n00b_remove_timer(t2);
}

void
timer_2(n00b_timer_t *t, n00b_duration_t *time, void *param)
{
    printf("Timer 2\n");
}
#endif

void *
input_callback(n00b_buf_t *b, void *capture)
{
    if (!b) {
        return NULL;
    }

    switch (b->data[0]) {
    case '\e':
        n00b_exit(0);
    case '!':
        n00b_write(n00b_stderr(), n00b_cached_newline());
        n00b_show_streams();
        break;
    case '@':
        // Write the data we read from the subprocess.
        n00b_write(n00b_stderr(), capture);
        break;
    case '%':
        n00b_write(n00b_stderr(), n00b_read_file(n00b_cstring("~/.profile")));
        break;
    case '\r':
        n00b_write(n00b_stderr(), n00b_cached_newline());
        break;
    default:
        n00b_eprintf("[|em2|]*[|#|]*", b);
        break;
    }

    return n00b_string_to_buffer(n00b_cstring("\nLOLOL\n"));
}

static void *
sock_close(n00b_stream_t *stream, void *rcv)
{
    n00b_flush(rcv);
    n00b_queue(n00b_stdout(),
               n00b_crich("[|red|]Lost connection."));
    return NULL;
}

void *
sock_rcv(n00b_buf_t *b, void *ignore)
{
    n00b_print(b);
    return NULL;
}

void *
demo_accept(n00b_stream_t *stream, void *ignore)
{
    n00b_printf("[|h6|]Got connection.");
    n00b_stream_t *rcv = n00b_new_callback_stream(sock_rcv,
                                                  NULL,
                                                  n00b_filter_hexdump(0x00));
    n00b_stream_subscribe_read(stream, rcv, false);

    n00b_stream_t *close = n00b_new_callback_stream(sock_close, rcv);
    n00b_stream_subscribe_close(stream, close);
    return NULL;
}

void
signal_demo(int signal, siginfo_t *info, void *user_param)
{
    n00b_show_streams();
}

void
signal_demo2(int signal, siginfo_t *info, void *user_param)
{
    n00b_printf(
        "[|red|]Received SIGHUP:[|/|] "
        "pid = [|#|], uid = [|#|], "
        "addr = [|#:p|]",
        (int64_t)info->si_pid,
        (int64_t)info->si_uid,
        info->si_addr);
}

void
print_eating_newline()
{
    n00b_string_t *rich = n00b_cformat(
        "[|b|]b[|/b|][|i|]i[|/|][|em3|]em3[|/em3|][|i|]i[|/i|]\n"
        "[|em|]one [|em2|]two[|/em2|] [|em3|]three[|/em3|] one[|/|]\n"
        "[|b|]b [|i|]&i[|/i|] [|u|]&u[|/u|] [|i|]&i[|/i|] b[|/b|]\n"
        "[|b|][|i|]b&i [|u|]&u[|/|]\n"
        "[|b|]b[|/b|][|i|]i[|/|][|em3|]em3[|/em3|][|em2|]em2[|/em2|][|/|][|i|]i[|/i|] [|em|]em[|/em|][|em2|]em2[|/|] [|b|]b [|i|]&i[|/i|] b[|/b|] [|b|][|i|]b&i[|/|] [|em3|]em3[|/em|][|em4|]em4[|/|]\n");
    n00b_print(rich);

    return;
}

int
main(void)
{
    n00b_futex_t futex = 100;
    n00b_futex_init(&futex);
    n00b_terminal_app_setup();

    n00b_signal_register(SIGHUP, signal_demo, (void *)1ULL);
    n00b_signal_register(SIGHUP, signal_demo2, (void *)2ULL);

    // eprintf() and printf() use the stream API (n00b_printf() takes
    // an optional stream parameter.
    n00b_eprintf("[|red|]Welcome to the IO demo[|/|]");
    n00b_eprintf("[|p|]");

    // The process API uses the stream API extensively:
    // 1) For routing data between parent and child.
    // 2) For handling data capture.
    // 3) For dealing w/ subprocess exit.

    n00b_list_t   *args = n00b_list(n00b_type_string());
    n00b_string_t *ls   = n00b_cstring("/bin/ls");

    n00b_list_append(args, n00b_cstring("--color"));
    n00b_list_append(args, n00b_cstring("/"));

    n00b_sleep_ms(500);
    n00b_proc_t *proc    = n00b_run_process(ls, args, true, true);
    n00b_buf_t  *capture = n00b_proc_get_stdout_capture(proc);

    uint64_t         port = 7879;
    n00b_net_addr_t *addr = NULL;
    n00b_stream_t   *srv;

    do {
        N00B_TRY
        {
            addr = n00b_new(n00b_type_net_addr(),
                            n00b_kargs_obj(
                                "address",
                                (int64_t)n00b_cstring("127.0.0.1"),
                                "port",
                                port,
                                NULL));
            srv  = n00b_create_listener(addr);
        }
        N00B_EXCEPT
        {
            port++;
        }
        N00B_TRY_END;
    } while (!srv);

    // The debug system uses the stream system, with logic to use a
    // local debug server, or fail over to stderr, when not available.
    n00b_debugf("test", "Address: [|#|]", addr);

    n00b_stream_t *log = n00b_stream_open_file(n00b_cstring("/tmp/testlog"),
                                               n00b_kargs_obj(
                                                   "write_only",
                                                   (int64_t) true,
                                                   "allow_file_creation",
                                                   (int64_t) true));

    n00b_stream_t *cb = n00b_new_callback_stream(input_callback, capture);
    n00b_stream_subscribe_read(n00b_stdin(), log, false);
    n00b_stream_subscribe_read(n00b_stdin(), cb, false);

    n00b_stream_t *accept_cb = n00b_new_callback_stream(demo_accept, NULL);
    n00b_stream_subscribe_read(srv, accept_cb, false);

    n00b_eprintf("Your two minutes begins on the first tick.");
    n00b_eprintf("ESC exits early; ! shows subscriptions.");

    for (int i = 0; i < 120; i++) {
        n00b_sleep_ms(1000);
        if (!(i % 15)) {
            n00b_string_t *s = n00b_cformat("[|em1|]tick...");
            n00b_debug("test", s);
        }
    }
    n00b_eprintf("[|em4|]Boom!");
    n00b_exit(0);
}
