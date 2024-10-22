#pragma once

#include "n00b.h"

#define N00B_IO_HEAP_SZ (256)
// #define N00B_ALLOC_LEN  (PIPE_BUF + sizeof(struct n00b_sb_msg_t))
#define N00B_SB_MSG_LEN PIPE_BUF

typedef enum {
    N00B_PT_STRING   = 1,
    N00B_PT_FD       = 2,
    N00B_PT_LISTENER = 4,
    N00B_PT_CALLBACK = 8
} n00b_party_enum;

typedef void (*n00b_sb_cb_t)(void *, void *, char *, size_t);
typedef void (*n00b_accept_decl)(void *, int fd, struct sockaddr *, socklen_t *);
typedef bool (*n00b_progress_decl)(void *);

/* We queue these messages up for parties registered for writing, but
 * only if the sink is a file descriptor; callbacks and strings will
 * have the write processed immediately when the reader generates it.
 *
 * Note that we alloc these messages in some bulk; the n00b_switchboard_t
 * context below handles the memory management.
 *
 * In most systems when no reader is particularly slow relative to
 * others, there may never need to be more than one malloc call.
 */
typedef struct n00b_sb_msg_t {
    struct n00b_sb_msg_t *next;
    size_t               len;
    char                 data[N00B_SB_MSG_LEN + 1];
} n00b_sb_msg_t;

/*
 * This is the heap data type; the switchboard mallocs one heap at a
 * time, and hands out n00b_sb_msg_t's from it. The switchboard also keeps
 * a list of returned cells, and prefers returned cells over giving
 * out unused cells from the heap.
 *
 * If there's nothing left to give out in the heap or in the free
 * list, then we create a new heap (keeping the old one linked).
 *
 * When we get rid of our switchboard, we free any heaps, and can
 * ignore individual n00b_sb_msg_t objects.
 */
typedef struct n00b_sb_heap_t {
    struct n00b_sb_heap_t *next;
    size_t                cur_cell;
    uint32_t              dummy; // force alignment portably.
    n00b_sb_msg_t          cells[];
} n00b_sb_heap_t;

/*
 * For file descriptors that we might read from, where we might proxy
 * the data to some other file descriptor, we keep a linked list of
 * subscriber information.
 *
 * Only parties implemented as FDs are allowed to have
 * subscribers. Strings are the other source for input, but those are
 * 'published' immediately when the string is connected to the output.
 */
typedef struct n00b_subscription_t {
    struct n00b_subscription_t *next;
    struct n00b_party_t        *subscriber;
    bool                       paused;
} n00b_subscription_t;

/*
 * This abstraction is used for any party that's a file descriptor.
 * If the file descriptor is read-only, the first_msg and last_msg
 * fields will be unused.
 *
 * If the FD is write-only, then subscribers will not be used.
 */
typedef struct {
    n00b_sb_msg_t       *first_msg;
    n00b_sb_msg_t       *last_msg;
    n00b_subscription_t *subscribers;
    int                 fd;
    bool                proxy_close; // close fd when proxy input is closed
} n00b_party_fd_t;

/*
 * This is used for listening sockets.
 */
typedef struct {
    n00b_accept_decl accept_cb;
    int             fd;
    int             saved_flags;
} n00b_party_listener_t;

/*
 * For strings being piped into a process, pipe or whatever.
 */
typedef struct {
    char  *strbuf;
    bool   free_on_close;      // Did we take ownership of the str?
    size_t len;                // Total length of strbuf.
    bool   close_fd_when_done; // Close the fd after writing?
} n00b_party_instr_t;

/*
 * For buffer output into a string that's fully returned at the end.
 * If you want incremental output, use a callback.
 */
typedef struct {
    char  *strbuf;
    char  *tag;  // Used when returning.
    size_t len;  // Length allocated for strbuf
    size_t ix;   // Current length; next write at strbuf + ix
    size_t step; // Step for alloc length
} n00b_party_outstr_t;

/*
 * For incremental output! If you need to save state, you can do it by
 * assigning to either the swichboard_t 'extra' field or the n00b_party_t
 * 'extra' field; these are there for you to be able to keep state.
 */
typedef struct {
    n00b_sb_cb_t callback;
} n00b_party_callback_t;

/*
 * The union for the five party types above.
 */
typedef union {
    n00b_party_instr_t    rstrinfo;     // Strings used as an input source only
    n00b_party_outstr_t   wstrinfo;     // Strings used as an output sink only
    n00b_party_fd_t       fdinfo;       // Can be source, sink or both.
    n00b_party_listener_t listenerinfo; // We only read from it to kick off accept cb
    n00b_party_callback_t cbinfo;       // Sink only.
} n00b_party_info_t;

