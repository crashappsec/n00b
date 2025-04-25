/*
 * Copyright © 2025 Crash Override, Inc.
 *
 *  Name:           bloom.c
 *  Description:    High performance, lock-free bloom filter
 *
 *  Author:         John Viega, john@crashoverride.com
 */

#pragma once
#include "hatrack_common.h"
#include "hatomic.h"

typedef struct {
    _Atomic(uint64_t) *bitfield;
    unsigned int       bit_length;
    unsigned int       hash_width;
    unsigned int       num_hashes;
    unsigned int       hash_bytes;
    double             false_rate;
} hatrack_bloom_t;

HATRACK_EXTERN void hatrack_bloom_init(hatrack_bloom_t *,
                                       double,
                                       unsigned int,
                                       int);
HATRACK_EXTERN void hatrack_bloom_add(hatrack_bloom_t *, hatrack_hash_t *);
HATRACK_EXTERN bool hatrack_bloom_contains(hatrack_bloom_t *, hatrack_hash_t *);
