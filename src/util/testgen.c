#define N00B_USE_INTERNAL_API
#include "n00b.h"

// SHORT-TERM utilities for using sessions to automatically generate
// test cases. Once sessions get integrated into the language
// properly, we will do a lot better.
//
// Currently this will do the following:
//
// 1. Set up a capture-replay for a terminal session, and inject the
// first command if you provide it.
//
//
// 2. Will record all events automatically, and move the capture file
//    into the current directory.
//
// 3. Will post-process the message, using bash commands when not
//    inside a program, and stdin when inside a program.
//
// 4. Produces a "script" file you can edit to massage the test case
// (it starts out with exact matching).
//
// 5. Can the file and the script and re-run the test case.

#define MERGE_HDR n00b_cstring(              \
    "# The capture merged stdout/stderr. "   \
    "This command ensures replays do too.\n" \
    "MERGE\n")

#define ANSI_HDR n00b_cstring(                                        \
    "# Recording used the --ansi flag; this next directive ensures\n" \
    "# that ansi is kept when matching during replay.\n"              \
    "ANSI\n")

#define INJECT_HDR \
    n00b_cstring(  \
        "# Leave the following command uncommented to auto-inject the text:\n")
#define EXPECT_HDR                                                            \
    n00b_cstring(                                                             \
        "# Leave the following for an exact match of the core text below.\n"  \
        "# Change the verb to PATTERN to use an (unanchored) regexp.\n"       \
        "# ERR_ in front of a verb matches the error stream; otherwise, it\n" \
        "# will match the output stream.\n")
#define PROMPT_HDR                                                 \
    n00b_cstring(                                                  \
        "# PROMPT matches whenever the starting shell is bash, \n" \
        "# and that shell gives you a prompt.\n"                   \
        "# If you run tasks in the foreground, it will match\n"    \
        "# on processes exiting.\n")
#define WINCH_HDR \
    n00b_cstring("# This sets the width and height of the test terminal.\n")

static inline void
gen_inject(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr)
{
    n00b_string_t *s;

    if (hdr) {
        n00b_queue(output, INJECT_HDR);
    }

    s = n00b_cformat("[|#|]\n", event->contents);
    s = n00b_string_escape(s);
    n00b_queue(output, n00b_cformat("INJECT [|#|]\n", s));
}

static inline void
gen_expect(n00b_cap_event_t *event,
           n00b_stream_t    *output,
           bool              ansi,
           bool              hdr,
           bool              err)
{
    n00b_string_t *s = n00b_ansi_nodes_to_string(event->contents, ansi);

    s = n00b_string_escape(s);

    if (hdr) {
        n00b_queue(output, EXPECT_HDR);
    }

    if (err) {
        n00b_queue(output, n00b_cformat("ERR_EXPECT [|#|]\n", s));
    }
    else {
        n00b_queue(output, n00b_cformat("EXPECT [|#|]\n", s));
    }
}

static inline void
gen_prompt(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr)
{
    if (hdr) {
        n00b_queue(output, PROMPT_HDR);
    }

    n00b_queue(output, n00b_cformat("PROMPT\n"));
}

static inline void
gen_winch(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr)
{
    if (hdr) {
        n00b_queue(output, WINCH_HDR);
    }

    struct winsize *ws  = event->contents;
    int64_t         col = ws->ws_col;
    int64_t         row = ws->ws_row;

    n00b_queue(output,
               n00b_cformat("SIZE [|#|]:[|#|]\n", col, row));
}

static inline void
gen_spawn(n00b_session_t   *session,
          n00b_cap_event_t *event,
          n00b_stream_t    *output)
{
    n00b_cap_spawn_info_t *info  = event->contents;
    n00b_duration_t       *start = session->start_time;
    n00b_date_time_t      *dt    = n00b_datetime_from_duration(start);

    n00b_queue(output,
               n00b_cformat("# Ran [|#|]:[|#|]\n", info->command, info->args));
    n00b_queue(output,
               n00b_cformat("# @[|#|]\n", dt));
}

static inline void
gen_end(n00b_stream_t *output)
{
    n00b_queue(output, n00b_cstring("# End of session.\n"));
}

static n00b_string_t *
build_testgen_script(n00b_session_t *session,
                     n00b_list_t    *events,
                     n00b_string_t  *base_path,
                     bool            ansi,
                     bool            merge)
{
    n00b_stream_t *script              = n00b_tempfile(NULL, NULL);
    n00b_string_t *tname               = n00b_stream_get_name(script);
    bool           gen_user_input      = false;
    bool           show_inject_comment = true;
    bool           show_expect_comment = true;
    bool           show_prompt_comment = true;
    bool           show_winch          = true;
    bool           expect_stuff        = false;

    int n = n00b_list_len(events);

    if (merge) {
        n00b_queue(script, MERGE_HDR);
    }
    if (ansi) {
        n00b_queue(script, ANSI_HDR);
    }

    for (int i = 0; i < n; i++) {
        n00b_cap_event_t *event = n00b_list_get(events, i, NULL);

        switch (event->kind) {
        case N00B_CAPTURE_STDIN:
        case N00B_CAPTURE_INJECTED:
            if (gen_user_input) {
                gen_inject(event, script, show_inject_comment);
            }
            show_inject_comment = false;
            continue;
        case N00B_CAPTURE_STDOUT:
            if (expect_stuff) {
                gen_expect(event, script, ansi, show_expect_comment, false);
                show_expect_comment = false;
            }
            continue;
        case N00B_CAPTURE_STDERR:
            if (expect_stuff) {
                gen_expect(event, script, ansi, show_expect_comment, true);
                show_expect_comment = false;
            }
            continue;
        case N00B_CAPTURE_CMD_RUN:
            expect_stuff = true;
            gen_inject(event, script, show_inject_comment);
            gen_user_input      = true;
            show_inject_comment = false;
            continue;
        case N00B_CAPTURE_CMD_EXIT:
            if (i != 0) {
                gen_prompt(event, script, show_prompt_comment);
            }
            gen_user_input      = false;
            show_prompt_comment = false;
            expect_stuff        = false;
            continue;
        case N00B_CAPTURE_END:
            gen_end(script);
            continue;
        case N00B_CAPTURE_SPAWN:
            gen_spawn(session, event, script);
            continue;
        case N00B_CAPTURE_WINCH:
            gen_winch(event, script, show_winch);
            show_winch = false;
            continue;
        default:
            n00b_unreachable();
        }
    }

    n00b_close(script);

    n00b_string_t *dst = n00b_cformat("[|#|].test", base_path);
    n00b_rename(tname, dst);

    return dst;
}

