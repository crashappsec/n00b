#pragma once

#include "n00b.h"

extern void n00b_terminal_dimensions(size_t *cols, size_t *rows);
extern void n00b_termcap_set_raw_mode(struct termios *termcap);

static inline size_t
n00b_terminal_width()
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
    tcsetattr(0, TCSANOW, termcap);
}
