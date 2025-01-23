#pragma once

extern void          n00b_exit(int);
extern void          n00b_thread_exit(void *);
extern void          n00b_abort(void);
extern __thread bool n00b_thread_has_exited;
