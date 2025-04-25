#include "n00b.h"

void
n00b_bloom_init(n00b_bloom_t *bf, va_list args)
{
    double   false_pct      = 0.01;
    uint32_t expected_items = 250000;
    int      num_hashes     = -1; // Compute it.

    n00b_karg_va_init(args);
    n00b_kw_float("false_pct", false_pct);
    n00b_kw_uint32("set_size", expected_items);
    n00b_kw_int32("num_hashes", num_hashes);

    if (false_pct <= 0.0 || false_pct >= 1) {
        N00B_CRAISE("Invalid value for false_pct");
    }

    if (expected_items < 10) {
        expected_items = 10;
    }

    hatrack_bloom_init(bf, false_pct, expected_items, num_hashes);
}

const n00b_vtable_t n00b_bloom_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_bloom_init,
    },
};
