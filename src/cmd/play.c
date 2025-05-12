#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_play_capture(n00b_cmdline_ctx *ctx)
{
    n00b_string_t  *arg    = n00b_resolve_path(n00b_list_get(ctx->args, 0, NULL));
    n00b_string_t  *ext    = n00b_path_get_extension(arg);
    n00b_channel_t *stream = NULL;

    if (ext == n00b_cached_empty_string()) {
        arg = n00b_cformat("[|#|].cap10", arg);
    }
    else {
        if (!n00b_string_eq(ext, n00b_cstring(".cap10"))) {
            n00b_eprintf("Capture file must have the [|em|].cap10[|/|] extension.");
            ctx->exit_code = -1;
            return;
        }
    }
    arg = n00b_resolve_path(arg);

    if (!n00b_path_exists(arg)) {
        n00b_eprintf("Capture file [|em|][|#|][|/|] not found.", arg);
        ctx->exit_code = -1;
        return;
    }
    if (!n00b_path_is_file(arg)) {
        n00b_eprintf("Capture file [|em|][|#|][|/|] is not a regular file.", arg);
        ctx->exit_code = -1;
        return;
    }

    bool fail = false;

    N00B_TRY
    {
        stream = n00b_channel_open_file(arg, "read_only", n00b_ka(true));
    }
    N00B_EXCEPT
    {
        n00b_exception_t *exc = N00B_X_CUR();
        n00b_eprintf("[|red|]Error:[|/|] [|#|]", exc->msg);
        fail = true;
    }
    N00B_TRY_END;

    if (fail) {
        ctx->exit_code = -1;
        return;
    }

    n00b_session_t *session = n00b_cinematic_replay_setup(stream);
    if (n00b_session_run(session)) {
        n00b_eprintf("«em»Replay complete.");
    }
    else {
        printf("\n");
        fflush(stdout);
        n00b_eprintf("«em2»Replay aborted.");
    }
    return;
}
