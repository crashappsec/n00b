#define N00B_USE_INTERNAL_API
#include "n00b.h"

static const char *err1 =
    "Cannot set mixed object contents with a "
    "concrete type.";
static const char *err2 =
    "Expected type does not match actual type when "
    "extracting the value from a mixed-type object";

void
n00b_mixed_set_value(n00b_mixed_t *m, n00b_ntype_t type, void **ptr)
{
    if (type == N00B_T_ERROR) {
        if (ptr != NULL) {
            N00B_CRAISE((char *)err1);
        }

        m->held_type  = N00B_T_ERROR;
        m->held_value = NULL;
        return;
    }

    if (!n00b_type_is_concrete(type)) {
        N00B_CRAISE((char *)err1);
    }

    m->held_type = type;

    switch (n00b_get_data_type_info(type)->typeid) {
    case N00B_T_BOOL: {
        bool   *p = (bool *)ptr;
        bool    b = *p;
        int64_t n = (int64_t)b;

        m->held_value = (void *)n;
        return;
    }
    case N00B_T_I8: {
        char   *p = (char *)ptr;
        char    c = *p;
        int64_t n = (int64_t)c;

        m->held_value = (void *)n;
        return;
    }
    case N00B_T_BYTE: {
        uint8_t *p = (uint8_t *)ptr;
        uint8_t  c = *p;
        int64_t  n = (int64_t)c;

        m->held_value = (void *)n;
        return;
    }
    case N00B_T_I32: {
        int32_t *p = (int32_t *)ptr;
        int32_t  v = *p;
        int64_t  n = (int64_t)v;

        m->held_value = (void *)n;
        return;
    }
    case N00B_T_CHAR:
    case N00B_T_U32: {
        uint32_t *p = (uint32_t *)ptr;
        uint32_t  v = *p;
        int64_t   n = (int64_t)v;

        m->held_value = (void *)n;
        return;
    }
    case N00B_T_INT:
    case N00B_T_UINT:
        m->held_value = *ptr;
        return;
    case N00B_T_F32: {
        float *p      = (float *)ptr;
        float  f      = *p;
        double d      = (double)f;
        m->held_value = n00b_double_to_ptr(d);
        return;
    }
    case N00B_T_F64: {
        double *p     = (double *)ptr;
        double  d     = *p;
        m->held_value = n00b_double_to_ptr(d);
        return;
    }
    default:
        m->held_value = *(void **)ptr;
        return;
    }
}

static void
mixed_init(n00b_mixed_t *m, va_list args)
{
    keywords
    {
        n00b_ntype_t type       = N00B_T_ERROR;
        void        *ptr(value) = NULL;
    }

    n00b_mixed_set_value(m, type, ptr);
}

void
n00b_unbox_mixed(n00b_mixed_t *m, n00b_ntype_t expected_type, void **ptr)
{
    if (!n00b_types_are_compat(m->held_type, expected_type, NULL)) {
        N00B_CRAISE((char *)err2);
    }

    switch (n00b_get_data_type_info(m->held_type)->typeid) {
    case N00B_T_BOOL: {
        if (m->held_value) {
            *(bool *)ptr = true;
        }
        else {
            *(bool *)ptr = false;
        }
        return;
    }
    case N00B_T_I8: {
        int64_t n    = (int64_t)m->held_value;
        char    c    = n & 0xff;
        *(char *)ptr = c;
        return;
    }
    case N00B_T_BYTE: {
        int64_t n       = (int64_t)m->held_value;
        uint8_t c       = n & 0xff;
        *(uint8_t *)ptr = c;
        return;
    }
    case N00B_T_I32: {
        int64_t n       = (int64_t)m->held_value;
        int32_t v       = n & 0xffffffff;
        *(int32_t *)ptr = v;
        return;
    }
    case N00B_T_CHAR:
    case N00B_T_U32: {
        int64_t  n       = (int64_t)m->held_value;
        uint32_t v       = n & 0xffffffff;
        *(uint32_t *)ptr = v;
        return;
    }
    case N00B_T_INT:
        *(int64_t *)ptr = (int64_t)m->held_value;
        return;
    case N00B_T_UINT:
        *(uint64_t *)ptr = (uint64_t)m->held_value;
        return;
    case N00B_T_F32: {
        double d = n00b_ptr_to_double(m->held_value);

        *(float *)ptr = (float)d;
        return;
    }
    case N00B_T_F64: {
        *(double *)ptr = n00b_ptr_to_double(m->held_value);
        return;
    }
    default:
        *(void **)m->held_value = m->held_value;
        return;
    }
}

static int64_t
mixed_as_word(n00b_mixed_t *m)
{
    switch (n00b_get_data_type_info(m->held_type)->typeid) {
    case N00B_T_BOOL:
        if (m->held_value == NULL) {
            return 0;
        }
        else {
            return 1;
        }
        break;
    case N00B_T_I8: {
        char b;

        n00b_unbox_mixed(m, m->held_type, (void **)&b);
        return (int64_t)b;
    }
    case N00B_T_BYTE: {
        uint8_t b;

        n00b_unbox_mixed(m, m->held_type, (void **)&b);
        return (int64_t)b;
    }
    case N00B_T_I32: {
        int32_t n;

        n00b_unbox_mixed(m, m->held_type, (void **)&n);
        return (int64_t)n;
    }
    case N00B_T_CHAR:
    case N00B_T_U32: {
        uint32_t n;
        n00b_unbox_mixed(m, m->held_type, (void **)&n);
        return (int64_t)n;
    }
    default:
        return (int64_t)m->held_value;
    }
}

static n00b_string_t *
mixed_to_string(n00b_mixed_t *mixed)
{
    // For the value types, we need to convert them to a 64-bit equiv
    // to send to the appropriate repr.
    return n00b_to_string_provided_type((void *)mixed_as_word(mixed),
                                        mixed->held_type);
}

static n00b_string_t *
mixed_to_literal(n00b_mixed_t *mixed)
{
    // For the value types, we need to convert them to a 64-bit equiv
    // to send to the appropriate repr.
    return n00b_to_literal_provided_type((void *)mixed_as_word(mixed),
                                         mixed->held_type);
}

static n00b_mixed_t *
mixed_copy(n00b_mixed_t *m)
{
    n00b_mixed_t *result = n00b_new(N00B_T_GENERIC);

    // Types are concrete whenever there is a value, so we don't need to
    // call copy, but we do it anyway.

    result->held_type = n00b_type_copy(m->held_type);

    if (n00b_get_data_type_info(m->held_type)->by_value) {
        result->held_value = m->held_value;
    }
    else {
        result->held_value = n00b_copy(m->held_value);
    }

    return result;
}

const n00b_vtable_t n00b_mixed_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)mixed_init,
        [N00B_BI_TO_STRING]   = (n00b_vtable_entry)mixed_to_string,
        [N00B_BI_TO_LITERAL]  = (n00b_vtable_entry)mixed_to_literal,
        [N00B_BI_COPY]        = (n00b_vtable_entry)mixed_copy,
        [N00B_BI_GC_MAP]      = (n00b_vtable_entry)N00B_GC_SCAN_ALL,
        [N00B_BI_FINALIZER]   = NULL,
    },
};
