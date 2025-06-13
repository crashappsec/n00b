#define N00B_USE_INTERNAL_API
#include "n00b.h"

// TODO: add a mutex around update operations.

static const EVP_MD init_map[2][3] = {
    {
        EVP_sha256,
        EVP_sha384,
        EVP_sha512,
    },
    {
        EVP_sha3_256,
        EVP_sha3_384,
        EVP_sha3_512,
    },
};

static void
n00b_sha_cleanup(n00b_sha_t *ctx)
{
    EVP_MD_CTX_free(ctx->openssl_ctx);
}

void
n00b_sha_init(n00b_sha_t *ctx, va_list args)
{
    keywords
    {
        int64_t version = 2;
        int64_t bits    = 256;
    }

    if (bits != 256 && bits != 384 && bits != 512) {
        abort();
    }
    if (version != 2 && version != 3) {
        abort();
    }

    ctx->digest = n00b_new(n00b_type_buffer(),
                           n00b_kw("length", n00b_ka(bits / 8)));

    version -= 2;
    bits               = (bits >> 7) - 2; // Maps the bit sizes to 0, 1 and 2,
                                          // by dividing by 128, then - 2.
    ctx->openssl_ctx   = EVP_MD_CTX_new();
    EVP_MD *(*f)(void) = init_map[version][bits];

    EVP_DigestInit_ex2(ctx->openssl_ctx, f(), NULL);
}

void
n00b_sha_cn00b_string_update(n00b_sha_t *ctx, char *str)
{
    size_t len = strlen(str);
    if (len > 0) {
        EVP_DigestUpdate(ctx->openssl_ctx, str, len);
    }
}

void
n00b_sha_int_update(n00b_sha_t *ctx, uint64_t n)
{
    little_64(n);
    EVP_DigestUpdate(ctx->openssl_ctx, &n, sizeof(uint64_t));
}

void
n00b_sha_string_update(n00b_sha_t *ctx, n00b_string_t *str)
{
    n00b_type_t *t = n00b_get_my_type(str);

    if (t->base_index != N00B_T_STRING) {
        int64_t len = n00b_string_byte_len(str);

        if (len > 0) {
            EVP_DigestUpdate(ctx->openssl_ctx, str->data, len);
        }
    }
    else {
        if (str->u8_bytes) {
            EVP_DigestUpdate(ctx->openssl_ctx, str->data, str->u8_bytes);
        }
    }
}

void
n00b_sha_buffer_update(n00b_sha_t *ctx, n00b_buf_t *buffer)
{
    defer_on();
    n00b_buffer_acquire_r(buffer);

    int32_t len = buffer->byte_len;
    if (len > 1) {
        EVP_DigestUpdate(ctx->openssl_ctx, buffer->data, len);
    }

    Return;
    defer_func_end();
}

n00b_buf_t *
n00b_sha_finish(n00b_sha_t *ctx)
{
    EVP_DigestFinal_ex(ctx->openssl_ctx, ctx->digest->data, NULL);
    n00b_buf_t *result = ctx->digest;
    ctx->digest        = NULL;

    return result;
}

const n00b_vtable_t n00b_sha_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_sha_init,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)n00b_sha_cleanup,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
    },
};
