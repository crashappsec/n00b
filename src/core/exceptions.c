#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void (*uncaught_handler)(n00b_exception_t *) = n00b_exception_uncaught;
static pthread_once_t exceptions_inited             = PTHREAD_ONCE_INIT;

static void
exception_init(n00b_exception_t *exception, va_list args)
{
    n00b_string_t *message    = NULL;
    n00b_obj_t     context    = NULL;
    int64_t        error_code = -1;

    n00b_karg_va_init(args);
    n00b_kw_ptr("message", message);
    n00b_kw_ptr("context", context);
    n00b_kw_int64("error_code", error_code);

    exception->msg     = message;
    exception->context = context;
    exception->code    = error_code;
}

void
n00b_exception_gc_bits(uint64_t *bitmap, void *obj)
{
    n00b_exception_t *x = (n00b_exception_t *)obj;

    n00b_mark_raw_to_addr(bitmap, obj, &x->previous);
}

n00b_exception_t *
_n00b_alloc_exception(const char *msg, ...)
{
    n00b_exception_t *ret = n00b_gc_alloc_mapped(n00b_exception_t,
                                                 n00b_exception_gc_bits);
    ret->msg              = n00b_cstring((char *)msg);

    return ret;
}

n00b_exception_t *
_n00b_alloc_string_exception(n00b_string_t *msg, ...)
{
    n00b_exception_t *ret = n00b_gc_alloc_mapped(n00b_exception_t,
                                                 n00b_exception_gc_bits);
    ret->msg              = msg;

    return ret;
}

extern void n00b_restart_io(void);

void
n00b_print_exception(n00b_exception_t *exception, n00b_string_t *title)
{
    n00b_table_t *tbl = n00b_table("columns",
                                   n00b_ka(2),
                                   "title",
                                   title,
                                   "style",
                                   n00b_ka(N00B_TABLE_SIMPLE));

    n00b_table_next_column_fit(tbl);
    n00b_table_next_column_flex(tbl, 1);

    n00b_table_add_cell(tbl, exception->msg, N00B_COL_SPAN_ALL);
    n00b_table_add_cell(tbl, n00b_crich("«em2»Raised from:"));
    n00b_table_add_cell(tbl,
                        n00b_cformat("«#»:«#:i»",
                                     n00b_cstring((char *)exception->file),
                                     (int64_t)exception->line));
    n00b_eprint(tbl);

#if defined(N00B_DEBUG) && defined(N00B_BACKTRACE_SUPPORTED)
    if (!exception->c_trace) {
        n00b_eprintf("«em2»No C backtrace available.");
    }
    else {
        n00b_eprintf("«em2»C Backtrace:");
        n00b_eprint(exception->c_trace);
    }
#endif
}

void
n00b_default_uncaught_handler(n00b_exception_t *exception)
{
    n00b_print_exception(exception, n00b_crich("«em1»Fatal Exception"));
    n00b_abort();
}

void
n00b_exception_register_uncaught_handler(void (*handler)(n00b_exception_t *))
{
    uncaught_handler = handler;
}

static void
n00b_exception_thread_start(void)
{
    uncaught_handler = n00b_default_uncaught_handler;
}

n00b_exception_stack_t *
n00b_exception_push_frame(jmp_buf *jbuf)
{
    n00b_exception_frame_t *frame;
    n00b_tsi_t             *tsi = n00b_get_tsi_ptr();

    pthread_once(&exceptions_inited, n00b_exception_thread_start);

    if (tsi->exception_stack.free_frames) {
        frame                            = tsi->exception_stack.free_frames;
        tsi->exception_stack.free_frames = frame->next;
    }
    else {
        frame = calloc(1, sizeof(n00b_exception_frame_t));
    }
    frame->buf               = jbuf;
    frame->next              = tsi->exception_stack.top;
    tsi->exception_stack.top = frame;

    return &tsi->exception_stack;
}

void
n00b_exception_free_frame(n00b_exception_frame_t *frame,
                          n00b_exception_stack_t *stack)
{
    if (frame == stack->top) {
        stack->top = NULL;
    }
    memset(frame,
           0,
           sizeof(n00b_exception_frame_t) - sizeof(n00b_exception_frame_t *));
    frame->next        = stack->free_frames;
    stack->free_frames = frame;
}

n00b_string_t *
n00b_repr_exception_stack_no_vm(n00b_string_t *title)
{
    n00b_exception_frame_t *frame     = n00b_get_tsi_ptr()->exception_stack.top;
    n00b_exception_t       *exception = frame->exception;

    n00b_string_t *result;

    if (title == NULL) {
        title = n00b_cached_empty_string();
    }

    result = n00b_cformat("«red»«#»«/» «#»\n", title, exception->msg);

    while (frame != NULL) {
        exception           = frame->exception;
        n00b_string_t *frep = n00b_cformat(
            "«i»Raised from:«/» «em2»«#»:«#»«/»\n",
            n00b_cstring((char *)exception->file),
            (int64_t)exception->line);

        result = n00b_string_concat(result, frep);
        frame  = frame->next;
    }

    return result;
}

void
n00b_exception_uncaught(n00b_exception_t *exception)
{
    n00b_string_t *msg = n00b_repr_exception_stack_no_vm(
        n00b_cstring("FATAL ERROR:"));

    if (n00b_in_io_thread()) {
        fprintf(stderr, "%s\n", msg->data);
    }
    else {
        n00b_print(n00b_chan_stderr(),
                   msg,
                   NULL);
    }

    n00b_abort();
}

void
n00b_exception_raise(n00b_exception_t *exception,
                     n00b_table_t     *backtrace,
                     char             *filename,
                     int               line)
{
    n00b_exception_frame_t *frame = n00b_get_tsi_ptr()->exception_stack.top;

    exception->file    = filename;
    exception->line    = line;
    exception->c_trace = backtrace;

    n00b_thread_resume_locking();

    if (!frame) {
        (*uncaught_handler)(exception);
        n00b_abort();
    }

    frame->exception = exception;

    longjmp(*(frame->buf), 1);
}

const n00b_vtable_t n00b_exception_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)exception_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]   = NULL,

        NULL,
    },
};
