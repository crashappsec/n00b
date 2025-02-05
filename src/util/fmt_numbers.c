#define N00B_USE_INTERNAL_API

#include "n00b.h"

#define FP_TO_string_BUFSZ        (24)
#define FP_MAX_INTERNAL_SZ     (100)
#define FP_OFFSET              (FP_MAX_INTERNAL_SZ - FP_TO_string_BUFSZ)
#define MAX_INT_LEN            (100)
#define LONGEST_UNICODE_ESCAPE (9)
#define LONGEST_PTR_REPR       (18)
#define MAX_CP                 (0x10FFFF)
#define INVALID_CP             (0xFFFD)

n00b_string_t *
n00b_fmt_hex(uint64_t value, bool caps)
{
    if (!value) {
        return n00b_cstring("0");
    }

    char *map               = (char *)(caps ? n00b_hex_map_upper
                                            : n00b_hex_map_lower);
    int   n                 = MAX_INT_LEN - 1;
    char  repr[MAX_INT_LEN] = {
        0,
    };

    while (value) {
        repr[--n] = map[value & 0x0f];
        value >>= 4;
    }

    return n00b_cstring(repr + n);
}

n00b_string_t *
n00b_fmt_int(__int128_t value, bool commas)
{
    if (!value) {
        return n00b_cstring("0");
    }

    bool sign              = false;
    int  n                 = MAX_INT_LEN - 1;
    char repr[MAX_INT_LEN] = {
        0,
    };

    if (value < 0) {
        sign = true;
        value *= -1;
    }

    if (!commas) {
        while (value) {
            repr[--n] = (value % 10) + '0';
            value /= 10;
        }
    }
    else {
        int digits = 0;

        while (value) {
            if (digits && !(digits % 3)) {
                repr[--n] = ',';
            }
            repr[--n] = (value % 10) + '0';
            value /= 10;
        }
    }

    if (sign) {
        repr[--n] = '-';
    }

    return n00b_cstring(repr + n);
}

n00b_string_t *
n00b_fmt_uint(uint64_t value, bool commas)
{
    if (!value) {
        return n00b_cstring("0");
    }

    int  n                 = MAX_INT_LEN - 1;
    char repr[MAX_INT_LEN] = {
        0,
    };

    if (!commas) {
        while (value) {
            repr[--n] = (value % 10) + '0';
            value /= 10;
        }
    }
    else {
        int digits = 0;

        while (value) {
            if (digits && !(digits % 3)) {
                repr[--n] = ',';
            }
            repr[--n] = (value % 10) + '0';
            value /= 10;
        }
    }

    return n00b_cstring(repr + n);
}

n00b_string_t *
n00b_fmt_float(double value, int width, bool fill)
{
    // Currently not using the precision field at all.
    char fprepr[FP_MAX_INTERNAL_SZ] = {
        0,
    };

    int  strlen     = n00b_internal_fptostr(value, fprepr + FP_OFFSET);
    int  n          = FP_OFFSET;
    bool using_sign = value < 0;

    if (width != 0 && strlen < width) {
        int tofill = width - strlen;

        if (fill) {
            // Zero-pad.
            n00b_string_t *pad = n00b_string_repeat('0', tofill);
            n00b_string_t *rest;
            n00b_string_t *sign;

            if (using_sign) {
                sign = n00b_string_repeat(fprepr[n++], 1);
            }

            rest = n00b_cstring(fprepr + n);

            if (using_sign) {
                pad = n00b_string_concat(sign, pad);
            }

            return n00b_string_concat(pad, rest);
        }
    }

    return n00b_cstring(fprepr + n);
}

