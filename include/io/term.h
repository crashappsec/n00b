#pragma once

#include "n00b.h"

extern void n00b_terminal_dimensions(size_t *cols, size_t *rows);
extern void n00b_termcap_apply_raw_mode(struct termios *termcap);

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

extern void n00b_terminal_raw_mode(void);
extern void n00b_terminal_app_setup(void);
