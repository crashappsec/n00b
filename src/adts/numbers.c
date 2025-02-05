#define N00B_USE_INTERNAL_API
#include "n00b.h"

inline int
clz_u128(__uint128_t u)
{
    uint64_t n;

    if ((n = u >> 64) != 0) {
        return __builtin_clzll(n);
    }
    else {
        if ((n = u & ~0ULL) != 0) {
            return 64 + __builtin_clzll(n);
        }
    }

    return 128;
}

static n00b_string_t *
signed_repr(int64_t item)
{
    // TODO, add hex as an option in how.
    char buf[21] = {
        0,
    };
    bool negative = false;

    if (item < 0) {
        negative = true;
        item *= -1;
    }

    if (item == 0) {
        n00b_cstring("0");
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    if (negative) {
        buf[--i] = '-';
    }

    return n00b_cstring(&buf[i]);
}

static n00b_string_t *
unsigned_repr(int64_t item)
{
    // TODO, add hex as an option in how.
    char buf[21] = {
        0,
    };

    if (item == 0) {
        n00b_cstring("0");
    }

    int i = 20;

    while (item) {
        buf[--i] = (item % 10) + '0';
        item /= 10;
    }

    return n00b_cstring(&buf[i]);
}

__uint128_t
n00b_raw_int_parse(n00b_string_t *u8, n00b_compile_error_t *err, bool *neg)
{
    __uint128_t cur  = 0;
    __uint128_t last = 0;
    char       *s    = u8->data;
    char       *p    = s;

    if (*p == '-') {
        *neg = true;
        p++;
    }
    else {
        *neg = false;
    }

    *err = n00b_err_no_error;

    int32_t c;

    while ((c = *p++) != 0) {
        c -= '0';
        last = cur;
        cur *= 10;
        if (c < 0 || c > 9) {
            if (err) {
                *err = n00b_err_parse_invalid_lit_char;
                // err->loc  = p - s - 1;
                return ~0;
            }
        }
        cur += c;
        if (cur < last) {
            if (err) {
                *err = n00b_err_parse_lit_overflow;
            }
        }
    }
    return cur;
}

__uint128_t
raw_hex_parse(n00b_string_t *u8, n00b_compile_error_t *err)
{
    // Here we expect *s to point to the first
    // character after any leading '0x'.
    __uint128_t cur = 0;
    char       *s   = u8->data + 2;
    char        c;
    bool        even = true;

    *err = n00b_err_no_error;

    while ((c = *s++) != 0) {
        if (cur & (((__uint128_t)0x0f) << 124)) {
            *err = n00b_err_parse_lit_overflow;
            return ~0;
        }
        even = !even;
        cur <<= 4;

        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            cur |= (c - '0');
            continue;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            cur |= (c + 10 - 'a');
            continue;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            cur |= (c + 10 - 'A');
            continue;
        default:
            *err = n00b_err_parse_invalid_lit_char;
            return ~0;
        }
    }

    if (!even) {
        *err = n00b_err_parse_lit_odd_hex;
        return ~0;
    }

    return cur;
}

#define BASE_INT_PARSE()                                       \
    bool        neg = false;                                   \
    __uint128_t val;                                           \
                                                               \
    switch (st) {                                              \
    case ST_Base10:                                            \
        val = n00b_raw_int_parse(s, code, &neg);               \
        break;                                                 \
    case ST_1Quote:                                            \
        N00B_CRAISE("Single quoted not reimplemented yet.\n"); \
    default:                                                   \
        val = raw_hex_parse(s, code);                          \
        break;                                                 \
    }                                                          \
                                                               \
    if (*code != n00b_err_no_error) {                          \
        return NULL;                                           \
    }

#define SIGNED_PARSE(underlow_val, overflow_val, magic_type) \
    BASE_INT_PARSE()                                         \
    if (neg) {                                               \
        if (val > overflow_val) {                            \
            *code = n00b_err_parse_lit_underflow;            \
            return NULL;                                     \
        }                                                    \
        return n00b_box_##magic_type(-1 * val);              \
    }                                                        \
    else {                                                   \
        if (st == ST_Base10 && val > overflow_val) {         \
            *code = n00b_err_parse_lit_overflow;             \
            return NULL;                                     \
        }                                                    \
        return n00b_box_##magic_type(val);                   \
    }

#define UNSIGNED_PARSE(overflow_val, magic_type) \
    BASE_INT_PARSE()                             \
    if (neg) {                                   \
        *code = n00b_err_parse_lit_invalid_neg;  \
        return NULL;                             \
    }                                            \
                                                 \
    if (val > overflow_val) {                    \
        *code = n00b_err_parse_lit_overflow;     \
        return NULL;                             \
    }                                            \
    return n00b_box_##magic_type(val)

static n00b_obj_t
i8_parse(n00b_string_t        *s,
         n00b_lit_syntax_t     st,
         n00b_string_t        *litmod,
         n00b_compile_error_t *code)
{
    SIGNED_PARSE(0x80, 0x7f, i8);
}

