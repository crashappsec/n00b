#include "n00b.h"

#define N00B_SZ_KB    1000ULL
#define N00B_SZ_KI    (1ULL << 10)
#define N00B_SZ_MB    1000000ULL
#define N00B_SZ_MI    (1ULL << 20)
#define N00B_SZ_GB    1000000000ULL
#define N00B_SZ_GI    (1ULL << 30)
#define N00B_SZ_TB    1000000000000ULL
#define N00B_SZ_TI    (1ULL << 40)
#define N00B_MAX_UINT (~0ULL)

static bool
parse_size_lit(n00b_string_t *to_parse, n00b_size_t *result, bool *oflow)
{
    __int128_t n_bytes = 0;
    __int128_t cur;

    if (oflow) {
        *oflow = false;
    }

    to_parse = n00b_string_lower(to_parse);

    int      l = n00b_string_byte_len(to_parse);
    char    *p = to_parse->data;
    char    *e = p + l;
    char     c;
    uint64_t multiplier;

    if (!l) {
        return false;
    }

    while (p < e) {
        c = *p;

        if (!isdigit(c)) {
            return false;
        }

        cur = 0;

        while (isdigit(c)) {
            cur *= 10;
            cur += (c - '0');

            if (cur > (__int128_t)N00B_MAX_UINT) {
                if (oflow) {
                    *oflow = true;
                }
                return false;
            }
            if (p == e) {
                return false;
            }
            c = *++p;
        }

        while (c == ' ' || c == ',') {
            if (p == e) {
                return false;
            }
            c = *++p;
        }

        switch (c) {
        case 'b':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'y') {
                    p++;
                    if (p != e) {
                        c = *p;
                        if (c == 't') {
                            p++;
                            if (p != e) {
                                c = *p;
                                if (c == 'e') {
                                    p++;
                                    if (p != e) {
                                        c = *p;
                                        if (c == 's') {
                                            p++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            multiplier = 1;
            break;
        case 'k':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = N00B_SZ_KI;
                }
                else {
                    multiplier = N00B_SZ_KB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 'm':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = N00B_SZ_MI;
                }
                else {
                    multiplier = N00B_SZ_MB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 'g':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = N00B_SZ_GI;
                }
                else {
                    multiplier = N00B_SZ_GB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        case 't':
            p++;
            if (p != e) {
                c = *p;
                if (c == 'i') {
                    p++;
                    multiplier = N00B_SZ_TI;
                }
                else {
                    multiplier = N00B_SZ_TB;
                }

                if (p != e) {
                    c = *p;
                    if (c == 'b') {
                        p++;
                    }
                }
            }
            break;
        default:
            return false;
        }
        n_bytes += multiplier * cur;

        if (n_bytes > (__int128_t)N00B_MAX_UINT) {
            if (oflow) {
                *oflow = true;
            }
            return false;
        }
        while (p < e) {
            c = *p;
            if (c != ' ' && c != ',') {
                break;
            }
            p++;
        }
    }

    uint64_t cast = (uint64_t)n_bytes;
    *result       = cast;

    return true;
}

static void
size_init(n00b_size_t *self, va_list args)
{
    keywords
    {
        n00b_string_t *to_parse = NULL;
        uint64_t       bytes    = 0;
    }

    bool oflow;

    if (to_parse != NULL && !parse_size_lit(to_parse, self, &oflow)) {
        if (oflow) {
            N00B_CRAISE("Size literal value is too large.");
        }
        else {
            N00B_CRAISE("Invalid size literal.");
        }
    }

    *self = bytes;
}

static n00b_string_t *
size_repr(n00b_size_t *self)
{
    // We produce both power of 2 and power of 10, and then return
    // the shorter of the 2.

    uint64_t       n   = *self;
    n00b_string_t *p10 = n00b_cached_empty_string();
    n00b_string_t *p2;
    uint64_t       tmp;

    if (!n) {
        return n00b_cstring("0 Bytes");
    }

    if (n >= N00B_SZ_TB) {
        tmp = n / N00B_SZ_TB;
        p10 = n00b_cformat("«#» Tb ", tmp);
        tmp *= N00B_SZ_TB;
        n -= tmp;
    }
    if (n >= N00B_SZ_GB) {
        tmp = n / N00B_SZ_GB;
        p10 = n00b_cformat("«#»«#» Gb ", p10, tmp);
        tmp *= N00B_SZ_GB;
        n -= tmp;
    }
    if (n >= N00B_SZ_MB) {
        tmp = n / N00B_SZ_MB;
        p10 = n00b_cformat("«#»«#» Mb ", p10, tmp);
        tmp *= N00B_SZ_MB;
        n -= tmp;
    }
    if (n >= N00B_SZ_KB) {
        tmp = n / N00B_SZ_KB;
        p10 = n00b_cformat("«#»«#» Kb ", p10, tmp);
        tmp *= N00B_SZ_KB;
        n -= tmp;
    }

    if (n != 0) {
        p10 = n00b_cformat("«#»«#» Bytes", p10, n00b_box_u64(n));
    }
    else {
        p10 = n00b_string_strip(p10);
    }

    n = *self;

    if (n < 1024) {
        return p10;
    }

    p2 = n00b_cached_empty_string();

    if (n >= N00B_SZ_TI) {
        tmp = n / N00B_SZ_TI;
        p2  = n00b_cformat("«#» TiB ", tmp);
        tmp *= N00B_SZ_TI;
        n -= tmp;
    }
    if (n >= N00B_SZ_GI) {
        tmp = n / N00B_SZ_GI;
        p2  = n00b_cformat("«#»«#» GiB ", p2, tmp);
        tmp *= N00B_SZ_GI;
        n -= tmp;
    }
    if (n >= N00B_SZ_MI) {
        tmp = n / N00B_SZ_MI;
        p2  = n00b_cformat("«#»«#» MiB ", p2, tmp);
        tmp *= N00B_SZ_MI;
        n -= tmp;
    }
    if (n >= N00B_SZ_KI) {
        tmp = n / N00B_SZ_KI;
        p2  = n00b_cformat("«#»«#» KiB ", p2, tmp);
        tmp *= N00B_SZ_KI;
        n -= tmp;
    }

    if (n != 0) {
        p2 = n00b_cformat("«#»«#» Bytes", p2, n);
    }
    else {
        p2 = n00b_string_strip(p2);
    }

    if (n00b_string_codepoint_len(p10) < n00b_string_codepoint_len(p2)) {
        return p10;
    }

    return p2;
}

static n00b_size_t *
size_lit(n00b_string_t        *s,
         n00b_lit_syntax_t     st,
         n00b_string_t        *mod,
         n00b_compile_error_t *err)
{
    n00b_size_t *result   = n00b_new(n00b_type_size());
    bool         overflow = false;

    if (st == ST_Base10) {
        __uint128_t v;
        bool        neg;
        v = n00b_raw_int_parse(s, err, &neg);

        if (neg) {
            *err = n00b_err_invalid_size_lit;
            return NULL;
        }

        if (*err != n00b_err_no_error) {
            return NULL;
        }

        if (v > (__uint128_t)N00B_MAX_UINT) {
            *err = n00b_err_parse_lit_overflow;
            return NULL;
        }
        *result = (uint64_t)v;

        return result;
    }

    if (!parse_size_lit(s, result, &overflow)) {
        if (overflow) {
            *err = n00b_err_parse_lit_overflow;
            return NULL;
        }
        else {
            *err = n00b_err_invalid_size_lit;
            return NULL;
        }
    }

    return result;
}

static bool
size_eq(n00b_size_t *r1, n00b_size_t *r2)
{
    return *r1 == *r2;
}

static bool
size_lt(n00b_size_t *r1, n00b_size_t *r2)
{
    return *r1 < *r2;
}

static bool
size_gt(n00b_size_t *r1, n00b_size_t *r2)
{
    return *r1 > *r2;
}

static n00b_size_t *
size_add(n00b_size_t *s1, n00b_size_t *s2)
{
    n00b_size_t *result = n00b_new(n00b_type_size());

    *result = *s1 + *s2;

    return result;
}

static n00b_size_t *
size_diff(n00b_size_t *s1, n00b_size_t *s2)
{
    n00b_size_t *result = n00b_new(n00b_type_size());

    if (*s1 > *s2) {
        *result = *s1 - *s2;
    }
    else {
        *result = *s2 - *s1;
    }

    return result;
}

static bool
size_can_coerce_to(n00b_ntype_t me, n00b_ntype_t them)
{
    switch (n00b_get_data_type_info(them)->typeid) {
    case N00B_T_INT:
    case N00B_T_UINT:
    case N00B_T_SIZE:
        return true;
    default:
        return false;
    }
}

static void *
size_coerce_to(n00b_size_t *self, n00b_ntype_t target_type)
{
    switch (n00b_get_data_type_info(target_type)->typeid) {
    case N00B_T_INT:
    case N00B_T_UINT:
        return (void *)*self;
    default:
        return self;
    }
}

const n00b_vtable_t n00b_size_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)size_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)size_repr,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)size_lit,
        [N00B_BI_EQ]           = (n00b_vtable_entry)size_eq,
        [N00B_BI_LT]           = (n00b_vtable_entry)size_lt,
        [N00B_BI_GT]           = (n00b_vtable_entry)size_gt,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_ADD]          = (n00b_vtable_entry)size_add,
        [N00B_BI_SUB]          = (n00b_vtable_entry)size_diff,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)size_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)size_coerce_to,
        [N00B_BI_FINALIZER]    = NULL,
    },
};
