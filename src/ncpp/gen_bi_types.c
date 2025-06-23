#include "core/builtin_ctypes.h"

char *
process_name(const char *n)
{
    char c = *n;
    if (c < 'A' || c > 'Z') {
        return (char *)n;
    }

    char *result = strdup(n);
    char *p      = result;

    while (c) {
        if (c >= 'A' && c <= 'Z') {
            *p = (c - 'A') + 'a';
        }
        c = *(++p);
    }

    return result;
}

void
gen_bi_types(void)
{
    int   i;
    char *name;

    // Preamble
    printf(
        "#pragma once\n#include \"n00b/base.h\"\n\n"
        "// Note: This file is generated; do not edit it. Instead, run:\n"
        "//    ncpp gen > include/core/builtin_tinfo_gen.h\n//\n"
        "// This should be run from the top level directory of the repo, \n"
        "// when you've made changes to the builtin C types.\n\n");

    for (i = 0; i < N00B_T_NUM_PRIMITIVE_BUILTINS; i++) {
        name = process_name(n00b_base_type_info[i].name);

        printf(
            "static inline n00b_ntype_t\n"
            "n00b_type_%s(void)\n"
            "{\n"
            "    return %d;\n"
            "}\n\n",
            name,
            i);
        printf(
            "static inline bool\n"
            "n00b_type_is_%s(n00b_ntype_t type_id)\n"
            "{\n"
            "    return type_id == %d;\n"
            "}\n\n",
            name,
            i);
    }

    // Use the same i, just skip N00B_T_NUM_PRIMITIVE_BUILTINS
    for (++i; i < N00B_NUM_BUILTIN_DTS; i++) {
        name = process_name(n00b_base_type_info[i].name);

        printf(
            "static inline bool\n"
            "n00b_type_is_%s(n00b_ntype_t typeid)\n"
            "{\n"
            "    return _n00b_has_base_container_type(typeid, %d, NULL);\n"
            "}\n",
            name,
            i);
    }
}
