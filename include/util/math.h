// Random math stuff
#include "n00b.h"

extern uint64_t n00b_clz(uint64_t);

static inline uint64_t
n00b_round_base2(uint64_t n)
{
    // n & (n - 1) only returns 0 when n is a power of two.
    // If n's already a power of 2, we're done.
    if (!(n & (n - 1))) {
        return n;
    }

    // CLZ returns the number of zeros before the leading one.
    // The next power of two will have one fewer leading zero,
    // and that will be the only bit set.
    return 0x8000000000000000ull >> (__builtin_clzll(n) - 1);
}

static inline uint64_t
n00b_int_log2(uint64_t n)
{
    return 63 - __builtin_clzll(n);
}
