#pragma once

#include "n00b.h"

extern void         n00b_utf8_ansi_render(const n00b_utf8_t *s,
                                          n00b_stream_t     *outstream);
extern void         n00b_utf32_ansi_render(const n00b_utf32_t *s,
                                           int32_t             start_ix,
                                           int32_t             end_ix,
                                           n00b_stream_t      *outstream);
extern void         n00b_ansi_render(const n00b_str_t *s,
                                     n00b_stream_t    *out);
extern void         n00b_ansi_render_to_width(const n00b_str_t *s,
                                              int32_t           width,
                                              int32_t           hang,
                                              n00b_stream_t    *out);
extern size_t       n00b_ansi_render_len(const n00b_str_t *s);
extern void         n00b_merge_ansi(n00b_stream_t *,
                                    size_t,
                                    size_t,
                                    bool);
extern n00b_utf8_t *n00b_ansify(n00b_str_t *s, bool, int);
