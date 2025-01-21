#pragma once
#include "n00b.h"

typedef struct archive n00b_archive_ctx;

// These are constructed so the two least significant bits
// specify the encoding algorithm.
typedef enum {
    n00b_compress_none                   = 0,
    n00b_compress_none_with_b64          = 0x01,
    n00b_compress_none_with_uuencode     = 0x02,
    n00b_compress_bzip2                  = 0x04,
    n00b_compress_bzip2_with_b64         = 0x05,
    n00b_compress_bzip2_with_uuencode    = 0x06,
    n00b_compress_compress               = 0x08,
    n00b_compress_compress_with_b64      = 0x09,
    n00b_compress_compress_with_uuencode = 0x0a,
    n00b_compress_grzip                  = 0x0c,
    n00b_compress_grzip_with_b64         = 0x0d,
    n00b_compress_grzip_with_uuencode    = 0x0e,
    n00b_compress_gzip                   = 0x10,
    n00b_compress_gzip_with_b64          = 0x11,
    n00b_compress_gzip_with_uuencode     = 0x12,
    n00b_compress_lrzip                  = 0x14,
    n00b_compress_lrzip_with_b64         = 0x15,
    n00b_compress_lrzip_with_uuencode    = 0x16,
    n00b_compress_lz4                    = 0x18,
    n00b_compress_lz4_with_b64           = 0x19,
    n00b_compress_lz4_with_uuencode      = 0x1a,
    n00b_compress_lzip                   = 0x1c,
    n00b_compress_lzip_with_b64          = 0x1d,
    n00b_compress_lzip_with_uuencode     = 0x1e,
    n00b_compress_lzma                   = 0x20,
    n00b_compress_lzma_with_b64          = 0x21,
    n00b_compress_lzma_with_uuencode     = 0x22,
    n00b_compress_lzop                   = 0x24,
    n00b_compress_lzop_with_b64          = 0x25,
    n00b_compress_lzop_with_uuencode     = 0x26,
    n00b_compress_xz                     = 0x28,
    n00b_compress_xz_with_b64            = 0x29,
    n00b_compress_xz_with_uuencode       = 0x2a,
    n00b_compress_zstd                   = 0x2c,
    n00b_compress_zstd_with_b64          = 0x2d,
    n00b_compress_zstd_with_uuencode     = 0x2e,
} n00b_compression_suite_t;

extern void
n00b_compress(n00b_buf_t              *buffer,
              n00b_stream_t            *outstream,
              n00b_compression_suite_t suite);