static n00b_obj_t
u8_parse(n00b_string_t        *s,
         n00b_lit_syntax_t     st,
         n00b_string_t        *litmod,
         n00b_compile_error_t *code)
{
    UNSIGNED_PARSE(0xff, u8);
}

static n00b_obj_t
i32_parse(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *litmod,
          n00b_compile_error_t *code)
{
    SIGNED_PARSE(0x80000000, 0x7fffffff, i32);
}

static n00b_obj_t
u32_parse(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *litmod,
          n00b_compile_error_t *code)
{
    UNSIGNED_PARSE(0xffffffff, u32);
}

static n00b_obj_t
i64_parse(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *litmod,
          n00b_compile_error_t *code)
{
    SIGNED_PARSE(0x8000000000000000, 0x7fffffffffffffff, i64);
}

bool
n00b_parse_int64(n00b_string_t *s, int64_t *out)
{
    n00b_compile_error_t err = n00b_err_no_error;

    int64_t *res = i64_parse(s, ST_Base10, NULL, &err);

    if (err != n00b_err_no_error) {
        return false;
    }

    *out = *res;
    return true;
}

static n00b_obj_t
u64_parse(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *litmod,
          n00b_compile_error_t *code)
{
    UNSIGNED_PARSE(0xffffffffffffffff, u64);
}

static n00b_obj_t false_lit = NULL;
static n00b_obj_t true_lit  = NULL;

static n00b_obj_t
bool_parse(n00b_string_t        *u8,
           n00b_lit_syntax_t     st,
           n00b_string_t        *litmod,
           n00b_compile_error_t *code)
{
    char *s = u8->data;
    *code   = n00b_err_no_error;

    switch (*s++) {
    case 't':
    case 'T':
        if (!strcmp(s, "rue")) {
            if (true_lit == NULL) {
                true_lit = n00b_box_bool(true);
                n00b_gc_register_root(&true_lit, 1);
            }
            return true_lit;
        }
        break;
    case 'f':
    case 'F':
        if (!strcmp(s, "alse")) {
            if (false_lit == NULL) {
                false_lit = n00b_box_bool(false);
                n00b_gc_register_root(&false_lit, 1);
            }
            return false_lit;
        }
        break;
    default:
        break;
    }

    *code = n00b_err_parse_invalid_lit_char;

    return NULL;
}

n00b_obj_t
f64_parse(n00b_string_t        *s,
          n00b_lit_syntax_t     st,
          n00b_string_t        *litmod,
          n00b_compile_error_t *code)
{
    *code = n00b_err_no_error;
    char  *end;
    double d = strtod(s->data, &end);

    if (end == s->data || *end) {
        *code = n00b_err_parse_invalid_lit_char;
        return NULL;
    }

    if (errno == ERANGE) {
        if (d == HUGE_VAL) {
            *code = n00b_err_parse_lit_overflow;
            return NULL;
        }
        *code = n00b_err_parse_lit_underflow;
        return NULL;
    }
    return n00b_box_double(d);
}

bool
n00b_parse_double(n00b_string_t *s, double *out)
{
    n00b_compile_error_t err;

    double *res = f64_parse(s, ST_Float, NULL, &err);

    if (err != n00b_err_no_error) {
        return false;
    }
    *out = *res;
    return true;
}

static n00b_string_t *true_repr  = NULL;
static n00b_string_t *false_repr = NULL;

static n00b_string_t *
bool_repr(bool b)
{
    if (b == false) {
        if (false_repr == NULL) {
            false_repr = n00b_cstring("false");
            n00b_gc_register_root(&false_repr, 1);
        }
        return false_repr;
    }
    if (true_repr == NULL) {
        true_repr = n00b_cstring("true");
        n00b_gc_register_root(&true_repr, 1);
    }

    return true_repr;
}

