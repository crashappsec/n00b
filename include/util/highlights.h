#pragma once
#include "n00b.h"

typedef struct {
    n00b_buf_t *start_text;
    int64_t     start_offset;
    bool        end_is_pattern;
    union {
        n00b_buf_t *text;
        int64_t     offset;
    } end;
    n00b_style_t style;
} n00b_highlight_t;

extern void *n00b_apply_highlight(void *, n00b_highlight_t *);
extern void *n00b_apply_highlights(void *, n00b_list_t *);

// If 'text' is provided, offset is where to start highlighting, as
// measured from the END of an exact text match.
extern n00b_highlight_t *n00b_new_highlight(n00b_buf_t *,
                                            int64_t,
                                            int64_t,
                                            n00b_style_t);

extern n00b_highlight_t *n00b_new_highlight_to_text(n00b_buf_t *,
                                                    int64_t,
                                                    n00b_buf_t *,
                                                    n00b_style_t);
