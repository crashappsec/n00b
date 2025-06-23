#pragma once
#include "n00b.h"

typedef enum {
    N00B_SESSION_CLOSED,
    N00B_USER_COMMAND_RUNNING,
    N00B_SHELL_RUNNING,
    N00B_SHELL_SUB_PROGRAM,
} n00b_session_state_type;

typedef struct n00b_session_t n00b_session_t;

typedef enum : int16_t {
    N00B_CAPTURE_ANY      = 0,
    N00B_CAPTURE_STDIN    = 1, // Typed by the user
    N00B_CAPTURE_INJECTED = 2, // Written to the tty
    N00B_CAPTURE_STDOUT   = 4,
    N00B_CAPTURE_STDERR   = 8,
    // These next two aren't matchable, just for replay.
    N00B_CAPTURE_SPAWN    = 16,   // Session start.
    N00B_CAPTURE_END      = 32,   // Session end.
    N00B_CAPTURE_CMD_RUN  = 64,   // Subcommand in shell runs.
    N00B_CAPTURE_CMD_EXIT = 128,  // Subcommand exits.
    N00B_CAPTURE_WINCH    = 256,  // Window size change.
    N00B_CAPTURE_TIMEOUT  = 512,  // Local timeout
    N00B_CAPTURE_MAX_TIME = 1024, // Max time exceeded.
    N00B_CAPTURE_ALL      = ~0,
} n00b_capture_t;

typedef struct {
    n00b_string_t *command;
    n00b_list_t   *args;
} n00b_cap_spawn_info_t;

// In the capture log, a single event.
typedef struct {
    uint32_t         id;
    n00b_duration_t *timestamp;
    void            *contents; // For stdout / stderr, list[n00b_ansi_node_t *]
    n00b_capture_t   kind;
} n00b_cap_event_t;

// For replaying capture logs.
typedef struct {
    n00b_stream_t    *log;
    n00b_stream_t    *stdin_dst;
    n00b_stream_t    *stdout_dst;
    n00b_stream_t    *stderr_dst;
    n00b_duration_t  *cursor;
    n00b_duration_t  *absolute_start;
    n00b_duration_t  *max_gap;
    n00b_cap_event_t *cache;
    // Use this when not pausing before inputs to even out typing
    // pauses, esp by setting min_gap == max_gap.
    n00b_duration_t  *min_input_gap;
    n00b_condition_t  unpause_notify;
    double            time_scale;
    int               streams;
    bool              cinematic;
    bool              paused;
    bool              finished;
    bool              pause_before_input;
    bool              got_unpause;
    bool              play_input_to_newline;
    bool              remove_autopause;
} n00b_log_cursor_t;

typedef enum {
    N00B_TRIGGER_SUBSTRING,
    N00B_TRIGGER_PATTERN,
    N00B_TRIGGER_TIMEOUT,
    N00B_TRIGGER_ANY_EXIT,
    N00B_TRIGGER_ALWAYS,
    N00B_TRIGGER_RESIZE,
    // N00B_TRIGGER_EXIT_CODE,    Not implemented yet.
} n00b_trigger_kind;

typedef struct n00b_trigger_t n00b_trigger_t;
// Return the name of the new state to enter.
typedef n00b_string_t *(*n00b_trigger_fn)(n00b_session_t *,
                                          n00b_trigger_t *,
                                          void *);

struct n00b_trigger_t {
    n00b_trigger_kind kind;
    n00b_capture_t    target; // Only used for SUBSTRING or PATTERN
    union {
        n00b_string_t *substring;
        n00b_regex_t  *regexp;
        // int64_t        exit_code;   Not implemented yet.
    } match_info;
    // Here, `quiet` means, when we select this trigger, if it was due
    // to the user typing, we do NOT send it to the PTY.
    bool            quiet;
    n00b_trigger_fn fn;
    void           *cur_match;
    void           *thunk;
    n00b_string_t  *next_state;
};

typedef n00b_list_t n00b_session_state_t;

struct n00b_session_t {
    // Until we select a start time, 'end_time' holds the 'max_time'
    // field from the constructor. If we end or reset, we subtract
    // to get the 'max_time' back.
    n00b_duration_t         *start_time;
    n00b_duration_t         *skew;                // Time lost to pausing.
    n00b_duration_t         *last_event;
    n00b_duration_t         *end_time;            // If  abs max time is given.
    n00b_duration_t         *max_event_gap;
    n00b_stream_t           *subproc_ctrl_stream; // Local interface
    n00b_stream_t           *stdin_injection;
    n00b_list_t             *stdout_cb;
    n00b_list_t             *stderr_cb;
    n00b_string_t           *input_match_buffer;
    n00b_string_t           *injection_match_buffer;
    n00b_string_t           *stdout_match_buffer;
    n00b_string_t           *stderr_match_buffer;
    n00b_string_t           *start_state;
    n00b_session_state_t    *cur_user_state;
    n00b_string_t           *cur_state_name;
    n00b_string_t           *rc_filename;
    n00b_string_t           *pwd;
    n00b_dict_t             *user_states;
    n00b_session_state_t    *global_actions;
    _Atomic(n00b_stream_t *) capture_stream;
    n00b_stream_t           *saved_capture; // For pausing.
    n00b_stream_t           *unproxied_capture;
    n00b_string_t           *cap_filename;
    n00b_string_t           *launch_command;
    n00b_list_t             *launch_args;
    n00b_proc_t             *subprocess;
    n00b_condition_t         control_notify;
    n00b_log_cursor_t        log_cursor; // replay state
    int                      capture_policy;
    uint32_t                 next_capture_id;
    uint32_t                 passthrough_output : 1; // Can be toggled.
    uint32_t                 passthrough_input  : 1; // Can be toggled.
    uint32_t                 using_pty          : 1;
    uint32_t                 merge_output       : 1;
    uint32_t                 likely_bash        : 1;
    uint32_t                 paused_clock       : 1;
    uint32_t                 got_user_input     : 1;
    uint32_t                 got_injection      : 1;
    uint32_t                 got_stdout         : 1;
    uint32_t                 got_stderr         : 1;
    uint32_t                 got_prompt         : 1;
    uint32_t                 got_run            : 1;
    uint32_t                 got_timeout        : 1;
    uint32_t                 got_winch          : 1;
    uint32_t                 early_exit         : 1;
    uint32_t                 match_ansi         : 1;
    n00b_session_state_type  state;
};

