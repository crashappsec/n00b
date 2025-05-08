#include "n00b.h"

// Right now, this is just holding our wrappers of stdin, stdout and
// stderr.  I may bundle together a 'terminal' abstraction that would
// cover ptys as well, not sure.

#define STDIN_IX  0
#define STDOUT_IX 1
#define STDERR_IX 2
#define NUM_CHANS 3
static n00b_channel_t *n00b_chan_term_io[NUM_CHANS];

n00b_channel_t *
n00b_chan_stdin(void)
{
    return n00b_chan_term_io[STDIN_IX];
}

n00b_channel_t *
n00b_chan_stdout(void)
{
    return n00b_chan_term_io[STDOUT_IX];
}

n00b_channel_t *
n00b_chan_stderr(void)
{
    return n00b_chan_term_io[STDERR_IX];
}

void
n00b_setup_term_channels(void)
{
    n00b_gc_register_root(n00b_chan_term_io, NUM_CHANS);
    n00b_fd_stream_t *instrm, *outstrm, *errstrm;

    instrm  = n00b_fd_stream_from_fd(STDIN_IX, NULL, NULL);
    outstrm = n00b_fd_stream_from_fd(STDOUT_IX, NULL, NULL);
    errstrm = n00b_fd_stream_from_fd(STDERR_IX, NULL, NULL);

    n00b_chan_term_io[STDIN_IX]  = n00b_new_fd_channel(instrm);
    n00b_chan_term_io[STDOUT_IX] = n00b_new_fd_channel(outstrm);
    n00b_chan_term_io[STDERR_IX] = n00b_new_fd_channel(errstrm);
}
