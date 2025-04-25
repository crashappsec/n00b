#pragma once
#include "n00b.h"

typedef hatrack_bloom_t n00b_bloom_t;

// This should move to its own module, and allow for custom hash functions.

static inline void
n00b_bloom_add(hatrack_bloom_t *bf, void *obj)
{
    hatrack_hash_t hv = n00b_hash(obj);
    hatrack_bloom_add(bf, &hv);
}

static inline bool
n00b_bloom_contains(hatrack_bloom_t *bf, void *obj)
{
    hatrack_hash_t hv = n00b_hash(obj);
    return hatrack_bloom_contains(bf, &hv);
}
