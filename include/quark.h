// When using musl, we need to replace these.

#pragma once

#include "hatrack/hatrack_config.h"

extern _Bool __atomic_compare_exchange_16(__int128_t *address,
                                          __int128_t *expected_value,
                                          __int128_t  new_value);

extern __int128_t __atomic_load_16(__int128_t *address);
extern void       __atomic_store_16(__int128_t *address, __int128_t new_value);
extern __int128_t __atomic_fetch_or_16(__int128_t *address, __int128_t operand);
