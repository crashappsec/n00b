#include "n00b.h"

// At least for the time being, we will statically ensure that there
// is a function in the compilation context with the right name and
// signature. For extern stuff, we will not attempt to bind until
// runtime.
//
// Note, there are two different callback abstractions in this file, the
// compile-time info we keep on callbacks (`n00b_callback_info_t`), and
//  the bare bones runtime callback object, `n00b_zcallback_t`.
//
// The former is not a N00b object, as it's not available at runtime.

n00b_callback_info_t *
n00b_callback_info_init()

{
    return n00b_gc_alloc_mapped(n00b_callback_info_t,
                               N00B_GC_SCAN_ALL);
    /*
    result->target_symbol_name = n00b_to_utf8(symbol_name);
    result->target_type        = type;

    return result;
    */
}

void
n00b_zcallback_gc_bits(uint64_t *bitmap, void *base)
{
    n00b_zcallback_t *cb = (void *)base;

    n00b_mark_raw_to_addr(bitmap, base, &cb->tid);
}

static n00b_utf8_t *
zcallback_repr(n00b_zcallback_t *cb)
{
    return n00b_cstr_format("func {}{}", cb->name, cb->tid);
}

const n00b_vtable_t n00b_callback_vtable = {
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_GC_MAP]    = (n00b_vtable_entry)n00b_zcallback_gc_bits,
        [N00B_BI_REPR]      = (n00b_vtable_entry)zcallback_repr,
        [N00B_BI_FINALIZER] = NULL,
    },
};
