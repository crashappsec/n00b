#pragma once
#include "n00b.h"

typedef enum {
    N00B_ANSI_TEXT,
    N00B_ANSI_C0_CODE,
    N00B_ANSI_C1_CODE,
    N00B_ANSI_NF_SEQUENCE,
    N00B_ANSI_FP_SEQUENCE,
    N00B_ANSI_FE_SEQUENCE,
    N00B_ANSI_FS_SEQUENCE,
    N00B_ANSI_CONTROL_SEQUENCE,
    N00B_ANSI_PRIVATE_CONTROL,
    N00B_ANSI_CTL_STR_CHAR,
    N00B_ANSI_CRL_STR_CMD,
    N00B_ANSI_PARTIAL,
    N00B_ANSI_INVALID,
} n00b_ansi_kind;

typedef struct {
    char   *pstart;
    char   *istart;
    int     plen;
    int     ilen;
    uint8_t ppi; // Only used when private
    uint8_t ctrl_byte;
} n00b_ansi_ctrl_t;

typedef struct {
    char            *start;
    char            *end;
    n00b_ansi_ctrl_t ctrl;
    n00b_ansi_kind   kind;
} n00b_ansi_node_t;

typedef struct {
    char        *cur;
    char        *end;
    n00b_list_t *results;
} n00b_ansi_ctx;

extern n00b_ansi_ctx *n00b_ansi_parser_create(void);
extern void           n00b_ansi_parse(n00b_ansi_ctx *, n00b_buf_t *);
extern n00b_list_t   *n00b_ansi_parser_results(n00b_ansi_ctx *);
extern n00b_string_t *n00b_ansi_node_repr(n00b_ansi_node_t *);
