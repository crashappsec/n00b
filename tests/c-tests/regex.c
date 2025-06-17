#include "n00b.h"

void
ansi_test(n00b_buf_t *b)
{
    n00b_ansi_ctx *c = n00b_ansi_parser_create();

    n00b_ansi_parse(c, b);

    n00b_list_t *r = n00b_ansi_parser_results(c);

#if 0
    int len = n00b_list_len(r);
    for (int i = 0; i < len; i++) {
        n00b_ansi_node_t *n = n00b_list_get(r, i, NULL);
        n00b_printf("[|#|]: [|#|]", (int64_t)(i + 1), n00b_ansi_node_repr(n));
    }

    //    char *p = b->data;
#endif

    n00b_printf("Okay, reassemble, stripping:\n");
    // printf("\e[?69h\e[8;40;40t");
    n00b_printf("[|#|]\n", n00b_ansi_nodes_to_string(r, false));
    n00b_printf("Okay, reassemble, NOT stripping:\n");

    n00b_list_t *parts = n00b_string_split(n00b_ansi_nodes_to_string(r, true),
                                           n00b_cached_newline());

    for (int i = 0; i < n00b_list_len(parts); i++) {
        n00b_printf("[|#|]", n00b_list_get(parts, i, NULL));
        printf("\n");
    }

    printf("%s", n00b_ansi_nodes_to_string(r, true)->data);
    /*
    for (int i = 0; i < b->byte_len; i++) {
        if (*p == 0x1b) {
            *p = '^';
        }
        p++;
    }
    printf("%s\n", b->data);
    */
}

void
regex_test(n00b_buf_t *b)
{
    n00b_string_t *s   = n00b_utf8(b->data, b->byte_len);
    n00b_string_t *pat = n00b_cstring("\\[34m([a-zA-Z_.]*)\\^\\[.*m");
    n00b_regex_t  *re  = n00b_regex_unanchored(pat);
    n00b_list_t   *l   = n00b_match_all(re, s);

    for (int i = 0; i < n00b_list_len(l); i++) {
        n00b_match_t  *m = n00b_list_get(l, i, NULL);
        n00b_string_t *cap;

        if (m->captures) {
            assert(n00b_list_len(m->captures) == 1);
            cap = n00b_list_pop(m->captures);
        }
        else {
            cap = n00b_cstring("<<None>>");
        }

        n00b_printf("Match [|#|] ([|#|]-[|#|]): [|#|]",
                    (int64_t)(i + 1),
                    m->start,
                    m->end,
                    cap);
    }
}

int
main()
{
    n00b_terminal_app_setup();
    n00b_string_t *cmd = n00b_cstring("ls");
    cmd                = n00b_find_first_command_path(cmd, NULL, true);
    n00b_list_t *l     = n00b_list(n00b_type_string());
    n00b_list_append(l, n00b_cstring("--color"));
    //    n00b_list_append(l, n00b_cstring("../../src"));
    n00b_proc_t *pi = n00b_run_process(cmd,
                                       l,
                                       false,
                                       true,
                                       n00b_header_kargs("pty",
                                                         1ULL,
                                                         "err_pty",
                                                         1ULL));

    n00b_buf_t *bout = n00b_proc_get_stdout_capture(pi);

    // end
    printf("stdout capture: %s\n",
           (bout && bout->byte_len) ? bout->data : "(none)");

    n00b_printf("«em1»Subprocess completed with error code «#».",
                (int64_t)n00b_proc_get_exit_code(pi));
    ansi_test(bout);
    regex_test(bout);
}
