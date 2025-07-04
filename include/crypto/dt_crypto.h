#pragma once
#include "n00b.h"

typedef void *EVP_MD_CTX;
typedef void *EVP_MD;
typedef void *OSSL_PARAM;

typedef struct {
    EVP_MD_CTX  openssl_ctx;
    n00b_buf_t *digest;
} n00b_sha_t;
