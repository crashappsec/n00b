#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
box_init(n00b_box_t *box, va_list args)
{
    n00b_box_t b = va_arg(args, n00b_box_t);
    box->u64     = b.u64;
    return;
}

static n00b_string_t *
box_to_string(n00b_box_t *box)
{
    return n00b_to_string_provided_type(box->v,
                                        n00b_type_unbox(n00b_get_my_type(box)));
}

static n00b_string_t *
box_format(n00b_box_t *box, n00b_string_t *spec)
{
    n00b_type_t    *t      = n00b_type_unbox(n00b_get_my_type(box));
    n00b_dt_info_t *info   = n00b_type_get_data_type_info(t);
    n00b_vtable_t  *vtable = (n00b_vtable_t *)info->vtable;
    n00b_format_fn  fn     = (n00b_format_fn)vtable->methods[N00B_BI_FORMAT];

    return (*fn)(box, spec);
}

static n00b_box_t
box_from_lit(n00b_box_t *b, void *i1, void *i2, void *i3)
{
    return n00b_unbox_obj(b);
}

const n00b_vtable_t n00b_box_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)box_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)box_to_string,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)box_format,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)box_from_lit,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]    = NULL,
    },
};
