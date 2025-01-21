// Filters return a list of messages to pass on.

#pragma once
#include "n00b.h"

extern n00b_stream_filter_t *n00b_new_filter(n00b_filter_fn,
                                             n00b_filter_fn,
                                             n00b_utf8_t *,
                                             size_t);
extern void                  n00b_remove_filter(n00b_stream_t *,
                                                n00b_stream_filter_t *);
extern n00b_stream_filter_t *n00b_new_ensure_utf8_filter(n00b_stream_t *);
extern void                  n00b_ensure_utf8_on_read(n00b_stream_t *);
extern void                  n00b_ensure_utf8_on_write(n00b_stream_t *);
extern n00b_stream_filter_t *n00b_new_line_buffering_xform(void);
extern void                  n00b_line_buffer_reads(n00b_stream_t *);
extern void                  n00b_line_buffer_writes(n00b_stream_t *);
extern n00b_stream_filter_t *n00b_new_custom_callback_filter(
    n00b_callback_filter);
extern void                  n00b_add_custom_read_filter(n00b_stream_t *,
                                                         n00b_callback_filter);
extern void                  n00b_add_custom_write_filter(n00b_stream_t *,
                                                          n00b_callback_filter);
extern n00b_stream_filter_t *n00b_new_to_json_xform(void);
extern void                  n00b_add_to_json_xform_on_read(n00b_stream_t *);
extern void                  n00b_add_to_json_xform_on_write(n00b_stream_t *);
extern n00b_stream_filter_t *n00b_new_from_json_xform(void);
extern void                  n00b_add_from_json_xform_on_read(n00b_stream_t *);
extern void                  n00b_add_from_json_xform_on_write(n00b_stream_t *);
extern void                  n00b_add_filter(n00b_stream_t *,
                                             n00b_stream_filter_t *,
                                             bool);
extern n00b_list_t          *n00b_hex_dump_xform(n00b_stream_t *s,
                                                 void          *ctx,
                                                 void          *msg);

static inline bool
n00b_add_read_filter(n00b_stream_t *stream, n00b_stream_filter_t *f)
{
    if (!n00b_stream_can_read(stream)) {
        return false;
    }

    n00b_list_append(stream->read_filters, f);
    return true;
}

static inline bool
n00b_add_write_filter(n00b_stream_t *stream, n00b_stream_filter_t *f)
{
    if (!n00b_stream_can_write(stream)) {
        return false;
    }

    n00b_list_append(stream->write_filters, f);
    return true;
}

static inline n00b_stream_filter_t *
n00b_new_hex_dump(void)
{
    return n00b_new_filter(n00b_hex_dump_xform,
                           NULL,
                           n00b_new_utf8("hex dump"),
                           sizeof(int64_t));
}

static inline void
n00b_add_hex_dump(n00b_stream_t *party)
{
    n00b_add_filter(party, n00b_new_hex_dump(), true);
    n00b_add_filter(party, n00b_new_hex_dump(), false);
}
