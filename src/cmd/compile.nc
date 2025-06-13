#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_compile(n00b_cmdline_ctx *ctx, bool save_image)
{
    n00b_string_t *s = n00b_list_get(ctx->args, 0, NULL);

    if (!n00b_cmd_quiet(ctx)) {
        n00b_eprintf("Parsing module «em1»«#»«/» and its dependencies", s);
    }

    ctx->cctx = n00b_compile_from_entry_point(s);

    if (n00b_got_fatal_compiler_error(ctx->cctx)) {
        ctx->exit_code = -1;
    }
}
