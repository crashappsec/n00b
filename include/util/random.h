#pragma once

#include "n00b.h"

#if defined(__APPLE__) || defined(BSD)

static inline uint64_t
n00b_rand64()
{
    uint64_t res;

    arc4random_buf(&res, 8);

    return res;
}

static inline uint32_t
n00b_rand32()
{
    uint32_t res;

    arc4random_buf(&res, 4);

    return res;
}

static inline uint16_t
n00b_rand16()
{
    uint16_t res;

    arc4random_buf(&res, 2);

    return res;
}

#elif defined(__linux__)

static inline uint64_t
n00b_rand64()
{
    uint64_t res;

    while (getrandom(&res, 8, GRND_NONBLOCK) != 8)
        // retry on interrupt.
        ;

    return res;
}
static inline uint32_t
n00b_rand32()
{
    uint32_t res;

    while (getrandom(&res, 4, GRND_NONBLOCK) != 4)
        // retry on interrupt.
        ;

    return res;
}

static inline uint16_t
n00b_rand16()
{
    uint16_t res;

    while (getrandom(&res, 2, GRND_NONBLOCK) != 2)
        // retry on interrupt.
        ;

    return res;
}

#else
#error "Unsupported platform."
#endif
