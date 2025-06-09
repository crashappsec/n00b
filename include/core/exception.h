#pragma once
#include "n00b.h"

// This exception handling mechanism is not meant to fully simulate a
// high-level language mechanism by itself.  Particularly, the EXCEPT
// macro exports an n00b_exception_t * object you get with the
// current_exception() macro, and you are excepted to switch on the
// object's type field (instead of having multiple EXCEPT blocks).
//
// You are responsible for either handling every possible exception
// that could be thrown (including system exceptions), or calling the
// RERAISE() macro.  So always make sure to have a default label.
//
// You MUST have an EXCEPT block,  a FINALLY block after the
// EXCEPT block and a TRY_END last (which we recommend adding
// a trailing semicolon to).
//
// If you don't do these things, you'll get compile errors.
//
// You shouldn't have any code after your switch statement in the
// EXCEPT block, and before the FINALLY or TRY_END.
//
// You may NOT use return, or any other control mechanism to escape
// any block, unless you use one of the two macros we provide:
//
// 1) JUMP_TO_FINALLY(), which must only be used in an EXCEPT block
// 2) JUMP_TO_TRY_END(), which can be used in a FINALLY block, but
//                       can only be used in an EXCEPT block if you
//                       have no FINALLY block.
//
// After your finally block runs(if any), you can do whatever you
// want.  So if you needed to, in concept, return a value from the
// middle of an except clause (for instance), then you can do
// something like:
//
// 1) Set a flag indicating you want to return after the finally code runs.
// 2) Use JUMP_TO_FINALLY()
// 3) After the TRY_END macro, test your flag and return whatever value when
//    appropriate.
//
//
// If the exception is NOT handled properly, you must either use
// RAISE() or RERAISE(), which will set the state to
// EXCEPTION_NOT_HANDLED, and then move to the next stage (either
// FINALLY or TRY_END, as appropriate), before unwinding the stack.
//
// Note that if you need two TRY blocks in one function you would end
// up with name collisions with the macros. We provide a second set of
// macros that allow you to label your exception handling code, to avoid
// this problem:
// 1) LFINALLY(label)
// 2) LTRY_END(label)
// 3) LJUMP_TO_FINALLY(label)
// 4) LJUMP_TO_TRY_END(label)
//
// Also, it is okay to add your own braces.
//
// TRY pushes a new exception frame onto the thread's exception
// stack.  It then does a setjmp and starts executing the block (if
// setjmp returns 0).  If longjmp is called, then the EXCEPT macro
// notices, and passes control to its block, so you can handle.
//
// The TRY_END call cleans up the exception stack and closes out
// hidden blocks created by the TRY and EXCEPT macros.
//
// You MUST NOT put the try/except block code in curly braces!
//

#define N00B_TRY                                                             \
    jmp_buf                    _n00bx_jmpbuf;                                \
    int                        _n00bx_setjmp_val;                            \
    n00b_exception_stack_t    *_n00bx_stack;                                 \
    n00b_exception_frame_t    *_n00bx_frame;                                 \
    volatile int               _n00bx_exception_state   = N00B_EXCEPTION_OK; \
    volatile n00b_exception_t *_n00bx_current_exception = NULL;              \
                                                                             \
    _n00bx_stack      = n00b_exception_push_frame(&_n00bx_jmpbuf);           \
    _n00bx_frame      = _n00bx_stack->top;                                   \
    _n00bx_setjmp_val = setjmp(_n00bx_jmpbuf);                               \
                                                                             \
    if (!_n00bx_setjmp_val) {
#define N00B_EXCEPT                                              \
    }                                                            \
    else                                                         \
    {                                                            \
        _n00bx_current_exception = _n00bx_stack->top->exception; \
        _n00bx_setjmp_val        = setjmp(_n00bx_jmpbuf);        \
        if (!_n00bx_setjmp_val) {
#define N00B_LFINALLY(user_label)                                             \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        _n00bx_exception_state            = N00B_EXCEPTION_IN_HANDLER;        \
        _n00bx_frame->exception->previous = (void *)_n00bx_current_exception; \
        _n00bx_current_exception          = _n00bx_stack->top->exception;     \
    }                                                                         \
    }                                                                         \
    _n00bx_finally_##user_label:                                              \
    {                                                                         \
        if (!setjmp(_n00bx_jmpbuf)) {
// The goto here avoids strict checking for unused labels.
#define N00B_LTRY_END(user_label)                                             \
    goto _n00bx_try_end_##user_label;                                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        _n00bx_exception_state            = N00B_EXCEPTION_IN_HANDLER;        \
        _n00bx_frame->exception->previous = (void *)_n00bx_current_exception; \
        _n00bx_current_exception          = _n00bx_stack->top->exception;     \
    }                                                                         \
    }                                                                         \
    _n00bx_try_end_##user_label : _n00bx_stack->top = _n00bx_frame->next;     \
    if (_n00bx_exception_state == N00B_EXCEPTION_IN_HANDLER                   \
        || _n00bx_exception_state == N00B_EXCEPTION_NOT_HANDLED) {            \
        if (!_n00bx_frame->next) {                                            \
            n00b_exception_uncaught((void *)_n00bx_current_exception);        \
        }                                                                     \
        n00b_exception_free_frame(_n00bx_frame, _n00bx_stack);                \
        _n00bx_stack->top->exception = (void *)_n00bx_current_exception;      \
        _n00bx_frame                 = _n00bx_stack->top;                     \
        _n00bx_frame->exception      = (void *)_n00bx_current_exception;      \
        longjmp(*(_n00bx_frame->buf), 1);                                     \
    }                                                                         \
    n00b_exception_free_frame(_n00bx_frame, _n00bx_stack);

