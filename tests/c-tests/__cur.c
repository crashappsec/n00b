#define N00B_USE_INTERNAL_API
#include "n00b.h"

// This stub doesn't do anything;
// This being here just avoids some meson complexity for us.

n00b_fd_stream_t *my_stdin, *my_stdout;

void
echo_it(n00b_fd_stream_t *s, n00b_fd_sub_t *sub, n00b_buf_t *buf, void *thunk)
{
    n00b_fd_write(my_stdout, buf->data, buf->byte_len);
    n00b_fd_write(my_stdout, "*", 1);
}

void
test_timer(n00b_timer_t *t, n00b_duration_t *time, void *thunk)
{
    printf("Hey you.\nGoing to save you from the 10 sec timer.\n");
    n00b_timer_t *t2 = thunk;

    n00b_remove_timer(t2);
}

void
timer_2(n00b_timer_t *t, n00b_duration_t *time, void *thunk)
{
    printf("PSYCHE!!!\n");
}

extern void n00b_fd_init_io(void);

void *
callback_test(n00b_buf_t *b, void *thunk)
{
    n00b_printf("subscribed callback got: [|#|]", b);
    if (b->data[0] == '\e') {
        n00b_exit(0);
    }
    return n00b_string_to_buffer(n00b_cstring("\nLOLOL\n"));
}

void *
sock_close(n00b_channel_t *chan, void *ignore)
{
    n00b_printf("Lost connection.");
    return NULL;
}

void *
sock_rcv(n00b_buf_t *b, void *ignore)
{
    n00b_printf("SOCK this bro: [|#|]", b);
    return NULL;
}

void *
my_accept(n00b_channel_t *chan, void *ignore)
{
    n00b_channel_t *rcv   = n00b_new_callback_channel(sock_rcv);
    n00b_channel_t *close = n00b_new_callback_channel(sock_close);
    n00b_channel_subscribe_read(chan, rcv, false);
    n00b_channel_subscribe_close(chan, close);
    n00b_printf("Got connection.");

    return NULL;
}

int
main(void)
{
    n00b_terminal_app_setup();
    n00b_fd_init_io();

    n00b_net_addr_t *addr = n00b_new(n00b_type_net_addr(),
                                     n00b_kw("address",
                                             n00b_cstring("127.0.0.1"),
                                             "port",
                                             7878ULL));

    n00b_debugf("test", "Address: [|#|]", addr);
    n00b_channel_t *srv = n00b_create_listener(addr);

    my_stdin           = n00b_fd_stream_from_fd(0, NULL, NULL);
    my_stdout          = n00b_fd_stream_from_fd(1, NULL, NULL);
    struct timespec d  = {.tv_sec = 200, .tv_nsec = 0};
    struct timespec t  = {.tv_sec = 5, .tv_nsec = 0};
    struct timespec t2 = {.tv_sec = 10, .tv_nsec = 0};

    _n00b_fd_read_subscribe(my_stdin, echo_it, 0);
    n00b_add_timer(&t, test_timer, n00b_add_timer(&t2, timer_2));

    n00b_channel_t *inchan = n00b_chan_stdin();
    n00b_channel_t *log    = _n00b_channel_open_file(n00b_cstring("/tmp/testlog"),
                                                  n00b_kw("write_only",
                                                          (int64_t) true,
                                                          "allow_file_creation",
                                                          (int64_t) true));

    n00b_channel_t *cb = n00b_new_callback_channel(callback_test);
    n00b_channel_subscribe_read(inchan, log, false);
    n00b_channel_subscribe_read(inchan, cb, false);
    n00b_channel_subscribe_read(cb, log, false);

    n00b_channel_t *accept_cb = n00b_new_callback_channel(my_accept);
    n00b_channel_subscribe_read(srv, accept_cb, false);

    n00b_fd_run_evloop(n00b_system_dispatcher, (n00b_duration_t *)&d);

    printf("Timeout completed.\n");
    return 0;
}
