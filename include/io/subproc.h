#pragma once

#include "n00b.h"

extern void  n00b_subproc_init(n00b_subproc_t *, char *, char *[], bool);
extern bool  n00b_subproc_set_envp(n00b_subproc_t *, char *[]);
extern bool  n00b_subproc_pass_to_stdin(n00b_subproc_t *, char *, size_t, bool);
extern bool  n00b_subproc_set_passthrough(n00b_subproc_t *, unsigned char, bool);
extern bool  n00b_subproc_set_capture(n00b_subproc_t *, unsigned char, bool);
extern bool  n00b_subproc_set_io_callback(n00b_subproc_t *,
                                         unsigned char,
                                         n00b_sb_cb_t);
extern void  n00b_subproc_set_timeout(n00b_subproc_t *, struct timeval *);
extern void  n00b_subproc_clear_timeout(n00b_subproc_t *);
extern bool  n00b_subproc_use_pty(n00b_subproc_t *);
extern bool  n00b_subproc_set_startup_callback(n00b_subproc_t *,
                                              void (*)(void *));
extern int   n00b_subproc_get_pty_fd(n00b_subproc_t *);
extern void  n00b_subproc_start(n00b_subproc_t *);
extern bool  n00b_subproc_poll(n00b_subproc_t *);
extern void  n00b_subproc_run(n00b_subproc_t *);
extern void  n00b_subproc_close(n00b_subproc_t *);
extern pid_t n00b_subproc_get_pid(n00b_subproc_t *);
extern char *n00b_sp_result_capture(n00b_capture_result_t *, char *, size_t *);
extern char *n00b_subproc_get_capture(n00b_subproc_t *, char *, size_t *);
extern int   n00b_subproc_get_exit(n00b_subproc_t *, bool);
extern int   n00b_subproc_get_errno(n00b_subproc_t *, bool);
extern int   n00b_subproc_get_signal(n00b_subproc_t *, bool);
extern void  n00b_subproc_set_parent_termcap(n00b_subproc_t *, struct termios *);
extern void  n00b_subproc_set_child_termcap(n00b_subproc_t *, struct termios *);
extern void  n00b_subproc_set_extra(n00b_subproc_t *, void *);
extern void *n00b_subproc_get_extra(n00b_subproc_t *);
extern int   n00b_subproc_get_pty_fd(n00b_subproc_t *);
extern void  n00b_subproc_pause_passthrough(n00b_subproc_t *, unsigned char);
extern void  n00b_subproc_resume_passthrough(n00b_subproc_t *, unsigned char);
extern void  n00b_subproc_pause_capture(n00b_subproc_t *, unsigned char);
extern void  n00b_subproc_resume_capture(n00b_subproc_t *, unsigned char);
extern void  n00b_subproc_status_check(n00b_monitor_t *, bool);