void
n00b_testgen_record(n00b_string_t *test_name, bool quiet, bool ansi, bool merge)
{
    n00b_session_t *session;
    n00b_list_t    *events;
    n00b_string_t  *filename;
    n00b_string_t  *path  = n00b_resolve_path(test_name);
    n00b_list_t    *parts = n00b_path_parts(path);
    n00b_string_t  *ext   = n00b_list_pop(parts);
    n00b_string_t  *base  = n00b_path_join(parts);

    if (!n00b_string_codepoint_len(ext)) {
        ext  = n00b_cstring("cap10");
        path = n00b_cformat("[|#|].[|#|]", base, ext);
    }

    if (!quiet) {
        n00b_eprintf("[|em4|]Beginning test shell.");
    }

    // Interactive shell that gets recorded.
    session = n00b_new(n00b_type_session(),
                       n00b_kw("capture_tmpfile",
                               n00b_ka(true),
                               "merge_output",
                               n00b_ka(merge)));

    n00b_session_run(session);

    if (!quiet) {
        n00b_eprintf("[|em4|]Test shell exited.");
    }
    events   = n00b_session_capture_extractor(session, N00B_CAPTURE_ALL);
    filename = n00b_session_capture_filename(session);
    n00b_close(session->unproxied_capture);
    path = n00b_rename(filename, path);

    if (!quiet) {
        n00b_eprintf("[|em2|]Capture saved to: [|i|][|#|]", path);
    }
    n00b_string_t *scr = build_testgen_script(session,
                                              events,
                                              base,
                                              ansi,
                                              merge);
    if (!quiet) {
        n00b_eprintf("[|em2|]Draft script saved to: [|i|][|#|]", scr);
    }
}

static n00b_test_cmd_t *
add_command(n00b_test_file_cmd cmd, n00b_list_t *out)
{
    n00b_test_cmd_t *result = n00b_gc_alloc_mapped(n00b_test_cmd_t,
                                                   N00B_GC_SCAN_ALL);
    result->cmd             = cmd;

    n00b_list_append(out, result);

    return result;
}

static n00b_string_t *
chop_verb(n00b_string_t *line)
{
    int n = n00b_string_find(line, n00b_cached_space());
    line  = n00b_string_slice(line, n, -1);
    return n00b_string_strip(line);
}

static inline n00b_string_t *
process_simple_escapes(n00b_string_t *s)
{
    return n00b_string_unescape(s, NULL);
}

static inline void
add_inject(n00b_test_t *t, n00b_string_t *line, n00b_list_t *out)
{
    n00b_test_cmd_t *cmd = add_command(N00B_TF_INJECT, out);
    line                 = chop_verb(line);

    cmd->payload.text = process_simple_escapes(line);
}

static inline void
add_expect(n00b_test_t   *t,
           n00b_string_t *line,
           n00b_list_t   *out,
           bool           err)
{
    n00b_test_file_cmd cmd_name;

    if (err) {
        cmd_name = N00B_TF_ERR_EXPECT;
    }
    else {
        cmd_name = N00B_TF_EXPECT;
    }

    n00b_test_cmd_t *cmd = add_command(cmd_name, out);
    line                 = chop_verb(line);
    cmd->payload.text    = process_simple_escapes(line);
}

static inline void
add_pattern(n00b_string_t *line, n00b_list_t *out, bool err)
{
    n00b_test_file_cmd cmd_name = err ? N00B_TF_ERR_PATTERN : N00B_TF_PATTERN;
    n00b_test_cmd_t   *cmd      = add_command(cmd_name, out);
    line                        = chop_verb(line);
    cmd->payload.pattern        = n00b_regex_multiline(line, false);
}

static inline void
add_prompt(n00b_list_t *out)
{
    add_command(N00B_TF_PROMPT, out);
}

static inline void
add_size(n00b_string_t *line, n00b_list_t *out)
{
    n00b_test_cmd_t *cmd = add_command(N00B_TF_SIZE, out);
    line                 = chop_verb(line);
    n00b_list_t *l       = n00b_string_split(line, n00b_cached_colon());

    if (n00b_list_len(l) != 2) {
        goto err;
    }
    n00b_string_t *s_col = n00b_list_get(l, 0, NULL);
    n00b_string_t *s_row = n00b_list_get(l, 1, NULL);
    int64_t        col, row;

    if (!n00b_parse_int64(s_col, &col)) {
        goto err;
    }
    if (!n00b_parse_int64(s_row, &row)) {
        goto err;
    }
    if (col < 0 || row < 0) {
        goto err;
    }
    cmd->payload.dims.ws_col = col;
    cmd->payload.dims.ws_row = row;

    return;

err:
    N00B_CRAISE("Invalid SIZE command; must be in the form of COLS:ROWS");
}

