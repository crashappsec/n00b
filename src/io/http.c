#include "n00b.h"

static pthread_once_t init = PTHREAD_ONCE_INIT;

static void
init_curl(void)
{
    curl_global_init(CURL_GLOBAL_ALL);
}

static inline void
ensure_curl(void)
{
    pthread_once(&init, init_curl);
}

static char *
format_cookies(n00b_dict_t *cookies)
{
    uint64_t             n;
    hatrack_dict_item_t *view = hatrack_dict_items_sort(cookies, &n);
    // Start with one = and one ; per item, plus a null byte.
    int                  len  = n * 2 + 1;

    for (unsigned int i = 0; i < n; i++) {
        n00b_string_t *s = view[i].key;
        view[i].key      = s;
        len += n00b_string_byte_len(s);

        s             = view[i].value;
        view[i].value = s;
        len += n00b_string_byte_len(s);
    }

    char *res = n00b_gc_raw_alloc(len, N00B_GC_SCAN_NONE);
    char *p   = res;

    for (unsigned int i = 0; i < n; i++) {
        n00b_string_t *s = view[i].key;
        int            l = n00b_string_byte_len(s);

        memcpy(p, s->data, l);
        p += l;
        *p++ = '=';

        s = view[i].value;
        l = n00b_string_byte_len(s);

        memcpy(p, s->data, l);
        p += l;
        *p++ = ';';
    }

    return res;
}

static void
n00b_basic_http_teardown(n00b_basic_http_t *self)
{
    if (self->curl) {
        curl_easy_cleanup(self->curl);
    }
}

static size_t
http_out_to_stream(char              *ptr,
                   size_t             size,
                   size_t             nmemb,
                   n00b_basic_http_t *self)
{
    n00b_stream_write_memory(self->output_stream, ptr, size * nmemb);
    return size;
}

static size_t
internal_http_send(char              *ptr,
                   size_t             size,
                   size_t             nmemb,
                   n00b_basic_http_t *self)
{
    size_t      to_write  = size * nmemb;
    n00b_buf_t *out       = n00b_channel_unfiltered_read(self->to_send,
                                                   to_write);
    size_t      to_return = n00b_buffer_len(out);

    memcpy(ptr, out->data, to_return);

    return to_return;
}

void
n00b_basic_http_set_gc_bits(uint64_t *bitmap, n00b_basic_http_t *self)
{
    n00b_mark_raw_to_addr(bitmap, self, &self->errbuf);
}

#define HTTP_CORE_COMMON_START(provided_url, vararg_name)         \
    n00b_basic_http_response_t *result;                           \
    int64_t                     connect_timeout = -1;             \
    int64_t                     total_timeout   = -1;             \
    n00b_string_t              *aws_sig         = NULL;           \
    n00b_string_t              *access_key      = NULL;           \
    n00b_dict_t                *cookies         = NULL;           \
    n00b_basic_http_t          *connection      = NULL;           \
                                                                  \
    n00b_karg_only_init(vararg_name);                             \
    n00b_kw_int64("connect_timeout", connect_timeout);            \
    n00b_kw_int64("total_timeout", total_timeout);                \
    n00b_kw_ptr("aws_sig", aws_sig);                              \
    n00b_kw_ptr("access_key", access_key);                        \
    n00b_kw_ptr("cookies", cookies);                              \
                                                                  \
    connection = n00b_new(n00b_type_http(),                       \
                          n00b_kw("url",                          \
                                  n00b_ka(provided_url),          \
                                  "connect_timeout",              \
                                  n00b_ka(connect_timeout),       \
                                  "total_timeout",                \
                                  n00b_ka(total_timeout),         \
                                  "aws_sig",                      \
                                  n00b_ka(aws_sig),               \
                                  "access_key",                   \
                                  n00b_ka(access_key),            \
                                  "cookies",                      \
                                  n00b_ka(cookies)));             \
    result     = n00b_gc_alloc_mapped(n00b_basic_http_response_t, \
                                  N00B_GC_SCAN_ALL)

#define HTTP_CORE_COMMON_END()                            \
    n00b_basic_http_run_request(connection);              \
    result->code = connection->code;                      \
                                                          \
    if (connection->errbuf && connection->errbuf[0]) {    \
        result->error = n00b_cstring(connection->errbuf); \
    }                                                     \
                                                          \
    result->contents = connection->buf;                   \
                                                          \
    return result