static bool
any_number_can_coerce_to(n00b_type_t *my_type, n00b_type_t *target_type)
{
    switch (n00b_type_get_data_type_info(target_type)->typeid) {
    case N00B_T_BOOL:
    case N00B_T_I8:
    case N00B_T_BYTE:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_U32:
    case N00B_T_INT:
    case N00B_T_UINT:
    case N00B_T_F32:
    case N00B_T_F64:
        return true;
    case N00B_T_SIZE:
        switch (n00b_type_get_data_type_info(my_type)->typeid) {
        case N00B_T_I8:
        case N00B_T_BYTE:
        case N00B_T_I32:
        case N00B_T_CHAR:
        case N00B_T_U32:
        case N00B_T_INT:
        case N00B_T_UINT:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

static void *
any_int_coerce_to(const int64_t data, n00b_type_t *target_type)
{
    double d;

    switch (n00b_type_get_data_type_info(target_type)->typeid) {
    case N00B_T_BOOL:
    case N00B_T_I8:
    case N00B_T_BYTE:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_U32:
    case N00B_T_INT:
    case N00B_T_UINT:
        return (void *)data;
    case N00B_T_SIZE:
        return n00b_new(n00b_type_size(), n00b_kw("bytes", n00b_ka(data)));
    case N00B_T_F32:
    case N00B_T_F64:
        d = (double)(data);
        return n00b_double_to_ptr(d);
    default:
        N00B_CRAISE("Invalid type conversion.");
    }
}

static void *
bool_coerce_to(const int64_t data, n00b_type_t *target_type)
{
    switch (n00b_type_get_data_type_info(target_type)->typeid) {
    case N00B_T_BOOL:
    case N00B_T_I8:
    case N00B_T_BYTE:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_U32:
    case N00B_T_INT:
    case N00B_T_UINT:
        if (data) {
            return (void *)NULL;
        }
        else {
            return NULL;
        }
    case N00B_T_F32:
    case N00B_T_F64:
        if (data) {
            return n00b_double_to_ptr(1.0);
        }
        else {
            return n00b_double_to_ptr(0.0);
        }
    default:
        N00B_CRAISE("Invalid type conversion.");
    }
}

static n00b_string_t *
float_repr(void *d)
{
    char buf[20] = {
        0,
    };

    // snprintf includes null terminator in its count.
    snprintf(buf, 20, "%g", ((n00b_box_t)d).dbl);
    return n00b_cstring(buf);
}

static void *
float_coerce_to(const double d, n00b_type_t *target_type)
{
    int64_t i;

    switch (n00b_type_get_data_type_info(target_type)->typeid) {
    case N00B_T_BOOL:
    case N00B_T_I8:
    case N00B_T_BYTE:
    case N00B_T_I32:
    case N00B_T_CHAR:
    case N00B_T_U32:
    case N00B_T_INT:
    case N00B_T_UINT:
        i = (int64_t)d;

        return (void *)i;
    case N00B_T_F32:
    case N00B_T_F64:
        return n00b_double_to_ptr(d);
    default:
        N00B_CRAISE("Invalid type conversion.");
    }
}

static n00b_string_t *
u8_fmt(uint8_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_uint((uint64_t)*repr, commas);
}

static n00b_string_t *
i8_fmt(int8_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_int((__int128_t)*repr, commas);
}

static n00b_string_t *
u32_fmt(uint32_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_uint((uint64_t)*repr, commas);
}

static n00b_string_t *
i32_fmt(int32_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_int((__int128_t)*repr, commas);
}

static n00b_string_t *
u64_fmt(uint64_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_uint(*repr, commas);
}

static n00b_string_t *
i64_fmt(int64_t *repr, n00b_string_t *spec)
{
    bool commas = spec && spec->codepoints && spec->data[0] == ',';
    return n00b_fmt_int((__int128_t)*repr, commas);
}

n00b_string_t *
n00b_int_to_hex(int64_t n)
{
    return n00b_fmt_hex((uint64_t)n, false);
}

static n00b_string_t *
bool_fmt(bool *repr, n00b_string_t *spec)
{
    if (!spec || !spec->codepoints) {
        return n00b_fmt_bool(*repr, false, true, false);
    }

    if (spec->codepoints == 1) {
        switch (spec->data[0]) {
        case 'b': // Boolean
            return n00b_fmt_bool(*repr, false, true, false);
        case 'B':
            return n00b_fmt_bool(*repr, true, true, false);
        case 't':
            return n00b_fmt_bool(*repr, false, false, false);
        case 'T':
            return n00b_fmt_bool(*repr, true, false, false);
        case 'y':
            return n00b_fmt_bool(*repr, false, false, true);
        case 'Y':
            return n00b_fmt_bool(*repr, true, false, true);
        case 'q': // Question
            return n00b_fmt_bool(*repr, false, true, true);
        case 'Q':
            return n00b_fmt_bool(*repr, true, true, true);
        default:
            break;
        }
    }
    N00B_CRAISE("Invalid boolean output type specifier");
}

static n00b_string_t *
float_fmt(double *repr, n00b_string_t *f)
{
    return n00b_fmt_float(*repr, 0, false);
}

const n00b_vtable_t n00b_u8_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)unsigned_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)u8_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)u8_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_i8_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)signed_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)i8_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)i8_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_u32_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)unsigned_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)u32_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)u32_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_i32_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)signed_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)i32_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)i32_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_u64_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)unsigned_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)u64_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)u64_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_i64_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)signed_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)i64_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)any_int_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)i64_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_bool_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)bool_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)bool_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)bool_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)bool_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};

const n00b_vtable_t n00b_float_type = {
    .methods = {
        [N00B_BI_REPR]         = (n00b_vtable_entry)float_repr,
        [N00B_BI_FORMAT]       = (n00b_vtable_entry)float_fmt,
        [N00B_BI_COERCIBLE]    = (n00b_vtable_entry)any_number_can_coerce_to,
        [N00B_BI_COERCE]       = (n00b_vtable_entry)float_coerce_to,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)f64_parse,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FINALIZER]    = NULL,

    },
};
