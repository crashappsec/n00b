#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void (*uncaught_handler)(n00b_exception_t *) = n00b_exception_uncaught;

thread_local n00b_exception_stack_t __exception_stack = {
    0,
};

static pthread_once_t exceptions_inited = PTHREAD_ONCE_INIT;

static void
exception_init(n00b_exception_t *exception, va_list args)
{
    n00b_utf8_t *message    = NULL;
    n00b_obj_t   context    = NULL;
    int64_t      error_code = -1;

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
    ret->msg              = n00b_new(n00b_type_utf8(),
                        n00b_kw("cstring", n00b_ka(msg)));

    return ret;
}

n00b_exception_t *
_n00b_alloc_str_exception(n00b_utf8_t *msg, ...)
{
    n00b_exception_t *ret = n00b_gc_alloc_mapped(n00b_exception_t,
                                                 n00b_exception_gc_bits);
    ret->msg              = msg;

    return ret;
}

extern void n00b_restart_io(void);

void
n00b_default_uncaught_handler(n00b_exception_t *exception)
{
    n00b_list_t       *empty = n00b_list(n00b_type_utf8());
    n00b_renderable_t *hdr   = n00b_to_str_renderable(
        n00b_new_utf8("UNCAUGHT EXCEPTION"),
        n00b_new_utf8("h1"));
    n00b_renderable_t *row1 = n00b_to_str_renderable(exception->msg, NULL);
    n00b_list_t       *row2 = n00b_list(n00b_type_utf8());

    n00b_list_append(empty, n00b_new_utf8(""));
    n00b_list_append(empty, n00b_new_utf8(""));

    n00b_list_append(row2, n00b_rich_lit("[h2]Raised from:[/]"));
    n00b_list_append(row2,
                     n00b_cstr_format("{}:{}",
                                      n00b_new_utf8(exception->file),
                                      n00b_box_u64(exception->line)));

    n00b_grid_t *tbl = n00b_new(n00b_type_grid(),
                                n00b_kw("header_rows",
                                        n00b_ka(0),
                                        "start_rows",
                                        n00b_ka(4),
                                        "start_cols",
                                        n00b_ka(2),
                                        "container_tag",
                                        n00b_ka(n00b_new_utf8("flow")),
                                        "stripe",
                                        n00b_ka(true)));

    n00b_set_column_style(tbl, 0, n00b_new_utf8("snap"));
    // For the moment, we need to write an empty row then install the span over it.
    n00b_grid_add_row(tbl, empty);
    n00b_grid_add_col_span(tbl, hdr, 0, 0, 2);
    n00b_grid_add_row(tbl, empty);
    n00b_grid_add_row(tbl, empty);
    n00b_grid_add_col_span(tbl, row1, 1, 0, 2);
    n00b_grid_add_row(tbl, row2);

    fprintf(stderr, "%s\n", n00b_repr(tbl)->data);

#if defined(N00B_DEBUG) && defined(N00B_BACKTRACE_SUPPORTED)
    if (!exception->c_trace) {
        fprintf(stderr, "No C backtrace available.");
    }
    else {
        fprintf(stderr, "C backtrace:");
        fprintf(stderr, "%s\n", n00b_repr(exception->c_trace)->data);
    }
#endif
    n00b_thread_exit(0);
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

    pthread_once(&exceptions_inited, n00b_exception_thread_start);

    if (__exception_stack.free_frames) {
        frame                         = __exception_stack.free_frames;
        __exception_stack.free_frames = frame->next;
    }
    else {
        frame = calloc(1, sizeof(n00b_exception_frame_t));
    }
    frame->buf            = jbuf;
    frame->next           = __exception_stack.top;
    __exception_stack.top = frame;

    return &__exception_stack;
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

n00b_utf8_t *
n00b_repr_exception_stack_no_vm(n00b_utf8_t *title)
{
    n00b_exception_frame_t *frame     = __exception_stack.top;
    n00b_exception_t       *exception = frame->exception;

    n00b_utf8_t *result;

    if (title == NULL) {
        title = n00b_new_utf8("");
    }

    result = n00b_cstr_format("[red]{}[/] {}\n", title, exception->msg);

    while (frame != NULL) {
        exception         = frame->exception;
        n00b_utf8_t *frep = n00b_cstr_format("[i]Raised from:[/] [em]{}:{}[/]\n",
                                             n00b_new_utf8(exception->file),
                                             n00b_box_u64(exception->line));
        result            = n00b_str_concat(result, frep);
        frame             = frame->next;
    }

    return result;
}

void
n00b_exception_uncaught(n00b_exception_t *exception)
{
    n00b_utf8_t *msg = n00b_repr_exception_stack_no_vm(
        n00b_new_utf8("FATAL ERROR:"));

    if (n00b_in_io_thread()) {
        fprintf(stderr, "%s\n", msg->data);
    }
    else {
        n00b_print(n00b_stderr(),
                   msg,
                   NULL);
    }

    n00b_abort();
}

void
n00b_exception_raise(n00b_exception_t *exception,
                     n00b_grid_t      *backtrace,
                     char             *filename,
                     int               line)
{
    n00b_exception_frame_t *frame = __exception_stack.top;

    exception->file    = filename;
    exception->line    = line;
    exception->c_trace = backtrace;

    if (!frame) {
        (*uncaught_handler)(exception);
        n00b_abort();
    }

    frame->exception = exception;

    longjmp(*(frame->buf), 1);
}

const n00b_vtable_t n00b_exception_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)exception_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]   = NULL,

        NULL,
    },
};