static inline void
add_timeout(n00b_string_t *line, n00b_test_t *test)
{
    line = chop_verb(line);

    if (test->got_timeout) {
        N00B_CRAISE(
            "Duplicate timeout set. One timeout put anywhere in "
            "the test file applies to the entire test.");
    }

    if (!n00b_parse_int64(line,
                          (int64_t *)&test->timeout_sec)
        || ((int64_t)test->timeout_sec) < 0) {
        N00B_CRAISE("Timeout must be an integer number of seconds.");
    }

    test->got_timeout = true;
}

static bool
n00b_parse_test_file(n00b_test_t *test)
{
    n00b_string_t *base     = test->path;
    n00b_string_t *full     = n00b_cformat("[|#|].test", base);
    n00b_string_t *contents = n00b_read_file(full);
    n00b_string_t *icmd     = n00b_cstring("INJECT ");
    n00b_string_t *x1cmd    = n00b_cstring("EXPECT ");
    n00b_string_t *x2cmd    = n00b_cstring("ERR_EXPECT ");
    n00b_string_t *p1cmd    = n00b_cstring("PATTERN ");
    n00b_string_t *p2cmd    = n00b_cstring("ERR_PATTERN ");
    n00b_string_t *prmpt    = n00b_cstring("PROMPT");
    n00b_string_t *win      = n00b_cstring("SIZE ");
    n00b_string_t *ansi     = n00b_cstring("ANSI");
    n00b_string_t *merge    = n00b_cstring("MERGE");
    n00b_string_t *timeout  = n00b_cstring("TIMEOUT");
    n00b_string_t *tab      = n00b_cstring("\t");
    n00b_string_t *nl       = n00b_cached_newline();

    if (!contents) {
        n00b_eprintf(
            "[|red|]Error:[|/|] Couldn't load test file: "
            "[|em|][|#|]",
            full);
        return false;
    }

    contents           = n00b_string_replace(contents, tab, nl);
    n00b_list_t *lines = n00b_string_split(contents, nl);
    n00b_list_t *cmds  = n00b_list(n00b_type_ref());
    int          n     = n00b_list_len(lines);

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_list_get(lines, i, NULL);

        if (!s || !s->codepoints) {
            continue;
        }
        s = n00b_string_strip(s);
        if (!s || !s->codepoints) {
            continue;
        }

        if (n00b_string_starts_with(s, n00b_cached_hash())) {
            continue;
        }
        if (n00b_string_starts_with(s, icmd)) {
            add_inject(test, s, cmds);
            continue;
        }
        if (n00b_string_eq(s, ansi)) {
            add_command(N00B_TF_ANSI, cmds);
            continue;
        }
        if (n00b_string_eq(s, merge)) {
            add_command(N00B_TF_MERGE, cmds);
            continue;
        }
        if (n00b_string_starts_with(s, x1cmd)) {
            add_expect(test, s, cmds, false);
            continue;
        }
        if (n00b_string_starts_with(s, x2cmd)) {
            add_expect(test, s, cmds, true);
            continue;
        }
        if (n00b_string_starts_with(s, p1cmd)) {
            add_pattern(s, cmds, false);
            continue;
        }
        if (n00b_string_starts_with(s, p2cmd)) {
            add_pattern(s, cmds, true);
            continue;
        }
        if (n00b_string_eq(s, prmpt)) {
            add_prompt(cmds);
            continue;
        }
        if (n00b_string_starts_with(s, win)) {
            add_size(s, cmds);
            continue;
        }

        if (n00b_string_starts_with(s, timeout)) {
            add_timeout(s, test);
            continue;
        }

        n00b_eprintf(
            "[|red|]Error:[|/|] "
            "Invalid command in test file: [|em|][|#|]:[|#|]",
            full,
            (i + 1));
        return false;
    }

    test->commands      = cmds;
    test->replay_stream = NULL;

    return true;
}

n00b_test_t *
n00b_testgen_load_test_file(n00b_string_t *s, int id)
{
    n00b_test_t *result = n00b_gc_alloc_mapped(n00b_test_t,
                                               N00B_GC_SCAN_ALL);
    result->path        = n00b_resolve_path(s);
    result->id          = id;
    result->name        = n00b_path_get_file(result->path);

    n00b_list_t *parts = n00b_path_parts(n00b_resolve_path(s));
    n00b_list_pop(parts);

    n00b_string_t *dir = n00b_list_get(parts, 0, NULL);
    parts              = n00b_string_split(dir, n00b_cached_slash());
    result->group      = n00b_list_pop(parts);

    if (!n00b_parse_test_file(result)) {
        return NULL;
    }

    return result;
}

static inline n00b_string_t *
new_state(n00b_session_t *s, int64_t n)
{
    n00b_string_t *name = n00b_cformat("state #[|#|]", n);
    n00b_new(n00b_type_session_state(), s, name);

    return name;
}

static n00b_string_t *
testgen_inject_cb(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    n00b_session_inject(s, thunk);

    return t->next_state;
}

