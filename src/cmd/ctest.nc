#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_stream_t *test_sin  = NULL;
n00b_stream_t *test_sout = NULL;
n00b_stream_t *test_scb  = NULL;
n00b_stream_t *test_srv  = NULL;

void
iotest(n00b_stream_t *p1, void *s)
{
    static int i = 0;

    // If we add the ANSI filter, it will line buffer. There's no
    // flush implementation yet.

    n00b_type_t *t = n00b_get_my_type(s);
    if (n00b_type_is_buffer(t)) {
        s = n00b_buf_to_utf8_string(s);
    }
    else {
        assert(n00b_type_is_string(t));
    }

    n00b_string_t *exit_code1 = n00b_string_from_codepoint(0x03);
    n00b_string_t *exit_code2 = n00b_string_from_codepoint('q');

    if (n00b_string_find(s, exit_code1) != -1) {
        n00b_printf("[em]See ya!");
        n00b_exit(0);
    }

    if (n00b_string_find(s, exit_code2) != -1) {
        n00b_printf("[em]See ya, q!");
        n00b_exit(0);
    }

    n00b_write(n00b_stderr(), s);

    n00b_debug("iteration", (void *)(uint64_t)i++);
    n00b_debug("testing", s);
    n00b_debug("testing", (void *)1000ull);
    n00b_debug("testing", ((char *)s) + 16);
}

void
topic_cb(void *ignore, n00b_message_t *msg, void *thunk)
{
    n00b_printf("[h1]{}:[/] {}", msg->topic, msg->payload);
}

void
setup_io_test(void)
{
    int fd = open("/tmp/sometest",
                  O_CREAT | O_CLOEXEC | O_RDWR | O_TRUNC,
                  S_IRWXU | S_IROTH | S_IRGRP);

    if (fd < 0) {
        fd = open("/tmp/sometest", O_CLOEXEC | O_RDWR | O_TRUNC);
    }

    assert(fd >= 0);

    n00b_gc_register_root(&test_sin, 1);
    n00b_gc_register_root(&test_sout, 1);
    n00b_gc_register_root(&test_scb, 1);
    n00b_gc_register_root(&test_srv, 1);

    test_sin  = n00b_fd_open(fileno(stdin));
    test_sout = n00b_fd_open(fileno(stderr));
    test_srv  = n00b_fd_open(fd);

    n00b_stream_t *sout = n00b_fd_open(fileno(stdout));

    n00b_io_set_repr(test_sin, n00b_cstring("[stdin]"));
    n00b_io_set_repr(test_sout, n00b_cstring("[stderr]"));
    n00b_io_set_repr(test_srv, n00b_cstring("[debug server]"));
    n00b_io_set_repr(sout, n00b_cstring("[stdout]"));

    n00b_io_subscribe_to_reads(test_sin, test_srv, NULL);
}

static void
exit_gracefully(n00b_stream_t *e, int64_t signal, void *aux)
{
    n00b_printf("«em»Shutting down«/» due to signal: «em»«#»",
                n00b_get_signal_name(signal));
    n00b_exit(-1);
}

#if 0
static void
report_signal(n00b_stream_t *e, int64_t signal, void *aux)
{
    n00b_printf("«#»Got«/» signal: «em»«#»", n00b_get_signal_name(signal));
}
#endif

int
main(int argc, char **argv, char **envp)
{
    // Miro: These two lines to wrap
    n00b_init(argc, argv, envp);
    n00b_terminal_app_setup();
    // END

    n00b_io_register_signal_handler(SIGTERM, (void *)exit_gracefully);
    // n00b_ignore_uncaught_io_errors();

    n00b_string_t *test_str = n00b_cformat("«#»Welcome to our testing.");
    n00b_print(test_str);

    printf("Call json parse.\n");
    void *r = n00b_json_parse(n00b_cstring("{\"foo\" : [1.2, true]}"), NULL);
    printf("Done.\n");
    n00b_printf("via n00b_to_json: «em»«#»«/»", n00b_to_json(r));

    n00b_debug("testing", test_str);

    n00b_stream_t *t = n00b_add_debug_topic("jtest");
    n00b_add_to_json_xform_on_write(t);

    n00b_debug("jtest", r);
    r = NULL;

#if 1
    n00b_buf_t *t2 = n00b_automarshal(test_str);
    void       *t3 = n00b_autounmarshal(t2);
    n00b_debug("t1", test_str);
    n00b_debug("t2", t2);
    n00b_debug("t3", t3);
#endif

    n00b_gc_register_root(&test_sin, 1);
    n00b_gc_register_root(&test_sout, 1);
    n00b_gc_register_root(&test_scb, 1);
    n00b_gc_register_root(&test_srv, 1);

    sigset_t saved_set, cur_set;

    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &cur_set, &saved_set);

