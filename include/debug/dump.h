#pragma once

#ifdef N00B_DEBUG
extern void n00b_debug_log_dump(void);
#define N00B_DEBUG_DIR_ENV n00b_cstring("N00B_DEBUG_DIR")
#else
#define n00b_debug_log_dump()
#endif