static n00b_string_t *
testgen_send_winch_cb(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    struct winsize *dims = thunk;
    int             fd;

    if (s->subprocess->subproc_stdout) {
        fd = n00b_stream_fileno(s->subprocess->subproc_stdout);
        ioctl(fd, TIOCSWINSZ, dims);
    }
    if (s->subprocess->subproc_stderr) {
        fd = n00b_stream_fileno(s->subprocess->subproc_stderr);
        ioctl(fd, TIOCSWINSZ, dims);
    }

    return t->next_state;
}

static n00b_string_t *
testgen_success_cb(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    n00b_test_t *test_case = thunk;

    test_case->pass = true;
    n00b_session_exit(s);

    return t->next_state;
}

static void
add_aux_error(n00b_test_t *tc, char *tag, n00b_string_t *buf, int hex)
{
    if (!tc->aux_error) {
        tc->aux_error = n00b_list(n00b_type_string());
    }

    if (!buf || !buf->codepoints) {
        return;
    }

    n00b_string_t *s = n00b_cstring(tag);

    n00b_list_append(tc->aux_error, n00b_cformat("«em4»Unmatched in «#»:", s));

    n00b_list_t *lines = n00b_string_split(buf, n00b_cached_newline());
    int          n     = n00b_list_len(lines);

    for (int i = 0; i < n; i++) {
        s = n00b_list_get(lines, i, NULL);
        n00b_list_append(tc->aux_error, n00b_string_escape(s));
    }

    if (!hex) {
        return;
    }

    n          = n00b_list_len(tc->fail_state_triggers);
    bool found = false;

    for (int i = 0; i < n; i++) {
        n00b_trigger_t *t = n00b_list_get(tc->fail_state_triggers, i, NULL);

        if (t->kind == N00B_TRIGGER_SUBSTRING) {
            // Only works w/ a single target.
            switch (t->target) {
            case N00B_CAPTURE_STDOUT:
                if (hex > 0) {
                    found = true;
                    s     = n00b_cformat("«em5»Trigger «#» expected hex:",
                                     (int64_t)i);
                    n00b_list_append(tc->aux_error, s);
                    s = t->match_info.substring;
                    s = n00b_hex_dump(s->data,
                                      s->u8_bytes,
                                      n00b_kw("width", (int64_t)80));
                    n00b_list_append(tc->aux_error, s);
                }
                continue;
            case N00B_CAPTURE_STDERR:
                if (hex < 0) {
                    found = true;
                    s     = n00b_cformat("«em5»Trigger «#» expected hex:",
                                     (int64_t)i);
                    n00b_list_append(tc->aux_error, s);
                    s = t->match_info.substring;
                    s = n00b_hex_dump(s->data,
                                      s->u8_bytes,
                                      n00b_kw("width", (int64_t)80));
                    n00b_list_append(tc->aux_error, s);
                }
                continue;
            default:
                continue;
            }
        }
    }

    if (found) {
        s = n00b_cformat("«em5»Actual contents as hex:");
        n00b_list_append(tc->aux_error, s);
        s = n00b_hex_dump(buf->data,
                          buf->u8_bytes,
                          n00b_kw("width", (int64_t)80));
        n00b_list_append(tc->aux_error, s);
    }
}

static n00b_string_t *
testgen_failure_cb(n00b_session_t *s, n00b_trigger_t *t, void *thunk)
{
    n00b_test_t *test_case         = thunk;
    test_case->fail_state          = s->cur_state_name;
    test_case->fail_state_triggers = (n00b_list_t *)s->cur_user_state;

    if (s->got_timeout) {
        test_case->got_timeout = true;
    }

    if (!test_case->state_repr) {
        test_case->state_repr = n00b_session_state_repr(s);
    }

    add_aux_error(test_case, "user input", s->input_match_buffer, 0);
    add_aux_error(test_case, "injection", s->injection_match_buffer, 0);
    add_aux_error(test_case, "output", s->stdout_match_buffer, 1);
    add_aux_error(test_case, "error", s->stderr_match_buffer, -1);

    test_case->pass = false;
    n00b_session_exit(s);

    return t->next_state;
}

static void
build_state_machine(n00b_session_t *s, n00b_test_t *test)
{
    int              sid       = 0;
    int              n         = n00b_list_len(test->commands);
    n00b_string_t   *cur_state = new_state(s, sid++);
    n00b_string_t   *next_state;
    n00b_trigger_t  *cur_trigger = NULL;
    n00b_test_cmd_t *info;

    n00b_session_set_start_state(s, cur_state);

    for (int i = 0; i < n; i++) {
        info       = n00b_list_get(test->commands, i, NULL);
        next_state = new_state(s, sid++);

        switch (info->cmd) {
        case N00B_TF_ANSI:
            n00b_session_enable_ansi_matching(s);
            continue;
        case N00B_TF_MERGE:
            s->merge_output = true;
            continue;
        case N00B_TF_INJECT:
            cur_trigger        = n00b_auto_trigger(s,
                                            cur_state,
                                            next_state);
            cur_trigger->fn    = testgen_inject_cb;
            cur_trigger->thunk = info->payload.text;
            break;
        case N00B_TF_EXPECT:
            cur_trigger = n00b_output_trigger(s,
                                              cur_state,
                                              info->payload.text,
                                              next_state);
            break;
        case N00B_TF_ERR_EXPECT:
            cur_trigger = n00b_error_trigger(s,
                                             cur_state,
                                             info->payload.text,
                                             next_state);
            break;
        case N00B_TF_PATTERN:
            cur_trigger = n00b_output_trigger(s,
                                              cur_state,
                                              info->payload.pattern,
                                              next_state);
            break;
        case N00B_TF_ERR_PATTERN:
            cur_trigger = n00b_error_trigger(s,
                                             cur_state,
                                             info->payload.pattern,
                                             next_state);
            break;
        case N00B_TF_PROMPT:

            cur_trigger = n00b_subproc_exit_trigger(s, cur_state, next_state);
            break;
        case N00B_TF_SIZE:
            cur_trigger        = n00b_auto_trigger(s,
                                            cur_state,
                                            next_state);
            cur_trigger->fn    = testgen_send_winch_cb;
            cur_trigger->thunk = &info->payload.dims;
            break;
        }
        cur_state = next_state;
    }

    cur_trigger        = n00b_auto_trigger(s, cur_state, testgen_success_cb);
    cur_trigger->thunk = test;

    cur_trigger        = n00b_subproc_timeout_trigger(s,
                                               NULL,
                                               testgen_failure_cb);
    cur_trigger->thunk = test;
}

