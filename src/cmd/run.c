#define N00B_USE_INTERNAL_API
#include "n00b.h"
#include "n00b/cmd.h"

void
n00b_compile_and_run(n00b_cmdline_ctx *ctx)
{
    n00b_compile(ctx, false);

    if (n00b_got_fatal_compiler_error(ctx->cctx)) {
        return;
    }

    if (!n00b_cmd_quiet(ctx)) {
        n00b_eprintf("«em1»Generating code.");
    }
    ctx->vm = n00b_vm_new(ctx->cctx);
    n00b_generate_code(ctx->cctx, ctx->vm);

    if (!n00b_cmd_quiet(ctx)) {
        n00b_eprintf("«em4»Beginning execution.");
    }
    n00b_vmthread_t *thread = n00b_vmthread_new(ctx->vm);
    n00b_vmthread_run(thread);
    if (!n00b_cmd_quiet(ctx)) {
        n00b_eprintf("«em4»Execution finished.");
    }
}
