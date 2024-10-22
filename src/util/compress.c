#include "n00b.h"

static int
dummy_write_callback(n00b_archive_ctx *a, void *thunk)
{
    return ARCHIVE_OK;
}

static la_ssize_t
archive_write_cb(n00b_archive_ctx *a,
                 void            *client_data,
                 const void      *buffer,
                 size_t           length)
{
    n00b_stream_t *stream = (n00b_stream_t *)client_data;

    return n00b_stream_raw_write(stream, length, (void *)buffer);
}

void
n00b_compress(n00b_buf_t              *buffer,
             n00b_stream_t           *outstream,
             n00b_compression_suite_t suite)
{
    n00b_archive_ctx *ctx = archive_write_new();

    switch (suite >> 2) {
    case n00b_compress_none:
        break;
    case (n00b_compress_bzip2 >> 2):
        archive_write_add_filter_bzip2(ctx);
        break;
    case (n00b_compress_compress >> 2):
        archive_write_add_filter_compress(ctx);
        break;
    case (n00b_compress_grzip >> 2):
        archive_write_add_filter_grzip(ctx);
        break;
    case (n00b_compress_gzip >> 2):
        archive_write_add_filter_gzip(ctx);
        break;
    case (n00b_compress_lrzip >> 2):
        archive_write_add_filter_lrzip(ctx);
        break;
    case (n00b_compress_lz4 >> 2):
        archive_write_add_filter_lz4(ctx);
        break;
    case (n00b_compress_lzip >> 2):
        archive_write_add_filter_lzip(ctx);
        break;
    case (n00b_compress_lzma >> 2):
        archive_write_add_filter_lzma(ctx);
        break;
    case (n00b_compress_lzop >> 2):
        archive_write_add_filter_lzop(ctx);
        break;
    case (n00b_compress_xz >> 2):
        archive_write_add_filter_xz(ctx);
        break;
    case (n00b_compress_zstd >> 2):
        archive_write_add_filter_zstd(ctx);
        break;
    default:
        n00b_unreachable();
    }

    switch (suite & 0x03) {
    case 0:
        break;
    case 1:
        archive_write_add_filter_b64encode(ctx);
        break;
    case 2:
        archive_write_add_filter_uuencode(ctx);
        break;
    default:
        n00b_unreachable();
    }

    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_size(entry, n00b_buffer_len(buffer));
    archive_entry_set_pathname(entry, "!");
    archive_entry_set_filetype(entry, AE_IFREG);
    printf("write header: %d\n", archive_write_header(ctx, entry));
    printf("Set format: %d\n", archive_write_set_format_raw(ctx));
    printf("Open: %d\n", archive_write_open(ctx, outstream, dummy_write_callback, archive_write_cb, dummy_write_callback));
    archive_write_data(ctx, buffer->data, n00b_buffer_len(buffer));
    printf("Current error: %s\n", archive_error_string(ctx));
}