static void
n00b_testgen_run_one_test(n00b_testing_ctx *ctx, n00b_test_t *test)
{
    n00b_session_t *s = n00b_new(n00b_type_session(),
                                 n00b_kw("pass_input",
                                         n00b_ka(false),
                                         "pass_output",
                                         n00b_ka(ctx->verbose),
                                         "merge_output",
                                         n00b_ka(test->merge_io)));

    build_state_machine(s, test);

    if (ctx->verbose) {
        n00b_print(n00b_session_state_repr(s));
    }

    uint64_t timeout;

    if (test->got_timeout) {
        timeout = test->timeout_sec;
    }
    else {
        if (ctx && ctx->default_timeout) {
            timeout = ctx->default_timeout;
        }
        else {
            timeout = N00B_TEST_TIMEOUT_SEC_DEFAULT;
        }
    }
    s->end_time      = n00b_new(n00b_type_duration(), n00b_kw("sec", timeout));
    s->max_event_gap = n00b_new(n00b_type_duration(), n00b_kw("sec", timeout));

    n00b_string_t *pwd      = n00b_get_current_directory();
    n00b_string_t *test_dir = ctx->test_root;

    if (test->group) {
        test_dir = n00b_path_simple_join(test_dir, test->group);
    }

    n00b_set_current_directory(test_dir);
    n00b_session_run(s);
    n00b_set_current_directory(pwd);
}

static void
n00b_testgen_load_one_group(n00b_testing_ctx *ctx,
                            n00b_string_t    *group_name)
{
    n00b_test_group_t *group = n00b_gc_alloc_mapped(n00b_test_group_t,
                                                    N00B_GC_SCAN_ALL);
    n00b_list_t       *test_fnames;

    group->name  = group_name;
    group->path  = n00b_path_simple_join(ctx->test_root, group_name);
    group->tests = n00b_list(n00b_type_ref());
    test_fnames  = n00b_list_directory(group->path,
                                      n00b_kw("directories",
                                              n00b_ka(false),
                                              "links",
                                              n00b_ka(false),
                                              "specials",
                                              n00b_ka(false),
                                              "dot_files",
                                              n00b_ka(false),
                                              "extension",
                                              n00b_cstring(".test")));

    int n = n00b_list_len(test_fnames);

    if (!n) {
        if (ctx->verbose) {
            n00b_eprintf(
                "[|yellow|]Warning:[|/|] Directory [|em|][|#|][|/|] "
                "contains no test files. Test files must use the "
                "file extension [|i|].test[|/|].",
                group->path);
        }
        return;
    }

    int bad_cases = 0;

    for (int i = 0; i < n; i++) {
        n00b_string_t *fname = n00b_list_get(test_fnames, i, NULL);
        fname                = n00b_path_remove_extension(fname);
        n00b_string_t *loc   = n00b_path_simple_join(group->path, fname);
        int            id    = i - bad_cases;
        n00b_test_t   *test  = n00b_testgen_load_test_file(loc, id);

        if (!test) {
            bad_cases++;
            continue;
        }

        n00b_list_append(group->tests, test);
    }

    if (n == bad_cases) {
        if (ctx->verbose) {
            n00b_eprintf(
                "[|yellow|]Warning:[|/|] Directory [|em|][|#|][|/|] "
                "contains no VALID test files.");
        }
        return;
    }

    n00b_list_append(ctx->groups, group);
    n -= bad_cases;
}

static bool
n00b_testgen_load_all_groups(n00b_testing_ctx *ctx)
{
    n00b_list_t *test_groups;

    if (!n00b_path_is_directory(ctx->test_root)) {
        if (ctx->supplied_tdir) {
            n00b_eprintf("Could not open test directory: [|em|][|#|][|/|].",
                         ctx->test_root);
        }
        else {
            n00b_eprintf(
                "Could not find a [|em|]tests[|/|] directory under "
                "$N00B_ROOT ([|em|][|#|][|/|]",
                n00b_n00b_root());
        }
        n00b_exit(-1);
    }

    test_groups = n00b_list_directory(ctx->test_root,
                                      n00b_kw("files",
                                              n00b_ka(false),
                                              "links",
                                              n00b_ka(false),
                                              "specials",
                                              n00b_ka(false),
                                              "dot_files",
                                              n00b_ka(false)));

    int n = n00b_list_len(test_groups);

    if (!n) {
        n00b_eprintf(
            "[|red|]Error: [|/|]"
            "Currently, tests must be structured inside directories "
            "that group test cases. All 'dot files' (files or directories "
            "starting with a period) get ignored.");
        return false;
    }

    for (int i = 0; i < n; i++) {
        n00b_string_t *item = n00b_list_get(test_groups, i, NULL);
        n00b_testgen_load_one_group(ctx, item);
    }

    return true;
}

