#pragma once
#include "n00b.h"

typedef void (*n00b_signal_handler_t)(int, siginfo_t *, void *);

extern bool
n00b_signal_register(int, n00b_signal_handler_t, void *);

extern bool
n00b_signal_unregister(int, n00b_signal_handler_t, void *);

#ifdef N00B_USE_INTERNAL_API

extern /*once*/ void  n00b_init_signals(void);
extern void           n00b_terminate_signal_handling(void);
extern n00b_string_t *n00b_get_signal_name(int64_t);
#endif
