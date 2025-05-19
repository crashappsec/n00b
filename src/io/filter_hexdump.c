#include "n00b.h"

#define CPG    4  // Chars in a column grouping.
#define CPL    16 // Chars on an 80 column line.
#define HCOL1  10
#define ACOL1  62
#define NL_COL 78

typedef struct {
    uint8_t *hp;
    uint8_t *ap;
    uint8_t *aend;
    int      offset;
    int      n; // How many chars we've read; mod by CPL.
    uint8_t  line[80];
} hex_state_t;

static inline void
emit_line(hex_state_t *state, n00b_list_t *result)
{
    n00b_buf_t    *old = n00b_list_pop(result);
    n00b_string_t *s   = n00b_cstring_copy((char *)state->line);
    n00b_buf_t    *b   = n00b_string_to_buffer(s);

    if (old) {
        b = n00b_buffer_add(old, b);
    }

    n00b_list_append(result, b);
}

static inline void
setup_line(hex_state_t *state)
{
    state->hp  = &state->line[HCOL1];
    state->ap  = &state->line[ACOL1];
    uint8_t *p = &state->line[HCOL1];

    int x = state->n + state->offset;

    *--p = ' ';
    *--p = ' ';

    while (p != state->line) {
        for (int i = 0; i < HCOL1 - 2; i++) {
            *--p = n00b_hex_map_lower[x & 0xf];
            x >>= 4;
        }
    }
}

static void *
hex_setup(hex_state_t *state, void *param)
{
    state->offset = (int)(int64_t)param;
    memset(state->line, 0, 80);
    state->line[NL_COL] = '\n';

    return NULL;
}

n00b_list_t *
apply_hex(hex_state_t *state, void *msg)
{
    n00b_buf_t  *b      = NULL;
    n00b_list_t *result = n00b_list(n00b_type_buffer());
    n00b_type_t *t      = n00b_get_my_type(msg);

    if (n00b_type_is_string(t)) {
        b = n00b_string_to_buffer(msg);
    }
    else {
        if (!n00b_type_is_buffer(t)) {
            n00b_list_append(result, msg);
            return result;
        }
        b = msg;
    }

    if (!b->byte_len) {
        return result;
    }

    uint8_t *p   = (uint8_t *)b->data;
    uint8_t *end = (uint8_t *)b->data + b->byte_len;

    if (!state->hp) {
        state->n = 0;
        setup_line(state);
    }

    while (p < end) {
        uint8_t x    = *p++;
        *state->hp++ = n00b_hex_map_lower[x >> 4];
        *state->hp++ = n00b_hex_map_lower[x & 0xf];
        *state->hp++ = ' ';

        if (x < 32 || x > 126) {
            *state->ap++ = '.';
        }
        else {
            *state->ap++ = x;
        }

        if (!(++state->n % CPG)) {
            *state->hp++ = ' ';
        }
        if (!(state->n % CPL)) {
            emit_line(state, result);
            setup_line(state);
        }
    }

    return result;
}

n00b_list_t *
flush_hex(hex_state_t *state, void *msg)
{
    n00b_list_t *results;

    if (msg) {
        results = apply_hex(state, msg);
    }
    else {
        results = n00b_list(n00b_type_ref());
    }

    if (state->n % CPL) {
        *state->ap = 0;
        while (!state->hp && state->hp < state->ap) {
            *state->hp++ = ' ';
        }
        *state->ap++ = '\n';
        *state->ap   = 0;

        emit_line(state, results);
        memset(state, '0', sizeof(hex_state_t));
    }

    return results;
}

static n00b_filter_impl hex_filter = {
    .cookie_size = sizeof(hex_state_t),
    .setup_fn    = (void *)hex_setup,
    .read_fn     = (void *)apply_hex,
    .write_fn    = (void *)apply_hex,
    .flush_fn    = (void *)flush_hex,
    .name        = NULL,
};

n00b_filter_spec_t *
n00b_filter_hexdump(int64_t start_value)
{
    if (!hex_filter.name) {
        hex_filter.name = n00b_cstring("hex");
    }

    n00b_filter_spec_t *result = n00b_gc_alloc_mapped(n00b_filter_spec_t,
                                                      N00B_GC_SCAN_ALL);
    result->impl               = &hex_filter;
    result->policy             = N00B_FILTER_WRITE | N00B_FILTER_READ;
    result->param              = (void *)start_value;

    return result;
}