static inline void
enable_all_group_tests(n00b_test_group_t *g)
{
    int m = n00b_list_len(g->tests);
    for (int i = 0; i < m; i++) {
        n00b_test_t *t = n00b_list_get(g->tests, i, NULL);
        t->enabled     = true;
    }

    g->num_enabled = m;
}

static inline void
enable_specific_group_test(n00b_test_group_t *g, n00b_string_t *tname)
{
    int n = n00b_list_len(g->tests);
    tname = n00b_path_remove_extension(tname);

    for (int i = 0; i < n; i++) {
        n00b_test_t *test = n00b_list_get(g->tests, i, NULL);
        if (n00b_string_eq(tname, test->name)) {
            if (!test->enabled) {
                test->enabled = true;
                g->num_enabled++;
            }
            return;
        }
    }

    n00b_eprintf(
        "[|red|]Error: [|/|] No test named [|em|][|#|][|/|] "
        "found in group [|em|][|#|][|/|]",
        tname,
        g->name);
}

static inline void
find_in_test_directory(n00b_testing_ctx *ctx,
                       n00b_string_t    *sub_root,
                       bool              default_group)
{
    n00b_string_t *group;
    n00b_string_t *test_name;
    int            ix, n;

    if (default_group) {
        test_name = sub_root;
        group     = n00b_cstring("default");
        goto start_looking;
    }

    ix = n00b_string_find(sub_root, n00b_cached_slash());

    if (ix == -1) {
        group     = sub_root;
        test_name = NULL;
    }
    else {
        group     = n00b_string_slice(sub_root, 0, ix);
        test_name = n00b_string_slice(sub_root, ix + 1, -1);
    }

start_looking:
    n = n00b_list_len(ctx->groups);

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g = n00b_list_get(ctx->groups, i, NULL);
        if (n00b_string_eq(g->name, group)) {
            if (!test_name || n00b_string_codepoint_len(test_name) == 0) {
                enable_all_group_tests(g);
            }
            else {
                enable_specific_group_test(g, test_name);
            }
            return;
        }
    }

    n00b_eprintf(
        "[|red|]Error:[|/|] Can't find a test group named [|em|][|#|][|/|]",
        group);

    return;
}

static inline void
find_test_by_file_path(n00b_testing_ctx *ctx, n00b_string_t *spec)
{
    spec = n00b_resolve_path(spec);
    if (n00b_string_starts_with(spec, ctx->test_root)) {
        int   n = ctx->test_root->u8_bytes;
        char *p = spec->data + n;
        if (*p == '/') {
            p++;
        }

        find_in_test_directory(ctx, n00b_cstring(p), false);
        return;
    }

    if (!ctx->orphans) {
        ctx->orphans = n00b_list(n00b_type_ref());
    }

    int numo = n00b_list_len(ctx->orphans);

    n00b_test_t *t = n00b_testgen_load_test_file(spec, numo);

    if (t) {
        n00b_list_append(ctx->orphans, t);
        t->enabled = true;
    }
}

static inline void
enable_individual_tests(n00b_testing_ctx *ctx, n00b_list_t *specs)
{
    int nspecs = n00b_list_len(specs);
    int ngrps  = n00b_list_len(ctx->groups);

    for (int i = 0; i < nspecs; i++) {
        n00b_string_t *spec = n00b_list_get(specs, i, NULL);

        int ix = n00b_string_rfind(spec, n00b_cached_slash());

        if (ix == -1) {
            find_in_test_directory(ctx, spec, true);
            continue;
        }

        n00b_string_t *gname = n00b_string_slice(spec, 0, ix);
        n00b_string_t *tname = n00b_string_slice(spec, ix + 1, -1);
        bool           found = false;

        for (int j = 0; j < ngrps; j++) {
            n00b_test_group_t *g = n00b_list_get(ctx->groups, j, NULL);
            if (n00b_string_eq(g->name, gname)) {
                enable_specific_group_test(g, tname);
                found = true;
                break;
            }
        }

        if (!found) {
            find_test_by_file_path(ctx, spec);
        }
    }
}

static inline void
enable_tests_per_spec(n00b_testing_ctx *ctx, n00b_list_t *args)
{
    int          nargs     = n00b_list_len(args);
    int          ngrps     = n00b_list_len(ctx->groups);
    n00b_list_t *non_group = n00b_list(n00b_type_string());

    for (int i = 0; i < nargs; i++) {
        n00b_string_t *spec  = n00b_list_get(args, i, NULL);
        bool           found = false;

        for (int j = 0; j < ngrps; j++) {
            n00b_test_group_t *g = n00b_list_get(ctx->groups, j, NULL);
            if (n00b_string_eq(g->name, spec)) {
                found = true;
                enable_all_group_tests(g);
                break;
            }
        }

        if (!found) {
            n00b_list_append(non_group, spec);
        }
    }

    enable_individual_tests(ctx, non_group);
}

