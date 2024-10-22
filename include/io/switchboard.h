#pragma once

#include "n00b.h"

extern ssize_t      n00b_sb_read_one(int, char *, size_t);
extern bool         n00b_sb_write_data(int, char *, size_t);
extern int          n00b_sb_party_fd(n00b_party_t *party);
extern void         n00b_sb_init_party_listener(n00b_switchboard_t *,
                                               n00b_party_t *,
                                               int,
                                               n00b_accept_cb_t,
                                               bool,
                                               bool);
extern n00b_party_t *n00b_sb_new_party_listener(n00b_switchboard_t *,
                                              int,
                                              n00b_accept_cb_t,
                                              bool,
                                              bool);
extern void         n00b_sb_init_party_fd(n00b_switchboard_t *,
                                         n00b_party_t *,
                                         int,
                                         int,
                                         bool,
                                         bool,
                                         bool);
extern n00b_party_t *n00b_sb_new_party_fd(n00b_switchboard_t *,
                                        int,
                                        int,
                                        bool,
                                        bool,
                                        bool);
extern void         n00b_sb_init_party_input_buf(n00b_switchboard_t *,
                                                n00b_party_t *,
                                                char *,
                                                size_t,
                                                bool,
                                                bool);
extern n00b_party_t *n00b_sb_new_party_input_buf(n00b_switchboard_t *,
                                               char *,
                                               size_t,
                                               bool,
                                               bool);
extern void         n00b_sb_party_input_buf_new_string(n00b_party_t *,
                                                      char *,
                                                      size_t,
                                                      bool,
                                                      bool);
extern void         n00b_sb_init_party_output_buf(n00b_switchboard_t *,
                                                 n00b_party_t *,
                                                 char *,
                                                 size_t);
extern n00b_party_t *n00b_sb_new_party_output_buf(n00b_switchboard_t *,
                                                char *,
                                                size_t);
extern void         n00b_sb_init_party_callback(n00b_switchboard_t *,
                                               n00b_party_t *,
                                               n00b_sb_cb_t);
extern n00b_party_t *n00b_sb_new_party_callback(n00b_switchboard_t *, n00b_sb_cb_t);
extern void         n00b_sb_monitor_pid(n00b_switchboard_t *,
                                       pid_t,
                                       n00b_party_t *,
                                       n00b_party_t *,
                                       n00b_party_t *,
                                       bool);
extern void        *n00b_sb_get_extra(n00b_switchboard_t *);
extern void         n00b_sb_set_extra(n00b_switchboard_t *, void *);
extern void        *n00b_sb_get_party_extra(n00b_party_t *);
extern void         n00b_sb_set_party_extra(n00b_party_t *, void *);
extern bool         n00b_sb_route(n00b_switchboard_t *,
                                 n00b_party_t *,
                                 n00b_party_t *);
extern bool         n00b_sb_pause_route(n00b_switchboard_t *,
                                       n00b_party_t *,
                                       n00b_party_t *);
extern bool         n00b_sb_resume_route(n00b_switchboard_t *,
                                        n00b_party_t *,
                                        n00b_party_t *);
extern bool         n00b_sb_route_is_active(n00b_switchboard_t *,
                                           n00b_party_t *,
                                           n00b_party_t *);
extern bool         n00b_sb_route_is_paused(n00b_switchboard_t *,
                                           n00b_party_t *,
                                           n00b_party_t *);
extern bool         n00b_sb_route_is_subscribed(n00b_switchboard_t *,
                                               n00b_party_t *,
                                               n00b_party_t *);
extern void         n00b_sb_init(n00b_switchboard_t *, size_t);
extern void         n00b_sb_set_io_timeout(n00b_switchboard_t *,
                                          struct timeval *);
extern void         n00b_sb_clear_io_timeout(n00b_switchboard_t *);
extern void         n00b_sb_destroy(n00b_switchboard_t *, bool);
extern bool         n00b_sb_operate_switchboard(n00b_switchboard_t *, bool);
extern void         n00b_sb_get_results(n00b_switchboard_t *,
                                       n00b_capture_result_t *);
extern char        *n00b_sb_result_get_capture(n00b_capture_result_t *,
                                              char *,
                                              bool);
extern void         n00b_sb_result_destroy(n00b_capture_result_t *);
extern n00b_party_t *n00b_new_party();
