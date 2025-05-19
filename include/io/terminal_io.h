#include "n00b.h"

n00b_stream_t *n00b_stdin(void);
n00b_stream_t *n00b_stdin_buffered(void);
n00b_stream_t *n00b_stdout(void);
n00b_stream_t *n00b_stderr(void);
n00b_stream_t *n00b_stdout_raw(void);
n00b_stream_t *n00b_stderr_raw(void);

extern void n00b_terminal_dimensions(size_t *cols, size_t *rows);
extern void n00b_termcap_apply_raw_mode(struct termios *termcap);
extern void n00b_termcap_apply_app_defaults(struct termios *termcap);
extern void n00b_termcap_apply_subshell_mode(struct termios *termcap);

static inline size_t
n00b_terminal_width(void)
{
    size_t result;

    n00b_terminal_dimensions(&result, NULL);

    return result;
}

static inline void
n00b_termcap_get(struct termios *termcap)
{
    tcgetattr(0, termcap);
}

static inline void
n00b_termcap_set(struct termios *termcap)
{
    tcsetattr(0, TCSADRAIN, termcap);
}

static inline void
n00b_unbuffer_stdin(void)
{
    setbuf(stdin, NULL);
}

static inline void
n00b_unbuffer_stdout(void)
{
    setbuf(stdout, NULL);
}

static inline void
n00b_unbuffer_stderr(void)
{
    setbuf(stderr, NULL);
}

static inline int
n00b_calculate_render_width(int width)
{
    if (width <= 0) {
        width = n00b_terminal_width();
        if (!width) {
            width = N00B_MIN_RENDER_WIDTH;
        }
    }

    return width;
}

extern void n00b_terminal_raw_mode(void);
extern void n00b_terminal_app_setup(void);

#ifdef N00B_USE_INTERNAL_API
extern void n00b_setup_terminal_streams(void);
#endif
