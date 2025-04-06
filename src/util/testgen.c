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

#define INJECT_HDR \
    n00b_cstring(  \
        "# Leave the following command uncommented to auto-inject the text:\n")
#define EXPECT_HDR                                                             \
    n00b_cstring(                                                              \
        "# Leave the following for an exact match of the core text below.\n"   \
        "# Change the 'verb' to ANSI if you want to match formatting codes.\n" \
        "# (Note: the codes will be stripped from these comments)\n#\n"        \
        "# You can change the verb to EXACT and enter a substring to match.\n" \
        "# You can also put PATTERN and give an unanchored regexp.\n"          \
        "# ERR_ in front of a verb matches the error stream; otherwise, it\n"  \
        "# will match the output stream.\n")
#define PROMPT_HDR \
    n00b_cstring("# The PROMPT command will match a top-level process exiting.\n")

static n00b_string_t *
to_comment(n00b_string_t *s)

{
    n00b_list_t *lines = n00b_string_split(s, n00b_cached_newline());
    int          n     = n00b_list_len(lines);

    for (int i = 0; i < n; i++) {
        n00b_string_t *s = n00b_private_list_get(lines, i, NULL);
        n00b_private_list_set(lines, i, n00b_cformat("# [|#|]", s));
    }

    return n00b_string_join(lines,
                            n00b_cached_newline(),
                            n00b_kw("add_trailing", n00b_ka(true)));
}

static inline void
gen_inject(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr)
{
    if (hdr) {
        n00b_write(output, INJECT_HDR);
    }
    else {
        n00b_write(output,
                   n00b_cstring("The next INJECT command will inject "
                                "the following text:\n"));
    }

    n00b_write(output, to_comment(event->contents));
    n00b_write(output, n00b_cformat("INJECT [|#|]\n", event->id));
}

static inline void
gen_expect(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr, bool err)
{
    n00b_string_t *cmd;

    if (err) {
        cmd = n00b_cformat("EXPECT [|#|]\n", event->id);
    }
    else {
        cmd = n00b_cformat("ERR_EXPECT [|#|]\n", event->id);
    }

    if (hdr) {
        n00b_write(output, EXPECT_HDR);
    }
    else {
        n00b_write(output,
                   n00b_cstring("The next command will exact-match all of "
                                "the following text:\n"));
    }

    n00b_list_t   *l = event->contents;
    n00b_string_t *s = n00b_ansi_nodes_to_string(l, false);

    n00b_write(output, to_comment(s));
    n00b_write(output, cmd);
}

static inline void
gen_prompt(n00b_cap_event_t *event, n00b_stream_t *output, bool hdr)
{
    if (hdr) {
        n00b_write(output, PROMPT_HDR);
    }

    n00b_write(output, n00b_cformat("PROMPT [|#|]\n", event->id));
}

static void
build_testgen_script(n00b_list_t *events, n00b_string_t *base_path)
{
    n00b_stream_t *script              = n00b_tempfile(NULL, NULL);
    n00b_string_t *tname               = n00b_stream_get_name(script);
    bool           gen_user_input      = false;
    bool           show_inject_comment = true;
    bool           show_expect_comment = true;
    bool           show_prompt_comment = true;

    int n = n00b_list_len(events);

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
            gen_expect(event, script, show_expect_comment, false);
            show_expect_comment = false;
            continue;
        case N00B_CAPTURE_STDERR:
            gen_expect(event, script, show_expect_comment, true);
            show_expect_comment = false;
            continue;
        case N00B_CAPTURE_CMD_RUN:
            gen_inject(event, script, show_inject_comment);
            gen_user_input      = true;
            show_inject_comment = false;
            continue;
        case N00B_CAPTURE_CMD_EXIT:
            gen_prompt(event, script, show_prompt_comment);
            gen_user_input      = false;
            show_prompt_comment = false;
            continue;
        case N00B_CAPTURE_END:
        case N00B_CAPTURE_SPAWN:
        case N00B_CAPTURE_WINCH:
            continue;
        default:
            n00b_unreachable();
        }
    }

    n00b_close(script);

    n00b_string_t *dst = n00b_cformat("[|#|].test", base_path);
    n00b_rename(tname, dst);

    n00b_eprintf("[|em2|]Draft script saved to: [|i|][|#|]", dst);
}

void
n00b_testgen_record(n00b_string_t *test_name)
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

    n00b_eprintf("[|em4|]Beginning test shell.");

    // Interactive shell that gets recorded.
    session = n00b_new(n00b_type_session(),
                       n00b_kw("capture_tmpfile", n00b_ka(true)));
    n00b_session_run(session);

    n00b_eprintf("[|em4|]Test shell exited.");
    events   = n00b_session_capture_extractor(session, N00B_CAPTURE_ALL);
    filename = n00b_session_capture_filename(session);
    n00b_close(session->unproxied_capture);
    path = n00b_rename(filename, path);

    n00b_eprintf("[|em2|]Capture saved to: [|i|][|#|]", path);
    build_testgen_script(events, base);
}
