#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This stub doesn't do anything;
// This being here just avoids some meson complexity for us.

n00b_fd_stream_t *my_stdin, *my_stdout;
n00b_buf_t       *capture;
n00b_string_t    *test_file;

void
echo_it(n00b_fd_stream_t *s,
        n00b_fd_sub_t    *sub,
        n00b_buf_t       *buf,
        void             *thunk)
{
    n00b_eprintf("[|h2|]*[|#|]*", buf);
}

void
test_timer(n00b_timer_t *t, n00b_duration_t *time, void *thunk)
{
    // n00b_show_streams();
    n00b_timer_t *t2 = thunk;
    n00b_remove_timer(t2);
}

void
timer_2(n00b_timer_t *t, n00b_duration_t *time, void *thunk)
{
    printf("Timer 2\n");
}

extern void n00b_dump_gts(void);

void *
callback_test(n00b_buf_t *b, void *thunk)
{
    switch (b->data[0]) {
    case '\e':
        n00b_exit(0);
    case '!':
        printf("\n");
        n00b_show_streams();
        break;
    case 'G':
        n00b_dump_gts();
        break;
    case '@':
        n00b_write(n00b_stderr(), capture);
        break;
    case '%':
        n00b_write(n00b_stderr(), test_file);
        break;
    case '\r':
        n00b_write(n00b_stderr(),
                           n00b_string_to_buffer(n00b_cached_newline()));
        break;
    default:
        break;
    }

    return n00b_string_to_buffer(n00b_cstring("\nLOLOL\n"));
}

void *
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
my_accept(n00b_stream_t *stream, void *ignore)
{
    n00b_printf("[|h6|]Got connection.");
    n00b_stream_t *rcv = n00b_new_callback_stream(sock_rcv,
                                                    NULL,
                                                    n00b_filter_hexdump(0x4a4a4a00));
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
table_obob() {
    n00b_table_t *t = n00b_table("columns", 1);
    n00b_list_t *header = n00b_list(n00b_type_string());
    n00b_list_append(header, n00b_cstring("very long string with multiple breaks"));
    n00b_table_add_row(t, header);
    n00b_list_t  *l = n00b_render(t, 10, -1);
    n00b_string_t *s = n00b_string_join(l, n00b_cached_empty_string());
    n00b_print(s);
}

int
main(void)
{
    n00b_terminal_app_setup();

    n00b_gc_register_root(&my_stdin, 1);
    n00b_gc_register_root(&my_stdout, 1);
    n00b_gc_register_root(&capture, 1);
    n00b_gc_register_root(&test_file, 1);
    n00b_signal_register(SIGHUP, signal_demo, (void *)1ULL);
    n00b_signal_register(SIGHUP, signal_demo2, (void *)2ULL);

    n00b_eprintf("[|red|]Welcome to the hood[|/|]");
    n00b_eprintf("[|p|]");

    n00b_list_t   *args = n00b_list(n00b_type_string());
    n00b_string_t *ls   = n00b_cstring("/bin/ls");

    n00b_list_append(args, n00b_cstring("--color"));
    n00b_list_append(args, n00b_cstring("/"));

    n00b_gc_register_root(&capture, 1);
    n00b_gc_register_root(&test_file, 1);

    test_file         = n00b_read_file(n00b_cstring("meson.build"));
    n00b_proc_t *proc = n00b_run_process(ls, args, true, true);
    capture           = n00b_proc_get_stdout_capture(proc);

    uint64_t         port = 7879;
    n00b_net_addr_t *addr = NULL;
    n00b_stream_t  *srv;

    do {
        N00B_TRY
        {
            addr = n00b_new(n00b_type_net_addr(),
                            n00b_kw("address",
                                    n00b_cstring("127.0.0.1"),
                                    "port",
                                    port));
            srv  = n00b_create_listener(addr);
        }
        N00B_EXCEPT
        {
            port++;
        }
        N00B_TRY_END;
    } while (!srv);

    n00b_debug("test", "Address: [|#|]", addr);

    my_stdin           = n00b_fd_stream_from_fd(0, NULL, NULL);
    my_stdout          = n00b_fd_stream_from_fd(1, NULL, NULL);
    struct timespec t  = {.tv_sec = 5, .tv_nsec = 0};
    struct timespec t2 = {.tv_sec = 10, .tv_nsec = 0};
    bool            err;

    _n00b_fd_read_subscribe(my_stdin, echo_it, 0, &err);
    n00b_add_timer(&t, test_timer, n00b_add_timer(&t2, timer_2));

    n00b_stream_t *instrm = n00b_new_stream_proxy(n00b_stdin());
    n00b_stream_t *log    = n00b_stream_open_file(n00b_cstring("/tmp/testlog"),
                                                 "write_only",
                                                 (int64_t) true,
                                                 "allow_file_creation",
                                                 (int64_t) true);

    log                = n00b_new_stream_proxy(log);
    n00b_stream_t *cb = n00b_new_callback_stream(callback_test, NULL);
    n00b_stream_subscribe_read(instrm, log, false);
    n00b_stream_subscribe_read(instrm, cb, false);
    n00b_stream_subscribe_read(cb, log, false);

    n00b_stream_t *accept_cb = n00b_new_callback_stream(my_accept, NULL);
    n00b_stream_subscribe_read(srv, accept_cb, false);

    // table_obob();

    n00b_eprintf("Your two minutes begins on the first tick.");
    n00b_eprintf("ESC exits early; ! shows subscriptions.");
    for (int i = 0; i < 120; i++) {
        n00b_sleep_ms(1000);
        if (!(i % 15)) {
            n00b_eprintf("[|em1|]tick..");
        }
    }
    n00b_eprintf("[|em4|]Boom!");
    n00b_exit(0);
}