static inline bool
n00b_session_is_active(n00b_session_t *s)
{
    return s->start_time != NULL;
}

extern void            n00b_setup_capture(n00b_session_t *,
                                          n00b_stream_t *,
                                          int);
extern bool            n00b_session_run(n00b_session_t *);
extern void            n00b_session_pause_recording(n00b_session_t *);
extern void            n00b_session_continue_recording(n00b_session_t *);
extern void            n00b_enable_replay(n00b_session_t *, bool);
extern void            n00b_enable_command_replay(n00b_session_t *, bool);
extern n00b_session_t *n00b_cinematic_replay_setup(n00b_stream_t *);
extern bool            n00b_session_run_cinematic(n00b_session_t *);
extern void            n00b_session_set_replay_stream(n00b_session_t *,
                                                      n00b_stream_t *);
extern n00b_table_t   *n00b_session_state_repr(n00b_session_t *);
extern void            n00b_session_start_replay(n00b_session_t *);

#ifdef N00B_USE_INTERNAL_API

#define N00B_SESSION_MAGIC 0x5355104f5355104fLL

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
    "    if (( __n00b_started == 0 )) ; then \n"                            \
    "        __n00b_started=1\n"                                            \
    "        return \n"                                                     \
    "    fi\n"                                                              \
    "    if (( __n00b_in_run_command == 1 )) ; then return; fi\n"           \
    "    if (( __n00b_in_prompt == 1 )) ; then return; fi\n"                \
    "\n"                                                                    \
    "    if [[ ! -t 1 ]] ; then return ; fi\n"                              \
    "    if [[ -n \"${COMP_POINT:-}\" || -n \"${READLINE_POINT:-}\" ]] ;\n" \
    "    then return\n"                                                     \
    "fi\n"                                                                  \
    "\n"                                                                    \
    "    if [[ ${BASH_COMMAND} = \"__n00b_bash_prompt\" ]] ; then \n"       \
    "        return \n"                                                     \
    "    fi\n"                                                              \
    "\n"                                                                    \
    "    __n00b_in_run_command=1\n"                                         \
    "\n"                                                                    \
    "    __n00b_log r $BASH_COMMAND\n"                                      \
    "    __n00b_log q\n"                                                    \
    "    __n00b_in_run_command=0\n"                                         \
    "}\n"                                                                   \
    "\n"                                                                    \
    "function __n00b_bash_prompt() {\n"                                     \
    "    __n00b_in_prompt=1\n"                                              \
    "    __n00b_log p\n"                                                    \
    "    __n00b_in_prompt=0\n"                                              \
    "}\n"                                                                   \
    "\n"                                                                    \
    "__n00b_started=0\n"                                                    \
    "trap __n00b_run_bash_command DEBUG\n"                                  \
    "PROMPT_COMMAND=__n00b_bash_prompt\n"

#define N00B_SESSION_SHELL_BASH "--rcfile"

extern void n00b_session_setup_control_stream(n00b_session_t *);

extern void                  n00b_session_setup_state_handling(n00b_session_t *);
extern void                  n00b_session_run_control_loop(n00b_session_t *);
extern void                 *n00b_session_run_replay_loop(n00b_session_t *);
extern void                  n00b_session_capture(n00b_session_t *, n00b_capture_t, void *);
extern void                  n00b_process_control_buffer(n00b_session_t *, n00b_string_t *);
extern void                  n00b_capture_launch(n00b_session_t *,
                                                 n00b_string_t *,
                                                 n00b_list_t *);
extern void                  n00b_session_finish_capture(n00b_session_t *);
extern void                  n00b_session_setup_user_read_cb(n00b_session_t *s);
extern void                  n00b_truncate_all_match_data(n00b_session_t *,
                                                          n00b_string_t *,
                                                          n00b_capture_t);
extern void                  n00b_session_scan_for_matches(n00b_session_t *);
extern n00b_session_state_t *n00b_session_find_state(n00b_session_t *,
                                                     n00b_string_t *);
#endif
