#pragma once
#include "n00b.h"

extern void  N00B_DBG_DECL(n00b_stop_the_world);
extern void  N00B_DBG_DECL(n00b_restart_the_world);
extern void  N00B_DBG_DECL(n00b_thread_suspend);
extern void  N00B_DBG_DECL(n00b_thread_resume);
extern void  n00b_thread_checkin(void);
extern void  n00b_thread_start(void);
extern void *N00B_DBG_DECL(n00b_gil_alloc_len, int);
extern bool  n00b_world_is_stopped(void);

#define n00b_stop_the_world()    N00B_DBG_CALL(n00b_stop_the_world)
#define n00b_restart_the_world() N00B_DBG_CALL(n00b_restart_the_world)
#define n00b_thread_suspend()    N00B_DBG_CALL(n00b_thread_suspend)
#define n00b_thread_resume()     N00B_DBG_CALL(n00b_thread_resume)

#define n00b_gil_alloc_len(n) N00B_DBG_CALL(n00b_gil_alloc_len, n)
#define n00b_gil_alloc(t)     ((t *)n00b_gil_alloc_len(sizeof(t)))
