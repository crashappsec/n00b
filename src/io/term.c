#include "n00b.h"

void
n00b_terminal_dimensions(size_t *cols, size_t *rows)
{
    struct winsize dims = {
        0,
    };

    ioctl(0, TIOCGWINSZ, &dims);
    if (cols != NULL) {
        *cols = dims.ws_col;
    }

    if (rows != NULL) {
        *rows = dims.ws_row;
    }
}

void
n00b_termcap_set_raw_mode(struct termios *termcap)
{
    termcap->c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // termcap->c_oflag &= ~OPOST;
    termcap->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    termcap->c_cflag &= ~(CSIZE | PARENB);
    termcap->c_cc[VMIN]  = 0;
    termcap->c_cc[VTIME] = 0;
    tcsetattr(1, TCSAFLUSH, termcap);
}
