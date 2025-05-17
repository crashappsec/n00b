#pragma once
#include "n00b.h"

typedef struct n00b_filter_t      n00b_filter_t;
typedef struct n00b_filter_step_t n00b_filter_step_t;

typedef n00b_list_t *(*n00b_chan_filter_fn)(void *, void *);

typedef enum {
    // Filter whenever a read operation occurs. This occurs whenever
    // the read implementation function is invoked, which is either
    // when the user explicitly calls a read() function, or when the
    // base implementation is event-driven and has read subscribers.
    N00B_CHAN_FILTER_READS,
    // For the other two, any underlying implementation requires
    // something calling n00b_chan_write() or n00b_chan_read().  The
    // filtering occurs before the underlying implementation is
    // called.
    N00B_CHAN_FILTER_WRITES,
} n00b_chan_filter_policy;

enum {
    N00B_FILTER_READ  = 1,
    N00B_FILTER_WRITE = 2,
    N00B_FILTER_MAX   = 3,
};

// Note: have not added the typing yet.
typedef struct {
    int                 cookie_size;
    n00b_chan_filter_fn setup_fn;
    n00b_chan_filter_fn read_fn;
    n00b_chan_filter_fn write_fn;
    n00b_chan_filter_fn flush_fn;
    n00b_string_t      *name;
    n00b_type_t        *output_type;
    // If `polymorphic_w` is true, it indicates that filter WRITES to
    // a channel may accept multiple types. They must always go to the
    // underlying channel implementation as a buffer. And, any
    // individual filter must produce a single output type, even if
    // it's not intended to go to the channel without first going
    // through a conversion filter.
    //
    // The same thing happens for reads in reverse w
    // `polymorphic_r`... the first filter must accept the raw buffer
    // of the channel, and *should* produce a single type, but it is
    // not required (e.g., marshalling can produce any
    // type). Subsequent layers of filtering may also be flexible both
    // in and out, but in that case they will be type checked with
    // every message, instead of just when setting up the pipeline.
    unsigned int        polymorphic_r : 1;
    unsigned int        polymorphic_w : 1;

    /* Type checking to be done.
        union {
            n00b_type_t      *type;
            n00b_chan_type_fn validator;
        } input_type;
    */

} n00b_filter_impl;

struct n00b_filter_t {
    n00b_string_t    *name;
    n00b_list_t      *read_cache;
    n00b_filter_impl *impl;
    n00b_filter_t    *next_read_step;
    n00b_filter_t    *next_write_step;
    unsigned int      w_skip : 1;
    unsigned int      r_skip : 1;
    alignas(32) char cookie[];
};

typedef struct {
    n00b_filter_impl *impl;
    void             *param;
    int               policy;
} n00b_filter_spec_t;

extern n00b_filter_spec_t *n00b_filter_apply_color(int);
extern n00b_filter_spec_t *n00b_filter_apply_line_buffering(
    void);

extern n00b_filter_spec_t *n00b_filter_hexdump(int64_t);

extern n00b_filter_spec_t *n00b_filter_marshal(bool);

extern n00b_buf_t *n00b_automarshal(void *);
extern void       *n00b_autounmarshal(n00b_buf_t *);

// json filter
extern n00b_filter_spec_t *n00b_filter_json(void);
extern n00b_filter_spec_t *n00b_filter_to_json(int);
extern n00b_filter_spec_t *n00b_filter_from_json(int);

extern void n00b_chan_filter_add_to_json_on_read(
    struct n00b_channel_t *c);
extern void n00b_chan_filter_add_to_json_on_write(
    struct n00b_channel_t *c);

extern void n00b_chan_filter_add_from_json_on_write(
    struct n00b_channel_t *c);
extern void n00b_chan_filter_add_from_json_on_read(
    struct n00b_channel_t *c);

extern n00b_filter_spec_t *n00b_filter_parse_ansi(bool);
extern n00b_filter_spec_t *n00b_filter_strip_ansi(bool);
