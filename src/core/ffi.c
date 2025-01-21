#define N00B_USE_INTERNAL_API
#include "n00b.h"

typedef struct {
    char   *ctype_name;
    uint8_t index;
    bool    can_take_param;
} ctype_name_info_t;

static const ctype_name_info_t ctype_name_info[] = {
    // clang-format off
    { "void",     0,  false, },
    { "cvoid",    0,  false, },
    { "u8",       1,  false, },
    { "cu8",      1,  false, },
    { "i8",       2,  false, },
    { "ci8",      2,  false, },
    { "u16",      3,  false, },
    { "cu16",     3,  false, },
    { "i16",      4,  false, },
    { "ci16",     4,  false, },
    { "u32",      5,  false, },
    { "cu32",     5,  false, },
    { "i32",      6,  false, },
    { "ci32",     6,  false, },
    { "u64",      7,  false, },
    { "cu64",     7,  false, },
    { "i64",      8,  false, },
    { "ci64",     8,  false, },
    { "cfloat",   9,  false, },
    { "cdouble",  10, false, },
    { "double",   10, false, },
    { "cuchar",   11, false, },
    { "cchar",    12, false, },
    { "short",    13, false, },
    { "cshort",   13, false, },
    { "ushort",   14, false, },
    { "cushort",  14, false, },
    { "cint",     15, false, },
    { "cuint",    16, false, },
    { "ulong",    17, false, },
    { "long",     18, false, },
    { "bool",     19, false, },
    { "size_t",   20, false, },
    { "size",     20, false, },
    { "size_t",   20, false, },
    { "csize",    20, false, },
    { "csize_t",  20, false, },
    { "ssize",    21, false, },
    { "ssize_t",  21, false, },
    { "cssize",   21, false, },
    { "ssize_t",  21, false, },
    { "cssize_t", 21, false, },
    { "ptr",      22, true, },
    { "pointer",  22, true, },
    { "carray",   23, true, },
    { "array",    23, true, },
    { "cstring",  N00B_CSTR_CTYPE_CONST, true, },
    { NULL,       0,  false, },
    // clang-format on
};

static const n00b_ffi_type *ffi_type_map[] = {
    &ffi_type_void,
    &ffi_type_uint8,
    &ffi_type_sint8,
    &ffi_type_uint16,
    &ffi_type_sint16,
    &ffi_type_uint32,
    &ffi_type_sint32,
    &ffi_type_uint64,
    &ffi_type_sint64,
    &ffi_type_float,
    &ffi_type_double,
    &ffi_type_uchar,
    &ffi_type_schar,
    &ffi_type_ushort,
    &ffi_type_sshort,
    &ffi_type_uint,
    &ffi_type_sint,
    &ffi_type_ulong,
    &ffi_type_slong,
    &ffi_type_sint8, // Bool is 1 byte per the C standard.
    &ffi_type_uint,  // I believe size_t is always a unsigned integer.
    &ffi_type_sint,
    &ffi_type_pointer,
    &ffi_type_pointer,
    &ffi_type_pointer,
    NULL,
};

void *
n00b_ref_via_ffi_type(n00b_box_t *box, n00b_ffi_type *t)
{
    if (t == &ffi_type_uint8 || t == &ffi_type_sint8) {
        return &box->u8;
    }
    if (t == &ffi_type_uint16 || t == &ffi_type_sint16) {
        return &box->u16;
    }
    if (t == &ffi_type_uint32 || t == &ffi_type_sint32) {
        return &box->u32;
    }

    return box;
}

static n00b_dict_t *n00b_symbol_cache = NULL;

static inline void
ffi_init()
{
    if (n00b_symbol_cache == NULL) {
        n00b_push_heap(n00b_thread_master_heap);
        n00b_symbol_cache = n00b_dict(n00b_type_utf8(), n00b_type_ref());
        n00b_pop_heap();
    }
}

int64_t
n00b_lookup_ctype_id(char *found)
{
    ctype_name_info_t *info = (ctype_name_info_t *)&ctype_name_info[0];

    while (true) {
        if (info->ctype_name == NULL) {
            return -1;
        }
        if (!strcmp(info->ctype_name, found)) {
            return (int64_t)info->index;
        }

        info++;
    }
}

n00b_ffi_type *
n00b_ffi_arg_type_map(uint8_t ix)
{
    return (n00b_ffi_type *)ffi_type_map[ix];
}

void
n00b_add_static_function(n00b_utf8_t *name, void *symbol)
{
    ffi_init();

    n00b_push_heap(n00b_thread_master_heap);
    name = n00b_str_copy(name);
    n00b_pop_heap();
    hatrack_dict_put(n00b_symbol_cache, name, symbol);
}

void *
n00b_ffi_find_symbol(n00b_utf8_t *name, n00b_list_t *opt_libs)
{
    ffi_init();

    void *ptr = hatrack_dict_get(n00b_symbol_cache, name, NULL);

    if (ptr != NULL) {
        return ptr;
    }

    ptr = dlsym(RTLD_DEFAULT, name->data);

    if (ptr != NULL) {
        return ptr;
    }

    if (opt_libs == NULL) {
        int n = n00b_list_len(opt_libs);

        for (int i = 0; i < n; i++) {
            n00b_utf8_t *s = n00b_list_get(opt_libs, i, NULL);
            if (dlopen(s->data, RTLD_NOW | RTLD_GLOBAL) != NULL) {
                ptr = dlsym(RTLD_DEFAULT, name->data);
                if (ptr != NULL) {
                    return ptr;
                }
            }
        }
    }

    return NULL;
}