n00b_basic_http_response_t *
_n00b_http_get(n00b_string_t *url, ...)
{
    HTTP_CORE_COMMON_START(url, url);
    n00b_basic_http_raw_setopt(connection, CURLOPT_HTTPGET, n00b_vp(1L));
    HTTP_CORE_COMMON_END();
}

n00b_basic_http_response_t *
_n00b_http_upload(n00b_string_t *url, n00b_buf_t *data, ...)
{
    // Does NOT make a copy of the buffer.

    HTTP_CORE_COMMON_START(url, data);
    n00b_basic_http_raw_setopt(connection, CURLOPT_UPLOAD, n00b_vp(1L));
    n00b_basic_http_raw_setopt(connection, CURLOPT_READFUNCTION, internal_http_send);
    n00b_basic_http_raw_setopt(connection, CURLOPT_READDATA, (void *)connection);
    n00b_basic_http_raw_setopt(connection,
                               CURLOPT_INFILESIZE_LARGE,
                               n00b_vp(data->byte_len));
    connection->to_send = n00b_instream_buffer(data);

    HTTP_CORE_COMMON_END();
}

static void
n00b_basic_http_init(n00b_basic_http_t *self, va_list args)
{
    int64_t         connect_timeout = -1;
    int64_t         total_timeout   = -1;
    n00b_string_t  *url             = NULL;
    n00b_string_t  *aws_sig         = NULL;
    n00b_string_t  *access_key      = NULL;
    n00b_dict_t    *cookies         = NULL;
    n00b_stream_t *output_stream   = NULL;

    ensure_curl();

    n00b_karg_va_init(args);

    n00b_kw_int64("connect_timeout", connect_timeout);
    n00b_kw_int64("total_timeout", total_timeout);
    n00b_kw_ptr("aws_sig", aws_sig);
    n00b_kw_ptr("access_key", access_key);
    n00b_kw_ptr("cookies", cookies);
    n00b_kw_ptr("url", url);
    n00b_kw_ptr("output_stream", output_stream);

    n00b_lock_init(&self->lock);

    // TODO: Do these in the near future (after objects)
    // bool       cert_info  = false;
    // n00b_kw_ptr("cert_info", cert_info);

    self->curl = curl_easy_init();

    if (url) {
        n00b_basic_http_raw_setopt(self, CURLOPT_URL, url->data);
    }

    if (total_timeout >= 0 && total_timeout > connect_timeout) {
        n00b_basic_http_raw_setopt(self,
                                   CURLOPT_TIMEOUT_MS,
                                   n00b_vp(total_timeout));
    }

    if (connect_timeout >= 0) {
        n00b_basic_http_raw_setopt(self,
                                   CURLOPT_CONNECTTIMEOUT_MS,
                                   n00b_vp(connect_timeout));
    }

    if (aws_sig != NULL) {
        n00b_basic_http_raw_setopt(self, CURLOPT_AWS_SIGV4, aws_sig->data);
    }

    if (access_key != NULL) {
        n00b_basic_http_raw_setopt(self, CURLOPT_USERPWD, access_key->data);
    }

    if (cookies) {
        n00b_basic_http_raw_setopt(self, CURLOPT_COOKIE, format_cookies(cookies));
    }

    if (output_stream) {
        self->output_stream = output_stream;
    }
    else {
        self->buf           = n00b_buffer_empty();
        self->output_stream = n00b_outstream_buffer(self->buf, true);
    }

    n00b_basic_http_raw_setopt(self, CURLOPT_WRITEFUNCTION, http_out_to_stream);
    n00b_basic_http_raw_setopt(self, CURLOPT_WRITEDATA, (void *)self);

    self->errbuf = n00b_gc_value_alloc(CURL_ERROR_SIZE);

    n00b_basic_http_raw_setopt(self, CURLOPT_ERRORBUFFER, self->errbuf);
}

const n00b_vtable_t n00b_basic_http_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_basic_http_init,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)n00b_basic_http_set_gc_bits,
        [N00B_BI_FINALIZER]   = (n00b_vtable_entry)n00b_basic_http_teardown,
    },
};