static inline void
enable_all_tests(n00b_testing_ctx *ctx)
{
    int n = n00b_list_len(ctx->groups);

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g = n00b_list_get(ctx->groups, i, NULL);
        enable_all_group_tests(g);
    }
}

n00b_testing_ctx *
_n00b_testgen_setup(N00B_OPT_KARGS)
{
    n00b_string_t *test_root = NULL;
    bool           quiet     = false;
    bool           verbose   = false;
    uint64_t       timeout   = N00B_TEST_TIMEOUT_SEC_DEFAULT;
    n00b_list_t   *args      = NULL;

    n00b_kw_ptr("test_root", test_root);
    n00b_kw_ptr("args", args);
    n00b_kw_bool("quiet", quiet);
    n00b_kw_bool("verbose", verbose);
    n00b_kw_uint64("timeout", timeout);

    n00b_testing_ctx *result = n00b_gc_alloc_mapped(n00b_testing_ctx,
                                                    N00B_GC_SCAN_ALL);
    result->groups           = n00b_list(n00b_type_ref());
    result->default_timeout  = timeout;
    result->quiet            = quiet;
    result->verbose          = verbose;

    if (verbose && quiet) {
        N00B_CRAISE("Testgen setup cannot be both 'quiet' and 'verbose'.");
    }

    if (!test_root) {
        result->test_root = n00b_path_simple_join(n00b_n00b_root(),
                                                  n00b_cstring("tests"));
    }
    else {
        result->test_root     = test_root;
        result->supplied_tdir = true;
    }

    if (!n00b_testgen_load_all_groups(result)) {
        return NULL;
    }

    if (args && n00b_list_len(args)) {
        enable_tests_per_spec(result, args);
    }
    else {
        enable_all_tests(result);
    }

    int n = n00b_list_len(result->groups);

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g = n00b_list_get(result->groups, i, NULL);
        result->tests_in_test_dir += n00b_list_len(g->tests);
        result->enabled_in_test_dir += g->num_enabled;

        if (g->num_enabled != 0) {
            result->groups_in_use++;
        }
    }

    return result;
}

static inline void
calculate_tg_column_widths(n00b_testing_ctx *ctx, int n)
{
    int largest_group_sz   = 0;
    int largest_group_name = 0;
    int largest_file_name  = 0;

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g = n00b_list_get(ctx->groups, i, NULL);
        if (!g->num_enabled) {
            continue;
        }
        if (g->num_enabled > largest_group_sz) {
            largest_group_sz = g->num_enabled;
        }
        if (g->name->codepoints > largest_group_name) {
            largest_group_name = g->name->codepoints;
        }

        int m = n00b_list_len(g->tests);

        for (int j = 0; j < m; j++) {
            n00b_test_t *tc = n00b_list_get(g->tests, j, NULL);
            if (tc->name->codepoints > largest_file_name) {
                largest_file_name = tc->name->codepoints;
            }
        }
    }

    // Add 2 to each column for pad.
    ctx->c1_width = 3;
    while (largest_group_sz) {
        ctx->c1_width++;
        largest_group_sz /= 10;
    }

    ctx->c2_width = largest_group_name + 2;
    ctx->c3_width = largest_file_name + 2;
}

static inline n00b_table_t *
create_matrix(n00b_testing_ctx *ctx)
{
    n00b_tree_node_t *ci = n00b_new_layout();
    n00b_new_layout_cell(ci,
                         n00b_kw("min",
                                 6LL,
                                 "max",
                                 6LL,
                                 "preference",
                                 6LL));
    n00b_new_layout_cell(ci,
                         n00b_kw("min",
                                 (int64_t)ctx->c1_width,
                                 "max",
                                 (int64_t)ctx->c1_width,
                                 "preference",
                                 (int64_t)ctx->c1_width));
    n00b_new_layout_cell(ci,
                         n00b_kw("min",
                                 (int64_t)ctx->c2_width,
                                 "max",
                                 (int64_t)ctx->c2_width,
                                 "preference",
                                 (int64_t)ctx->c2_width));

    n00b_new_layout_cell(ci,
                         n00b_kw("min",
                                 (int64_t)ctx->c3_width,
                                 "max",
                                 (int64_t)ctx->c3_width,
                                 "preference",
                                 (int64_t)ctx->c3_width));

    n00b_new_layout_cell(ci, n00b_kw("flex", 1ULL));

    n00b_table_t *result = n00b_table("outstream",
                                      n00b_stderr(),
#ifndef N00B_TG_DEFAULT_TABLE
                                      "style",
                                      N00B_TABLE_SIMPLE,
#endif
                                      "columns",
                                      5ULL,
                                      "column_widths",
                                      ci);
#ifdef N00B_TG_DEFAULT_TABLE
    n00b_table_add_cell(result, n00b_cstring("Action"));
    n00b_table_add_cell(result, n00b_cstring("Number"));
    n00b_table_add_cell(result, n00b_cstring("Group"));
    n00b_table_add_cell(result, n00b_cstring("Test"));
    n00b_table_add_cell(result, n00b_cstring("Test Result"));
#else
    n00b_table_add_cell(result, n00b_crich("[|head|]Action"));
    n00b_table_add_cell(result, n00b_crich("[|head|]Number"));
    n00b_table_add_cell(result, n00b_crich("[|head|]Group"));
    n00b_table_add_cell(result, n00b_crich("[|head|]Test"));
    n00b_table_add_cell(result, n00b_crich("[|head|]Test Result"));
#endif

    return result;
}

