#include "n00b.h"

extern void n00b_stream_debug_register(n00b_stream_t *);
extern void n00b_stream_debug_deregister(n00b_stream_t *);
extern void n00b_show_streams(void);
extern void n00b_log_streams(FILE *);
extern void n00b_log_streams_to_file(char *);
extern void n00b_stream_debug_enable(bool);
extern void n00b_stream_debug_disable(void);
