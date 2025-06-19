#pragma once

#include <stdatomic.h>

#define atomic_read(x) \
    atomic_load_explicit(x, memory_order_seq_cst)

#define n00b_cas(target, expected, desired) \
    atomic_compare_exchange_strong(target, expected, desired)
