#include "n00b.h"

n00b_utf8_t *
n00b_read_utf8_file(n00b_str_t *path)
{
    n00b_utf8_t   *result  = NULL;
    n00b_stream_t *stream  = NULL;
    bool          success = false;

    N00B_TRY
    {
        stream  = n00b_file_instream(n00b_to_utf8(path), N00B_T_UTF8);
        result  = (n00b_utf8_t *)n00b_stream_read_all(stream);
        success = true;
        n00b_stream_close(stream);
    }
    N00B_EXCEPT
    {
        if (stream != NULL) {
            n00b_stream_close(stream);
        }
    }
    N00B_TRY_END;

    // Don't return partial if we had a weird failure.
    if (success) {
        return result;
    }
    return NULL;
}

n00b_buf_t *
n00b_read_binary_file(n00b_str_t *path)
{
    n00b_buf_t    *result  = NULL;
    n00b_stream_t *stream  = NULL;
    bool          success = false;

    N00B_TRY
    {
        stream  = n00b_file_instream(n00b_to_utf8(path), N00B_T_BUFFER);
        result  = (n00b_buf_t *)n00b_stream_read_all(stream);
        success = true;
        n00b_stream_close(stream);
    }
    N00B_EXCEPT
    {
        if (stream != NULL) {
            n00b_stream_close(stream);
        }
    }
    N00B_TRY_END;

    // Don't return partial if we had a weird failure.
    if (success) {
        return result;
    }
    return NULL;
}
