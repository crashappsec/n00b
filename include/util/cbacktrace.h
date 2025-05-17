#pragma once

#ifdef N00B_BACKTRACE_SUPPORTED
extern void n00b_backtrace_init(char *);
extern void n00b_print_c_backtrace();
#else
#define n00b_backtrace_init(x)
#define n00b_print_c_backtrace()
#endif

extern n00b_table_t *n00b_get_c_backtrace(int);
extern void          n00b_static_c_backtrace(void);
extern void          n00b_set_crash_callback(void (*)(void));
extern void          n00b_set_show_trace_on_crash(bool);
extern n00b_string_t  *n00b_backtrace_utf8(void);
