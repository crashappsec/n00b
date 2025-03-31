#pragma once
#include "n00b.h"

typedef enum {
    N00B_SESSION_CLOSED,
    N00B_USER_COMMAND_RUNNING,
    N00B_SHELL_RUNNING,
    N00B_SHELL_SUB_PROGRAM,
} n00b_session_state_t;

typedef struct n00b_session_t n00b_session_t;

typedef enum {
    N00B_CAPTURE_SYSTEM    = 0,
    N00B_CAPTURE_STDIN     = 1,
    N00B_CAPTURE_INJECTED  = 2,
    N00B_CAPTURE_STDOUT    = 4,
    N00B_CAPTURE_STDERR    = 8,
    N00B_CAPTURE_SPAWN     = 16,  // Session start.
    N00B_CAPTURE_END       = 32,  // Session end.
    N00B_CAPTURE_CMD_RUN   = 64,  // Subcommand in shell runs.
    N00B_CAPTURE_CMD_EXIT  = 128, // Subcommand exits.
    N00B_CAPTURE_AUX_READ  = 256,
    N00B_CAPTURE_AUX_WRITE = 512,
    N00B_CAPTURE_AUX_PAUSE = 1024,
} n00b_capture_t;

typedef struct {
    n00b_string_t *command;
    n00b_list_t   *args;
} n00b_cap_spawn_info_t;

// In the capture log, a single event.
typedef struct {
    n00b_duration_t *timestamp;
    void            *contents; // For stdout / stderr, list[n00b_ansi_node_t *]
    int              newlines;
    n00b_capture_t   kind;
} n00b_cap_event_t;

// The capture log, with the start time of the capture.
typedef struct {
    n00b_duration_t *base_timestamp;
    n00b_list_t     *events;
} n00b_capture_log_t;

// For replaying capture logs.
typedef struct {
    n00b_capture_log_t *log;
    n00b_duration_t    *time_offset;
    n00b_duration_t    *pause_time;
    int                 index;
    int                 replay_streams;
} n00b_log_cursor_t;

typedef enum {
    N00B_TRIGGER_PATTERN,
    N00B_TRIGGER_CONTROL_PATTERN,
    N00B_TRIGGER_TIMEOUT,
    N00B_TRIGGER_ANY_EXIT,
    N00B_TRIGGER_EXIT_CODE,
} n00b_trigger_kind;

typedef struct n00b_trigger_t n00b_trigger_t;
// Return the name of the new state to enter.
typedef n00b_string_t *(*n00b_trigger_fn)(n00b_session_t *,
                                          n00b_trigger_t *,
                                          void *);
struct n00b_trigger_t {
    n00b_capture_t    target;
    n00b_trigger_kind kind;
    void             *match_info;
    n00b_trigger_fn   fn;
};

// TODO:
// - Ensure that events are firmly ordered by when they came in, despite
//   processing time (grab the lock early).
//
// - Ensure triggers are well ordered too.

struct n00b_session_t {
    // Until we select a start time, 'end_time' holds the 'max_time'
    // field from the constructor. If we end or reset, we subtract
    // to get the 'max_time' back.
    n00b_duration_t         *start_time;
    n00b_duration_t         *last_event;
    n00b_duration_t         *end_time;            // If  abs max time is given.
    n00b_duration_t         *idle_timeout;        // If  idle timeout is given.
    n00b_duration_t         *max_gap;             // For replay
    n00b_stream_t           *subproc_ctrl_stream; // Local interface
    n00b_capture_log_t      *capture_log;
    n00b_string_t           *stdin_match_buffer;
    n00b_string_t           *stdout_match_buffer;
    n00b_string_t           *stderr_match_buffer;
    n00b_string_t           *start_state;
    n00b_string_t           *cur_user_state;
    n00b_string_t           *rc_filename;
    n00b_string_t           *pwd;
    n00b_dict_t             *user_states;
    _Atomic(n00b_thread_t *) control_thread;
    n00b_string_t           *launch_command;
    n00b_list_t             *launch_args;
    n00b_proc_t             *subprocess;
    n00b_log_cursor_t        replay_state;
    int64_t                  max_match_backscroll;
    float                    speedup;
    int                      capture_policy;
    uint32_t                 passthrough_output : 1; // Can be toggled.
    uint32_t                 passthrough_input  : 1; // Can be toggled.
    uint32_t                 using_pty          : 1;
    uint32_t                 merge_output       : 1;
    uint32_t                 likely_bash        : 1;
    uint32_t                 paused_clock       : 1;
    n00b_session_state_t     state;
};

#ifdef N00B_USE_INTERNAL_API
static inline bool
n00b_session_start_control(n00b_session_t *s)
{
    n00b_thread_t *self     = n00b_thread_self();
    n00b_thread_t *expected = NULL;

    if (!CAS(&s->control_thread, &expected, self)) {
        return (expected == self);
    }

    return true;
}

static inline bool
n00b_session_end_control(n00b_session_t *s)
{
    n00b_thread_t *self = n00b_thread_self();

    return CAS(&s->control_thread, &self, NULL);
}

static inline bool
n00b_session_is_active(n00b_session_t *s)
{
    return atomic_read(&s->control_thread) != NULL;
}

#define N00B_SESSION_BASH_SETUP                                             \
    "    if [[ -f ~/.bashrc ]] ; then . ~/.bashrc; fi\n"                    \
    "\n"                                                                    \
    "function __n00b_log() {\n"                                             \
    "    if [[ -z \"${N00B_BASH_INFO_LOG}\" ]] ; then\n"                    \
    "        echo $@\n"                                                     \
    "    else\n"                                                            \
    "        echo $@ >> $N00B_BASH_INFO_LOG\n"                              \
    "    fi\n"                                                              \
    "}\n"                                                                   \
    "function __n00b_run_bash_command() {\n"                                \
    "    if (( __n00b_in_run_command == 1 )) ; then return; fi\n"           \
    "    if (( __n00b_in_prompt == 1 )) ; then return; fi\n"                \
    "\n"                                                                    \
    "    if [[ ! -t 1 ]] ; then return ; fi\n"                              \
    "    if [[ -n \"${COMP_POINT:-}\" || -n \"${READLINE_POINT:-}\" ]] ;\n" \
    "    then return\n"                                                     \
    "fi\n"                                                                  \
    "\n"                                                                    \
    "    if [[ ${BASH_COMMAND} = \"__n00b_prompt\" ]] ; then return ; fi\n" \
    "\n"                                                                    \
    "    __n00b_in_run_command=1\n"                                         \
    "\n"                                                                    \
    "    __n00b_log n00b_r $BASH_COMMAND\n"                                 \
    "    __n00b_in_run_command=0\n"                                         \
    "}\n"                                                                   \
    "\n"                                                                    \
    "function __n00b_bash_prompt() {\n"                                     \
    "    __n00b_in_prompt=1\n"                                              \
    "    __n00b_log n00b_p\n"                                               \
    "    __n00b_in_prompt=0\n"                                              \
    "}\n"                                                                   \
    "\n"                                                                    \
    "trap __n00b_run_bash_command DEBUG\n"                                  \
    "PROMPT_COMMAND=__n00b_bash_prompt\n"

#define N00B_SESSION_SHELL_BASH "--rcfile=[|#|]"
#define N00B_SESION_SHELL_SH    "-i"
#endif