/*
 * The common abstraction for parties.
 * - `erno` will hold the value of any captured (fatal) os error we
 *    ran accross. This is only used for N00B_PT_FD and N00B_PT_LISTENER.
 * - `open` tracks whether we should deal with this party at all anymore;
 *   it can mean the fd is closed, or that nothing is routed to it anymore.
 * - `can_read_from_it` and `can_write_to_it` indicates whether a party is
 *   a source (the former) or a sink (the later). Can be both, too.
 * - `close_on_destroy` indicates that we should call close() on any file
 *   descriptors when tearing down the switchboard.
 *   When this is set, we do not report errors in close(), and we
 *   assume the same fd won't have been reused if it was otherwise
 *   closed during the switchboard operation.
 *   This only gets used for objs of type `N00B_PT_FD` and `N00B_PT_LISTENER`
 * - `stop_on_close` indicates that, when we notice a failure to
 *   read/write from a file descriptor, we should stop the switchboard;
 *   we do go ahead and finish available reads/writes, but we do nothing
 *   else.
 *   This should generally be the behavior you want when stdin, stdout
 *   or stderr go away (the controlling terminal is probably gone).
 *   However, it's not the right behavior for when a sub-process dies;
 *   There, you want to drain the read-size of the file descriptor.
 *   For that, you register the sub-process with `sb_monitor_pid()`.
 * - `next_reader`, `next_writer` and `next_loner` are three linked lists;
 *   a party might appear on up to two at once. `next_reader` and
 *   `next_writer` are only used for fd types. The first list can have
 *   both `N00B_PT_FD`s and `N00B_PT_LISTENER`s; the second only `N00B_PT_FD`s.
 *   The switchboard runs down these to figure out what to select() on.
 *   And, then when exiting, these lists are walked to free `n00b_party_t`
 *   objects.
 *   `next_loner` is for all other types, and is only used at the end to
 *   free stuff.
 * - `extra` is user-defined, ideal for state keeping in callbacks.
 */
typedef struct n00b_party_t {
    n00b_party_info_t    info;
    struct n00b_party_t *next_reader;
    struct n00b_party_t *next_writer;
    struct n00b_party_t *next_loner;
    void               *extra;
    n00b_party_enum      n00b_party_type;
    int                 found_errno;
    bool                open_for_write;
    bool                open_for_read;
    bool                can_read_from_it;
    bool                can_write_to_it;
    bool                close_on_destroy;
    bool                stop_on_close;
} n00b_party_t;

/*
 * When some of the i/o consists of other processes, we check on the
 * status of each process after every select call. This both keeps
 * state we need to monitor those processes, and anything we might
 * return about the process when returning switchboard results.
 */
typedef struct n00b_monitor_t {
    struct n00b_monitor_t *next;
    n00b_party_t          *stdin_fd_party;
    n00b_party_t          *stdout_fd_party;
    n00b_party_t          *stderr_fd_party;
    int                   exit_status;
    pid_t                 pid;
    bool                  shutdown_when_closed;
    bool                  closed;
    int                   found_errno;
    int                   term_signal;
} n00b_monitor_t;

typedef struct {
    char *tag;
    char *contents;
    int   len;
} n00b_one_capture_t;

typedef struct {
    n00b_one_capture_t *captures;
    bool               inited;
    int                num_captures;
} n00b_capture_result_t;

/*
 * The main switchboard object. Generally, the fields here can be
 * transparent to the user; everything should be dealt with via API.
 */
typedef struct n00b_switchboard_t {
    n00b_party_t      *parties_for_reading;
    n00b_party_t      *parties_for_writing;
    n00b_party_t      *party_loners;
    n00b_monitor_t    *pid_watch_list;
    n00b_sb_msg_t     *freelist;
    n00b_sb_heap_t    *heap;
    void             *extra;
    struct timeval   *io_timeout_ptr;
    struct timeval    io_timeout;
    n00b_progress_decl progress_callback;
    bool              progress_on_timeout_only;
    bool              done;
    fd_set            readset;
    fd_set            writeset;
    int               max_fd;
    int               fds_ready;
    size_t            heap_elems;
    bool              ignore_running_procs_on_shutdown;
} n00b_switchboard_t;

typedef struct {
    void (*startup_callback)(void *);
    char                *cmd;
    char               **argv;
    char               **envp;
    char                *path;
    n00b_switchboard_t    sb;
    bool                 run;
    struct termios      *child_termcap;
    struct n00b_dcb_t    *deferred_cbs;
    struct termios      *parent_termcap;
    n00b_capture_result_t result;
    struct termios       saved_termcap;
    int                  signal_fd;
    int                  pty_fd;
    bool                 pty_stdin_pipe;
    bool                 proxy_stdin_close;
    bool                 use_pty;
    bool                 str_waiting;
    char                 passthrough;
    bool                 pt_all_to_stdout;
    char                 capture;
    bool                 combine_captures; // Combine stdout / err and termout
    n00b_party_t          str_stdin;
    n00b_party_t          parent_stdin;
    n00b_party_t          parent_stdout;
    n00b_party_t          parent_stderr;
    n00b_party_t          subproc_stdin;
    n00b_party_t          subproc_stdout;
    n00b_party_t          subproc_stderr;
    n00b_party_t          capture_stdin;
    n00b_party_t          capture_stdout;
    n00b_party_t          capture_stderr;

} n00b_subproc_t;

#define N00B_SP_IO_STDIN  1
#define N00B_SP_IO_STDOUT 2
#define N00B_SP_IO_STDERR 4
#define N00B_SP_IO_ALL    7
#define N00B_CAP_ALLOC    16 // In # of PIPE_BUF sized chunks

// These are the real signatures.
typedef void (*n00b_accept_cb_t)(struct n00b_switchboard_t *,
                                int fd,
                                struct sockaddr *,
                                socklen_t *);
typedef bool (*n00b_progress_cb_t)(struct n00b_switchboard_t *);

typedef struct n00b_dcb_t {
    struct n00b_dcb_t *next;
    n00b_party_t      *to_free;
    unsigned char     which;
    n00b_sb_cb_t       cb;
} n00b_deferred_cb_t;
