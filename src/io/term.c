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
n00b_termcap_apply_raw_mode(struct termios *termcap)
{
    termcap->c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXANY);
    // termcap->c_oflag &= ~OPOST;
    termcap->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    termcap->c_cflag &= ~(CSIZE | PARENB);
    termcap->c_cc[VMIN]  = 0;
    termcap->c_cc[VTIME] = 0;
}

void
n00b_termcap_apply_app_defaults(struct termios *termcap)
{
    termcap->c_iflag     = ICRNL | INPCK;
    termcap->c_lflag     = 0;
    termcap->c_cflag     = 0;
    termcap->c_oflag     = ONLCR | OPOST;
    termcap->c_cc[VMIN]  = 1;
    termcap->c_cc[VTIME] = 0;
}

pthread_once_t termcap_save = PTHREAD_ONCE_INIT;

struct termios starting_termcap;
bool           termcap_stashed = false;
static void    stash_termcap(void);

static void
restore_termcap(void)
{
    pthread_once(&termcap_save, stash_termcap);
    n00b_termcap_set(&starting_termcap);
}

static void
stash_termcap(void)
{
    n00b_termcap_get(&starting_termcap);
    termcap_stashed = true;
    n00b_add_exit_handler(restore_termcap);
}

void
n00b_terminal_raw_mode(void)
{
    struct termios tc;
    pthread_once(&termcap_save, stash_termcap);

    n00b_termcap_get(&tc);
    n00b_termcap_apply_raw_mode(&tc);
    n00b_termcap_set(&tc);
}

void
n00b_terminal_app_setup(void)
{
    struct termios tc;
    n00b_unbuffer_stdin();
    n00b_unbuffer_stdout();

    pthread_once(&termcap_save, stash_termcap);

    n00b_termcap_get(&tc);
    n00b_termcap_apply_app_defaults(&tc);
    n00b_termcap_set(&tc);

    n00b_colorterm_enable(n00b_stdout(), 0, 0, true, NULL);
    n00b_colorterm_enable(n00b_stderr(), 0, 0, true, NULL);

    //    n00b_terminal_raw_mode();
}
