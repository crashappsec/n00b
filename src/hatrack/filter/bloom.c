/*
 * Copyright © 2025 Crash Override, Inc.
 *
 *  Name:           bloom.c
 *  Description:    High performance, lock-free bloom filter
 *
 *  Author:         John Viega, john@crashoverride.com
 */

#include "hatrack/bloom.h"
#include "hatrack/hash.h"
#include "hatrack/dict.h"
#include "hatrack/hatomic.h"
#include <math.h>

#define HATRACK_BLOOM_HASHSIZE (sizeof(hatrack_hash_t) * 8)
#define HATRACK_LOG2           0.6931471805599453
#define HATRACK_LOG2_SQ        0.4804530139182014

void
hatrack_bloom_init(hatrack_bloom_t *bf,
                   double           false_pct,
                   unsigned int     expected_items,
                   int              num_hashes)
{
    double  nfplog = -log(false_pct);
    double  optlen = expected_items * nfplog / HATRACK_LOG2_SQ;
    int64_t bitlen = (int64_t)(optlen + .5);

    if (num_hashes <= 0) {
        double opthash = nfplog / HATRACK_LOG2;
        opthash += .5;
        num_hashes = (int)opthash;
    }

    // We round up to a multiple of 8 bits to make sure the array
    // divides cleanly into bytes.
    bf->bit_length       = hatrack_round_up_to_given_power_of_2(bitlen, 8);
    bf->hash_width       = hatrack_int_log2(bf->bit_length);
    bf->num_hashes       = num_hashes;
    int aligned_bytes_ph = hatrack_round_up_to_power_of_2(bf->hash_width) >> 3;
    bf->hash_bytes       = aligned_bytes_ph * num_hashes;
    bf->bitfield         = hatrack_malloc(bf->bit_length / 8);
}

static inline char *
bloom_get_stream(hatrack_bloom_t *bf, hatrack_hash_t *hv, char *arr)
{
    XXH128_hash_t expanded;

    if (bf->hash_bytes <= sizeof(hatrack_hash_t)) {
        return (char *)hv;
    }

    char        *p      = arr;
    uint64_t    *hv64   = (uint64_t *)hv;
    uint64_t     ctr[4] = {0, hv64[0], hv64[1], 0};
    unsigned int len    = bf->hash_bytes;

    while (len >= sizeof(hatrack_hash_t)) {
        expanded = XXH3_128bits(&ctr[0], sizeof(ctr));
        memcpy(p, &expanded, sizeof(hatrack_hash_t));
        p += sizeof(hatrack_hash_t);
        len -= sizeof(hatrack_hash_t);
        ctr[0]++;
        ctr[3]++;
    }
    if (len) {
        expanded = XXH3_128bits(&ctr[0], sizeof(ctr));
        memcpy(p, &expanded, len);
    }

    return arr;
}

static inline uint64_t
bloom_hash_value(char *stream, unsigned int width)
{
    uint64_t result = 0;

    assert(width <= sizeof(int64_t));

    for (unsigned int i = 0; i < width; i++) {
        result <<= 8;
        result |= (uint64_t)(*(uint8_t *)stream++);
    }

    return result;
}

void
hatrack_bloom_add(hatrack_bloom_t *bf, hatrack_hash_t *hv)
{
    unsigned int width = bf->hash_width;
    char         arr[bf->hash_bytes];
    char        *hash_stream = bloom_get_stream(bf, hv, arr);
    uint64_t     one_hash;

    for (unsigned int i = 0; i < bf->num_hashes; i++) {
        one_hash = bloom_hash_value(hash_stream, width);
        hash_stream += width;

        uint64_t           word_number = one_hash >> 6;
        uint64_t           bit_index   = one_hash & 63;
        uint64_t           one_bit     = 1 << bit_index;
        _Atomic(uint64_t) *p           = bf->bitfield + word_number;
        uint64_t           expected    = atomic_read(p);
        uint64_t           desired;

        do {
            desired = expected | one_bit;
        } while (!CAS(p, &expected, desired));
    }
}

bool
hatrack_bloom_contains(hatrack_bloom_t *bf, hatrack_hash_t *hv)
{
    unsigned int width = bf->hash_width;
    char         arr[bf->hash_bytes];
    char        *hash_stream = bloom_get_stream(bf, hv, arr);
    uint64_t     one_hash;

    for (unsigned int i = 0; i < bf->num_hashes; i++) {
        one_hash = bloom_hash_value(hash_stream, width);
        hash_stream += width;

        uint64_t           word_number = one_hash >> 6;
        uint64_t           bit_index   = one_hash & 63;
        uint64_t           one_bit     = 1 << bit_index;
        _Atomic(uint64_t) *p           = bf->bitfield + word_number;
        uint64_t           actual      = atomic_read(p);
        if (!(actual & one_bit)) {
            return false;
        }
    }

    return true;
}
