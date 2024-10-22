#pragma once
#include "n00b.h"

#ifdef N00B_DEBUG

// Basic polling watchpoint support. May eventually flesh out more;
// may put it in the VM loop, and poll after any instructions that do
// memory writes.  For now, it's just a prototype to help me hunt down
// a Heisenbug that is afraid of debuggers.

typedef enum : int8_t {
    n00b_wa_ignore,
    n00b_wa_log,
    n00b_wa_print,
    n00b_wa_abort,
} n00b_watch_action;

typedef struct {
    void            *address;
    uint64_t         value;
    n00b_watch_action read_action;
    n00b_watch_action write_action;
    int              last_write;
} n00b_watch_target;

typedef struct {
    char    *file;
    int      line;
    uint64_t start_value;
    uint64_t seen_value;
    int64_t  logid;
    bool     changed;
} n00b_watch_log_t;

extern void _n00b_watch_set(void            *addr,
                           int              ix,
                           n00b_watch_action ra,
                           n00b_watch_action wa,
                           bool             print,
                           char            *file,
                           int              line);

extern void _n00b_watch_scan(char *file, int line);

#define n00b_watch_set(p, ix, ra, wa, print) \
    _n00b_watch_set(p, ix, ra, wa, print, __FILE__, __LINE__)

#define n00b_watch_scan() _n00b_watch_scan(__FILE__, __LINE__)

#endif