#if 1
    setup_io_test();
    n00b_stream_t *cb = n00b_callback_open((void *)iotest, NULL);
    n00b_io_set_repr(cb, n00b_cstring("[input cb]"));

    /*n00b_stream_sub_t *sub =*/
    n00b_io_subscribe(test_sin, cb, NULL, n00b_io_sk_read);
#endif

#if 1
    n00b_printf("«i»«b»Items opened«/»: «reverse»«#», «#», «#», «#»",
                test_sin,
                test_sout,
                test_srv,
                cb);
#endif

#if 1
    n00b_stream_t *tst = n00b_get_topic(n00b_cstring("test"), NULL);
    n00b_add_marshaling(tst);
    n00b_add_unmarshaling(tst);

    cb = n00b_callback_open((void *)topic_cb, NULL);
    n00b_ensure_utf8_on_write(cb);
    n00b_topic_subscribe(tst, cb);

    n00b_topic_post(tst,
                    n00b_cformat("«em»A little test of the topic system."));
    n00b_topic_post(tst,
                    n00b_cformat("«em»Test 2."));

    uint64_t *basic = n00b_box_u64(0xddddddddddddddddull);
    n00b_topic_post(tst, basic);
#endif
#if 1
    // Miro: these lines to wrap
    n00b_string_t *cmd = n00b_cstring("/bin/ls");
    n00b_list_t   *l   = n00b_list(n00b_type_string());
    n00b_list_append(l, n00b_cstring("-alG"));
    n00b_list_append(l, n00b_cached_period());

    n00b_proc_t *pi = n00b_run_process(cmd, l, true, true, "pty" : true);

    n00b_buf_t *b = n00b_proc_get_stdout_capture(pi);
    // End

    n00b_debug("tty",
               n00b_cformat("«em2»len(output):«/»\n«#»", n00b_buffer_len(b)));

    n00b_printf("«em1»Subprocess completed with error code «#».",
                (int64_t)n00b_proc_get_exit_code(pi));
    n00b_debug("tty", n00b_crich("«em1»DONE."));
#endif

#if 0
    n00b_gopt_ctx      *opt_ctx = setup_cmd_line();
    n00b_gopt_result_t *opt_res = n00b_run_getopt_raw(opt_ctx, NULL, NULL);

    n00b_print(n00b_grammar_format(opt_ctx->grammar));

    if (!opt_res) {
        n00b_exit(1);
    }
    n00b_string_t        *cmd     = n00b_gopt_get_command(opt_res);

    if (n00b_gopt_has_errors(opt_res)) {
        n00b_list_t *errs = n00b_gopt_get_errors(opt_res);
        n00b_printf("«red»error: «/» «#»", n00b_list_get(errs, 0, NULL));
        n00b_getopt_show_usage(opt_ctx, cmd);
        n00b_exit(1);
    }

    n00b_list_t *args = n00b_gopt_get_args(opt_res, cmd);

    n00b_printf("«em2»Argument: «/»«em»«#»", n00b_list_get(args, 0, NULL));
    n00b_string_t *cmd  = n00b_cstring("/usr/local/bin/docker");
    n00b_list_t *args = n00b_list(n00b_type_string());
    n00b_list_append(args, n00b_cstring("--version"));
    n00b_cmd_out_t *out = n00b_subproc(cmd, args);

#endif
    n00b_thread_exit(0);
}
