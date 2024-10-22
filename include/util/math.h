// Random math stuff
#include "n00b.h"

extern uint64_t n00b_clz(uint64_t);

static inline uint64_t
n00b_int_log2(uint64_t n)
{
    return 63 - __builtin_clzll(n);
}
