#pragma once
#include "n00b.h"

typedef void (*n00b_signal_handler_t)(int, siginfo_t *, void *);

extern bool
n00b_signal_register(int, n00b_signal_handler_t, void *);

extern bool
n00b_signal_unregister(int, n00b_signal_handler_t, void *);

#ifdef N00B_USE_INTERNAL_API

extern pthread_once_t n00b_signals_inited;
extern void           n00b_setup_signals(void);
extern void           n00b_terminate_signal_handling(void);

static inline void
n00b_init_signals(void)
{
    pthread_once(&n00b_signals_inited, n00b_setup_signals);
}

#endif