n00b_string_t *
n00b_fmt_bool(bool value, bool upper, bool word, bool yn)
{
    int val = value ? 1 : 0;

    if (upper) {
        val |= 2;
    }

    if (word) {
        val |= 4;
    }

    if (yn) {
        val |= 8;
    }

    char *s;

    switch (val) {
    case 0:
        s = "f";
        break;
    case 1:
        s = "t";
        break;
    case 2:
        s = "F";
        break;
    case 3:
        s = "T";
        break;
    case 4:
        s = "false";
        break;
    case 5:
        s = "true";
        break;
    case 6:
        s = "False";
        break;
    case 7:
        s = "True";
        break;
    case 8:
        s = "n";
        break;
    case 9:
        s = "y";
        break;
    case 10:
        s = "N";
        break;
    case 11:
        s = "Y";
        break;
    case 12:
        s = "no";
        break;
    case 13:
        s = "yes";
        break;
    case 14:
        s = "No";
        break;
    case 15:
        s = "Yes";
        break;
    default:
        n00b_unreachable();
    }

    return n00b_cstring(s);
}

n00b_string_t *
n00b_fmt_codepoint(n00b_codepoint_t cp)
{
    if (cp > MAX_CP || cp < 0) {
        cp = INVALID_CP;
    }

    utf8proc_property_t *pi = (void *)utf8proc_get_property(cp);
    char                 buf[LONGEST_UNICODE_ESCAPE];
    int                  i;
    int                  extract;

    switch (pi->category) {
    case UTF8PROC_CATEGORY_CN:
    case UTF8PROC_CATEGORY_CC:
    case UTF8PROC_CATEGORY_CF:
    case UTF8PROC_CATEGORY_CS:
    case UTF8PROC_CATEGORY_CO:
        i      = 2;
        buf[0] = 'U';
        buf[1] = '+';

        if (cp & 0x100000) {
            buf[i++] = '1';
        }

        extract  = (cp >> 16) & 0x0f;
        buf[i++] = n00b_hex_map_upper[extract];
        extract  = (cp >> 12) & 0x0f;
        buf[i++] = n00b_hex_map_upper[extract];
        extract  = (cp >> 8) & 0x0f;
        buf[i++] = n00b_hex_map_upper[extract];
        extract  = (cp >> 4) & 0x0f;
        buf[i++] = n00b_hex_map_upper[extract];
        extract  = cp & 0xF;
        buf[i++] = n00b_hex_map_upper[extract];
        buf[i]   = 0;
        return n00b_cstring(buf);
    default:
        return n00b_string_from_codepoint(cp);
    }
}

n00b_string_t *
n00b_fmt_pointer(void *ptr, bool caps)
{
    char buf[LONGEST_PTR_REPR] = {
        '@',
    };

    uint64_t as_int = (uint64_t)ptr;
    uint64_t extract;
    char    *map = (char *)(caps ? n00b_hex_map_upper : n00b_hex_map_lower);

    extract = as_int >> 60;
    buf[1]  = map[extract];
    extract = (as_int >> 56) & 0x0f;
    buf[2]  = map[extract];
    extract = (as_int >> 52) & 0x0f;
    buf[3]  = map[extract];
    extract = (as_int >> 48) & 0x0f;
    buf[4]  = map[extract];
    extract = (as_int >> 44) & 0x0f;
    buf[5]  = map[extract];
    extract = (as_int >> 40) & 0x0f;
    buf[6]  = map[extract];
    extract = (as_int >> 36) & 0x0f;
    buf[7]  = map[extract];
    extract = (as_int >> 32) & 0x0f;
    buf[8]  = map[extract];
    extract = (as_int >> 28) & 0x0f;
    buf[9]  = map[extract];
    extract = (as_int >> 24) & 0x0f;
    buf[10] = map[extract];
    extract = (as_int >> 20) & 0x0f;
    buf[11] = map[extract];
    extract = (as_int >> 16) & 0x0f;
    buf[12] = map[extract];
    extract = (as_int >> 12) & 0x0f;
    buf[13] = map[extract];
    extract = (as_int >> 8) & 0x0f;
    buf[14] = map[extract];
    extract = (as_int >> 4) & 0x0f;
    buf[15] = map[extract];
    extract = as_int & 0x0f;
    buf[16] = map[extract];
    buf[17] = 0;

    return n00b_cstring(buf);
}
