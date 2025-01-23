#pragma once
#include "n00b.h"

typedef struct n00b_module_t {
    // The module_id is calculated by combining the package name and
    // the module name, then hashing it with SHA256. This is not
    // necessarily derived from the URI path.
    //
    // Note that packages (and our combining of it and the module) use
    // dotted syntax like with most PLs. When we combine for the hash,
    // we add a dot in there.
    //
    // n00b_new_compile_ctx will add __default__ as the package if none
    // is provided. The URI fields are optional (via API you can just
    // pass raw source as long as you give at least a module name).

    struct n00b_ct_module_info_t *ct;           // Compile-time only info.
    n00b_str_t                   *name;         // Module name.
    n00b_str_t                   *path;         // Fully qualified path
    n00b_str_t                   *package;      // Package name.
    n00b_str_t                   *full_uri;     // Abs path / URL if found.
    n00b_str_t                   *source;       // raw src before lex pass.
    n00b_scope_t                 *module_scope; // Symbols w/ module scope
    n00b_list_t                  *fn_def_syms;  // Cache of fns defined.
    n00b_list_t                  *extern_decls;
    n00b_list_t                  *instructions;
    n00b_dict_t                  *parameters;
    n00b_utf8_t                  *short_doc;
    n00b_utf8_t                  *long_doc;
    uint64_t                      modref;    // unique ref w/o src
    int32_t                       static_size;
    uint32_t                      module_id; // Index in object file.
} n00b_module_t;

extern void _n00b_set_package_search_path(n00b_utf8_t *, ...);