int
n00b_testgen_run_tests(n00b_testing_ctx *ctx)
{
    int            n         = n00b_list_len(ctx->groups);
    bool           quiet     = ctx->quiet;
    bool           verbose   = ctx->verbose;
    int            result    = 0; // Total failed
    int            total_run = 0;
    n00b_string_t *tmp;
    n00b_list_t   *fails = n00b_list(n00b_type_ref());

    calculate_tg_column_widths(ctx, n);

    n00b_table_t *t = NULL;

    if (!quiet || verbose) {
        t = create_matrix(ctx);
    }

    for (int i = 0; i < n; i++) {
        n00b_test_group_t *g = n00b_list_get(ctx->groups, i, NULL);
        g->passed            = 0;

        if (!g->num_enabled) {
            continue;
        }
        int m = n00b_list_len(g->tests);

        if (!quiet) {
            tmp = n00b_cformat(
                "[|h4|]Entering group:[|h1|] [|#|] "
                "([|#|]/[|#|] enabled) [|h4|] ",
                g->name,
                g->num_enabled,
                m);

            n00b_table_add_cell(t, tmp, -1);
            n00b_table_end_row(t);
        }

        for (int64_t j = 0; j < m; j++) {
            n00b_string_t *testnum = n00b_to_string((void *)(j + 1));
            n00b_test_t   *tc      = n00b_list_get(g->tests, j, NULL);
            if (!tc->enabled) {
                if (!quiet) {
                    n00b_table_add_cell(t, n00b_crich("[|h6|]Skip"));
                    n00b_table_add_cell(t, testnum);
                    n00b_table_add_cell(t, g->name);
                    n00b_table_add_cell(t, tc->name);
                    n00b_table_empty_cell(t);
                    n00b_table_end_row(t);
                }
                continue;
            }
            tc->quiet = !verbose;
            tc->pass  = false; // Just incase we do multiple runs.

            if (!quiet) {
                n00b_table_add_cell(t, n00b_crich("[|h5|]Run"));
                n00b_table_add_cell(t, testnum);
                n00b_table_add_cell(t, g->name);
                n00b_table_add_cell(t, tc->name);
                n00b_table_empty_cell(t);
                n00b_table_end_row(t);
            }
            n00b_testgen_run_one_test(ctx, tc);

            if (!quiet) {
                fprintf(stderr, "\r");
                n00b_table_add_cell(t, n00b_crich("[|h6|]Done"));
                n00b_table_add_cell(t, testnum);
                n00b_table_add_cell(t, g->name);
                n00b_table_add_cell(t, tc->name);

                if (tc->pass) {
                    n00b_table_add_cell(t, n00b_crich("[|green|]PASSED  "));
                }
                else {
                    if (tc->got_timeout) {
                        n00b_table_add_cell(t, n00b_crich("[|red|]TIMEOUT"));
                    }
                    else {
                        n00b_table_add_cell(t, n00b_crich("[|red|]FAILED "));
                    }
                }

                n00b_table_end_row(t);
            }

            if (!tc->pass) {
                n00b_list_append(fails, tc);
            }

            g->passed += (int)tc->pass;
        }

        if (!quiet) {
            tmp = n00b_cformat(
                "[|h5|]Exiting group:  [|h1|][|#|] "
                "([|#|]/[|#|] passed)",
                g->name,
                g->passed,
                g->num_enabled);

            n00b_table_add_cell(t, tmp, -1);
            n00b_table_end_row(t);
        }

        total_run += g->num_enabled;
        result += g->num_enabled - g->passed;
    }

    if (!quiet) {
        n00b_table_add_cell(t, n00b_cformat("[|h4|] "), -1);
        n00b_table_end_row(t);
        n00b_table_add_cell(t, n00b_cformat("[|h4|]SUMMARY:"), -1);
        n00b_table_end_row(t);
        n00b_table_add_cell(t, n00b_crich("[|h1|]Passed:"), 3);
        tmp = n00b_cformat("[|h2|][|#|]/[|#|][|/|] run tests.",
                           total_run - result,
                           total_run);
        n00b_table_add_cell(t, tmp, 2);
        n00b_table_end_row(t);
    }

    n = n00b_list_len(fails);

    if (n) {
        if (!t) {
            t = create_matrix(ctx);
        }

        for (int64_t i = 0; i < n; i++) {
            n00b_test_t   *tc = n00b_list_pop(fails);
            n00b_string_t *tn = n00b_to_string((void *)(i + 1));
            n00b_table_add_cell(t, n00b_crich("[|h6|]Failure"));
            n00b_table_add_cell(t, tn);
            n00b_table_add_cell(t, tc->group);
            n00b_table_add_cell(t, tc->name, 2);
            n00b_table_end_row(t);

            if (tc->state_repr) {
                if (!quiet) {
                    n00b_table_add_cell(t, tc->state_repr, -1);
                    n00b_table_end_row(t);
                }
            }

            if (tc->aux_error) {
                if (quiet) {
                    n00b_eprintf("Test [|#|] ([|#|]/[|#|]) [|red|]FAILED.",
                                 tn,
                                 tc->name,
                                 tc->name);
                    n00b_eprintf(tc->aux_error);
                }
                else {
                    n00b_table_add_cell(t, tc->aux_error, -1);
                    n00b_table_end_row(t);
                }
            }
        }
    }

    if (t) {
        n00b_table_end(t);
    }

#if defined(N00B_DEBUG)
    if (ctx->debug) {
        n00b_debug_log_dump();
    }
#endif

    return result;
}
