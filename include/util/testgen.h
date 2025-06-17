#pragma once
#include "n00b.h"

#ifdef N00B_USE_INTERNAL_API

#define N00B_TEST_TIMEOUT_SEC_DEFAULT 2
#endif

typedef enum {
    N00B_TF_MERGE,
    N00B_TF_ANSI,
    N00B_TF_INJECT,
    N00B_TF_EXPECT,
    N00B_TF_ERR_EXPECT,
    N00B_TF_PATTERN,
    N00B_TF_ERR_PATTERN,
    N00B_TF_PROMPT,
    N00B_TF_SIZE,
} n00b_test_file_cmd;

typedef struct {
    union {
        int            event_id;
        n00b_string_t *text;
        n00b_regex_t  *pattern;
        struct winsize dims;
    } payload;

    uint64_t           timeout_sec;
    n00b_test_file_cmd cmd;
    bool               event_ref;
    bool               got_timeout;
} n00b_test_cmd_t;

typedef struct {
    n00b_string_t *name;
    n00b_string_t *path;
    n00b_string_t *group;
    n00b_string_t *fail_state;
    n00b_list_t   *fail_state_triggers;
    int64_t        id;
    uint64_t       timeout_sec;
    bool           enabled;
    bool           run;
    bool           pass;
    bool           quiet;
    bool           verbose;
    bool           got_timeout;
    bool           use_ansi;
    bool           merge_io;
    int            next_eventid;
    n00b_list_t   *commands;
    n00b_stream_t *replay_stream;
    n00b_table_t  *state_repr;
    n00b_list_t   *aux_error;
} n00b_test_t;

typedef struct {
    n00b_string_t *name;
    n00b_string_t *path;
    n00b_list_t   *tests;
    int            num_enabled;
    int            passed;
} n00b_test_group_t;

typedef struct {
    n00b_list_t   *groups;
    n00b_list_t   *orphans; // Tests w/o groups, not in the test dir.
    n00b_string_t *test_root;
    uint64_t       default_timeout;
    bool           quiet;
    bool           verbose;
    bool           supplied_tdir;
    int            tests_in_test_dir;
    int            enabled_in_test_dir;
    int            groups_in_use;
    int            c1_width;
    int            c2_width;
    int            c3_width;
#if defined(N00B_DEBUG)
    bool debug;
#endif
} n00b_testing_ctx;

extern void              n00b_testgen_record(n00b_string_t *, bool, bool, bool);
extern n00b_test_t      *n00b_testgen_load_test_file(n00b_string_t *, int);
extern n00b_testing_ctx *n00b_testgen_setup(n00b_karg_info_t *);
extern int               n00b_testgen_run_tests(n00b_testing_ctx *);