#define N00B_LJUMP_TO_FINALLY(user_label) goto _n00bx_finally_##user_label
#define N00B_LJUMP_TO_TRY_END(user_label) goto _n00bx_try_end_##user_label

#define N00B_JUMP_TO_FINALLY() N00B_LJUMP_TO_FINALLY(default_label)
#define N00B_JUMP_TO_TRY_END() N00B_LJUMP_TO_TRY_END(default_label)
#define N00B_FINALLY           N00B_LFINALLY(default_label)
#define N00B_TRY_END           N00B_LTRY_END(default_label)

#if defined(N00B_DEBUG) && defined(N00B_BACKTRACE_SUPPORTED)
extern n00b_table_t *n00b_get_c_backtrace(int);

#define n00b_trace() n00b_get_c_backtrace(1)
#else
#define n00b_trace() NULL
#endif

#define N00B_CRAISE(s, ...)                                      \
    fprintf(stderr, "Exception: %s\n", s),                       \
        n00b_exception_raise(                                    \
            n00b_alloc_exception(s, __VA_OPT__(, ) __VA_ARGS__), \
            n00b_trace(),                                        \
            __FILE__,                                            \
            __LINE__)

#define N00B_RAISE(s, ...)                                         \
    n00b_exception_raise(                                          \
        n00b_alloc_string_exception(s __VA_OPT__(, ) __VA_ARGS__), \
        n00b_trace(),                                              \
        __FILE__,                                                  \
        __LINE__)

#define N00B_RERAISE()                                   \
    _n00bx_exception_state = N00B_EXCEPTION_NOT_HANDLED; \
    longjmp(*(_n00bx_current_frame->buf), 1)

#define N00B_X_CUR() ((void *)_n00bx_current_exception)

n00b_exception_t *_n00b_alloc_exception(const char *s, ...);
#define n00b_alloc_exception(s, ...) _n00b_alloc_exception(s, N00B_VA(__VA_ARGS__))

n00b_exception_t *_n00b_alloc_string_exception(n00b_string_t *s, ...);
#define n00b_alloc_string_exception(s, ...) \
    _n00b_alloc_string_exception(s, N00B_VA(__VA_ARGS__))

enum : int64_t {
    N00B_EXCEPTION_OK,
    N00B_EXCEPTION_IN_HANDLER,
    N00B_EXCEPTION_NOT_HANDLED
};

n00b_exception_stack_t *n00b_exception_push_frame(jmp_buf *);
void                    n00b_exception_free_frame(n00b_exception_frame_t *,
                                                  n00b_exception_stack_t *);
void                    n00b_exception_uncaught(n00b_exception_t *);
void                    n00b_exception_raise(n00b_exception_t *,
                                             n00b_table_t *,
                                             char *,
                                             int) __attribute((__noreturn__));
n00b_string_t          *n00b_repr_exception_stack_no_vm(n00b_string_t *);

static inline n00b_string_t *
n00b_exception_get_file(n00b_exception_t *exception)
{
    return n00b_new(n00b_type_string(),
                    n00b_kw("cstring", n00b_ka(exception->file)));
}

static inline uint64_t
n00b_exception_get_line(n00b_exception_t *exception)
{
    return exception->line;
}

static inline n00b_string_t *
n00b_exception_get_message(n00b_exception_t *exception)
{
    return exception->msg;
}

void n00b_exception_register_uncaught_handler(void (*)(n00b_exception_t *));

#ifdef _GNU_SOURCE
#define N00B_TURN_ON_GS
#undef _GNU_SOURCE
#endif

static inline n00b_string_t *
get_errno_message(int code)
{
    char buf[BUFSIZ];
    if (strerror_r(code, buf, BUFSIZ) != 0) {
        return n00b_cstring("Unknown error");
    }

    return n00b_cstring(buf);
}

#ifdef N00B_TURN_ON_GS
#define _GNU_SOURCE
#endif

#define n00b_raise_errcode(code) N00B_RAISE(get_errno_message((code)))

#define n00b_raise_errno()                                 \
    {                                                      \
        N00B_RAISE(get_errno_message(errno),               \
                   n00b_kw("error_code", n00b_ka(errno))); \
    }

#define n00b_unreachable()                                    \
    {                                                         \
        n00b_string_t *s = n00b_cformat(                      \
            "Reached code that the developer "                \
            "(wrongly) believed was unreachable, at «#»:«#»", \
            n00b_cstring(__FILE__),                           \
            (int64_t)__LINE__);                               \
        N00B_RAISE(s);                                        \
    }

extern void n00b_default_uncaught_handler(n00b_exception_t *exception);
extern void n00b_print_exception(n00b_exception_t *, n00b_string_t *);
