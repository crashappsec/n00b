#include "n00b.h"

// Right now, this is just holding our wrappers of stdin, stdout and
// stderr.  I may bundle together a 'terminal' abstraction that would
// cover ptys as well, not sure.

#define STDIN_RAW_IX  0
#define STDOUT_RAW_IX 1
#define STDERR_RAW_IX 2
#define STDIN_IX      3
#define STDOUT_IX     4
#define STDERR_IX     5
#define NUM_CHANS     6

static n00b_stream_t *n00b_stream_terminal_io[NUM_CHANS];

n00b_stream_t *
n00b_stdin(void)
{
    return n00b_stream_terminal_io[STDIN_RAW_IX];
}

n00b_stream_t *
n00b_stdin_buffered(void)
{
    return n00b_stream_terminal_io[STDIN_IX];
}

n00b_stream_t *
n00b_stdout(void)
{
    return n00b_stream_terminal_io[STDOUT_IX];
}

n00b_stream_t *
n00b_stderr(void)
{
    return n00b_stream_terminal_io[STDERR_IX];
}

n00b_stream_t *
n00b_stdout_raw(void)
{
    return n00b_stream_terminal_io[STDOUT_RAW_IX];
}

n00b_stream_t *
n00b_stderr_raw(void)
{
    return n00b_stream_terminal_io[STDERR_RAW_IX];
}

void
n00b_setup_terminal_streams(void)
{
    n00b_gc_register_root(n00b_stream_terminal_io, NUM_CHANS);
    n00b_fd_stream_t *instrm, *outstrm, *errstrm;
    n00b_stream_t    *stream;
    n00b_stream_t    *proxy;

    instrm  = n00b_fd_stream_from_fd(STDIN_RAW_IX, NULL, NULL);
    outstrm = n00b_fd_stream_from_fd(STDOUT_RAW_IX, NULL, NULL);
    errstrm = n00b_fd_stream_from_fd(STDERR_RAW_IX, NULL, NULL);

    stream = n00b_new_fd_stream(instrm);
    proxy  = n00b_new_stream_proxy(stream,
                                  n00b_filter_apply_line_buffering());

    stream->name = n00b_cstring("stdin");
    proxy->name  = n00b_cstring("stdin-line-buffered");

    n00b_stream_terminal_io[STDIN_RAW_IX] = stream;
    n00b_stream_terminal_io[STDIN_IX]     = proxy;

    stream = n00b_new_fd_stream(outstrm);
    proxy  = n00b_new_stream_proxy(stream,
                                  n00b_filter_apply_color(-1));

    n00b_stream_terminal_io[STDOUT_RAW_IX] = stream;
    n00b_stream_terminal_io[STDOUT_IX]     = proxy;

    proxy->name                            = n00b_cstring("stdout");
    stream                                 = n00b_new_fd_stream(errstrm);
    n00b_stream_terminal_io[STDERR_RAW_IX] = stream;
    proxy                                  = n00b_new_stream_proxy(stream,
                                  n00b_filter_apply_color(-1));
    proxy->name                            = n00b_cstring("stderr");
    n00b_stream_terminal_io[STDERR_IX]     = proxy;
}

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
    termcap->c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG | PENDIN);
    termcap->c_cflag &= ~(CSIZE | PARENB);
    termcap->c_cflag |= CREAD | HUPCL;
    termcap->c_iflag |= IUTF8 | IGNBRK;
}

void
n00b_termcap_apply_subshell_mode(struct termios *termcap)
{
    /*
    n00b_termcap_apply_raw_mode(termcap);
    termcap->c_lflag = IEXTEN | ISIG;
    termcap->c_cflag = HUPCL | CS8 | CREAD;
    termcap->c_oflag |= OPOST;
    termcap->c_iflag     = IUTF8 | IGNBRK;
    termcap->c_cc[VMIN]  = 1;
    termcap->c_cc[VTIME] = 0;
    */
}

void
n00b_termcap_apply_app_defaults(struct termios *termcap)
{
    n00b_termcap_apply_raw_mode(termcap);
    termcap->c_cflag |= CREAD | HUPCL | CS8;
    termcap->c_oflag |= OPOST;
    termcap->c_iflag = IUTF8;
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
    n00b_unbuffer_stderr();

    pthread_once(&termcap_save, stash_termcap);

    n00b_termcap_get(&tc);
    n00b_termcap_apply_app_defaults(&tc);
    n00b_termcap_set(&tc);
}
