#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_table_t *
n00b_get_module_summary_info(n00b_compile_ctx *ctx)
{
    int           n   = n00b_list_len(ctx->module_ordering);
    n00b_table_t *tbl = n00b_table("columns", n00b_ka(4));

    n00b_table_add_cell(tbl, n00b_cstring("Module"));
    n00b_table_add_cell(tbl, n00b_cstring("Path"));
    n00b_table_add_cell(tbl, n00b_cstring("Hash"));
    n00b_table_add_cell(tbl, n00b_cstring("Obj module index"));

    for (int i = 0; i < n; i++) {
        n00b_module_t *f = n00b_list_get(ctx->module_ordering, i, NULL);

        n00b_string_t *spec;

        if (f->package == NULL) {
            spec = f->name;
        }
        else {
            spec = n00b_cformat("«#».«#»", f->package, f->name);
        }

        n00b_string_t *hash = n00b_cformat("«#:x»", (int64_t)f->modref);
        n00b_string_t *mod  = n00b_cformat("«#»", (int64_t)f->module_id);

        n00b_table_add_cell(tbl, spec);
        n00b_table_add_cell(tbl, f->path);
        n00b_table_add_cell(tbl, hash);
        n00b_table_add_cell(tbl, mod);
    }

    return tbl;
}

void
n00b_module_ctx_gc_bits(uint64_t *bitfield, n00b_module_t *ctx)
{
    n00b_mark_raw_to_addr(bitfield, ctx, &ctx->long_doc);
}

n00b_module_t *
n00b_new_module_compile_ctx()
{
    n00b_module_t *res = n00b_gc_alloc_mapped(n00b_module_t,
                                              N00B_GC_SCAN_ALL);
    res->noscan        = N00B_NOSCAN;
    // n00b_module_ctx_gc_bits);

    res->ct         = n00b_gc_alloc_mapped(n00b_ct_module_info_t,
                                   N00B_GC_SCAN_ALL);
    res->ct->noscan = N00B_NOSCAN;

    return res;
}

static inline uint64_t
module_key(n00b_string_t *package, n00b_string_t *module)
{
    if (!package) {
        package = n00b_cached_empty_string();
    }

    n00b_sha_t sha;
    n00b_sha_init(&sha, NULL);
    n00b_sha_string_update(&sha, package);
    n00b_sha_int_update(&sha, '.');
    n00b_sha_string_update(&sha, module);

    n00b_buf_t *digest = n00b_sha_finish(&sha);

    return ((uint64_t *)digest->data)[0];
}

// package is the only string allowed to be null here.
// fext can also be null, in which case we take in the default values.
static n00b_module_t *
one_lookup_try(n00b_compile_ctx *ctx,
               n00b_string_t    *path,
               n00b_string_t    *module,
               n00b_string_t    *package,
               n00b_list_t      *fext)
{
    // First check the cache.
    uint64_t       key    = module_key(package, module);
    n00b_module_t *result = hatrack_dict_get(ctx->module_cache,
                                             (void *)key,
                                             NULL);

    if (result) {
        return result;
    }

    if (!fext) {
        fext = n00b_get_allowed_file_extensions();
    }

    n00b_list_t *l = n00b_list(n00b_type_string());

    n00b_list_append(l, path);

    if (package && n00b_string_byte_len(package)) {
        // For the package lookup, we need to replace dots in the package
        // name with slashes.
        n00b_string_t *s = n00b_string_replace(package,
                                               n00b_cached_period(),
                                               n00b_cached_slash());
        n00b_list_append(l, s);
    }
    n00b_list_append(l, module);

    n00b_string_t *base    = n00b_path_join(l);
    int            num_ext = n00b_list_len(fext);
    n00b_string_t *contents;

    n00b_string_t *attempt = NULL;

    for (int i = 0; i < num_ext; i++) {
        attempt = n00b_cformat("«#».«#»",
                               base,
                               n00b_list_get(fext, i, NULL));

        if (n00b_path_is_url(attempt)) {
            n00b_basic_http_response_t *r = n00b_http_get(attempt);
            contents                      = n00b_http_op_get_output_utf8(r);
        }
        else {
            contents = n00b_read_utf8_file(attempt, true);
        }
        if (contents != NULL) {
            break;
        }
    }

    // Tell the caller to try again with a different path.
    if (!contents) {
        return NULL;
    }

    result             = n00b_new_module_compile_ctx();
    result->name       = module;
    result->package    = package;
    result->path       = path;
    result->source     = contents;
    result->ct->errors = n00b_list(n00b_type_ref());
    result->full_uri   = attempt;
    result->modref     = key;

    n00b_buf_t    *b = n00b_new(n00b_type_buffer(),
                             n00b_kw("length",
                                     n00b_ka(n00b_string_byte_len(contents)),
                                     "ptr",
                                     n00b_ka(contents->data)));
    n00b_stream_t *s = n00b_instream_buffer(b);

    if (!n00b_lex(result, s)) {
        ctx->fatality = true;
    }

    hatrack_dict_put(ctx->module_cache, (void *)key, result);

    return result;
}

static inline void
adjust_it(n00b_string_t **pathp,
          n00b_string_t **pkgp,
          n00b_string_t  *path_string,
          n00b_string_t  *matched_system_path,
          int             matched_len,
          int             path_len)
{
    path_string = n00b_string_slice(path_string, matched_len, path_len);

    if (path_string->data[0] == '/') {
        path_string->data = path_string->data + 1;
        path_string->u8_bytes--;
        path_string->codepoints--;
    }

    n00b_string_t *new_prefix = n00b_string_replace(path_string,
                                                    n00b_cached_slash(),
                                                    n00b_cached_period());

    *pathp = matched_system_path;

    if (*pkgp == NULL || (*pkgp)->u8_bytes == 0) {
        *pkgp = new_prefix;
    }
    else {
        n00b_string_t *package = n00b_string_replace(*pkgp,
                                                     n00b_cached_slash(),
                                                     n00b_cached_period());
        if (!package->u8_bytes) {
            *pkgp = new_prefix;
        }
        else {
            *pkgp = n00b_cformat("«#»/«#»", new_prefix, package);
        }
    }
}

static void
adjust_path_and_package(n00b_string_t **pathp, n00b_string_t **pkgp)
{
    n00b_string_t *path    = *pathp;
    int            pathlen = n00b_string_byte_len(path);
    int            otherlen;

    n00b_list_t *sp = n00b_get_module_search_path();

    for (int i = 0; i < n00b_list_len(sp); i++) {
        n00b_string_t *possible = n00b_list_get(sp, i, NULL);

        possible = n00b_path_trim_trailing_slashes(possible);

        if (n00b_string_starts_with(path, possible)) {
            otherlen = n00b_string_byte_len(possible);

            if (pathlen == otherlen) {
                if (*pkgp && n00b_string_byte_len(*pkgp)) {
                    *pkgp = n00b_string_replace(*pkgp,
                                                n00b_cached_slash(),
                                                n00b_cached_period());
                }
                break;
            }
            if (possible->data[otherlen - 1] == '/') {
                adjust_it(pathp, pkgp, path, possible, otherlen, pathlen);
                break;
            }
            if (path->data[otherlen] == '/') {
                adjust_it(pathp, pkgp, path, possible, otherlen, pathlen);
                break;
            }
        }
    }
}

// In this function, the 'path' parameter is either a full URL or
// file system path, or a URL. If it's missing, we need to search.
//
// If the package is missing, EITHER it is going to be the same as the
// context we're in when we're searching (i.e., when importing another
// module), or it will be a top-level module.
//
// Note that the module must have the file extension stripped; the
// extension can be provided in the list, but if it's not there, the
// system extensions get searched.
n00b_module_t *
n00b_find_module(n00b_compile_ctx *ctx,
                 n00b_string_t    *path,
                 n00b_string_t    *module,
                 n00b_string_t    *package,
                 n00b_string_t    *relative_package,
                 n00b_string_t    *relative_path,
                 n00b_list_t      *fext)
{
    n00b_module_t *result;

    // If a path was provided, then the package / module need to be
    // fully qualified.
    if (path != NULL) {
        // clang-format off
	if (!n00b_string_starts_with(path, n00b_cached_slash()) &&
	    !n00b_path_is_url(path)) {
            // clang-format on

            path             = n00b_path_simple_join(relative_path, path);
            // Would be weird to take this relative to the file too.
            relative_package = NULL;
        }

        adjust_path_and_package(&path, &package);
        result = one_lookup_try(ctx, path, module, package, fext);
        return result;
    }

    if (package == NULL && relative_package != NULL) {
        result = one_lookup_try(ctx,
                                relative_path,
                                module,
                                relative_package,
                                fext);
        if (result) {
            return result;
        }
    }

    // At this point, it could be in the top level of the relative
    // path, or else we have to do a full search.

    if (relative_path != NULL) {
        result = one_lookup_try(ctx, relative_path, module, NULL, fext);
        if (result) {
            return result;
        }
    }

    n00b_list_t *sp = n00b_get_module_search_path();
    int          n  = n00b_list_len(sp);

    for (int i = 0; i < n; i++) {
        n00b_string_t *one = n00b_list_get(sp, i, NULL);

        result = one_lookup_try(ctx, one, module, package, fext);

        if (result) {
            return result;
        }
    }

    // If we searched everywhere and nothing, then it's not found.
    return NULL;
}

bool
n00b_add_module_to_worklist(n00b_compile_ctx *cctx, n00b_module_t *fctx)
{
    return n00b_set_add(cctx->backlog, fctx);
}

static n00b_module_t *
postprocess_module(n00b_compile_ctx *cctx,
                   n00b_module_t    *fctx,
                   n00b_string_t    *path,
                   bool              http_err,
                   n00b_string_t    *errmsg)
{
    if (!fctx) {
        n00b_module_t *result = n00b_new_module_compile_ctx();
        result->path          = path;
        result->ct->errors    = n00b_list(n00b_type_ref());
        cctx->fatality        = true;

        hatrack_dict_put(cctx->module_cache, NULL, result);

        if (!errmsg) {
            errmsg = n00b_cstring("Internal error");
        }
        n00b_module_load_error(result,
                               n00b_err_open_module,
                               path,
                               errmsg);

        return result;
    }

    if (http_err) {
        n00b_module_load_error(fctx, n00b_warn_no_tls);
    }

    return fctx;
}

n00b_string_t *
n00b_package_from_path_prefix(n00b_string_t *path, n00b_string_t **path_loc)
{
    n00b_list_t   *paths = n00b_get_module_search_path();
    n00b_string_t *one;

    int n = n00b_list_len(paths);

    for (int i = 0; i < n; i++) {
        one = n00b_list_get(paths, i, NULL);

        if (n00b_string_ends_with(one, n00b_cached_slash())) {
            one = n00b_string_copy(one);
            while (one->u8_bytes && one->data[one->u8_bytes - 1] == '/') {
                one->data[--one->u8_bytes] = 0;
                one->codepoints--;
            }
        }

        if (n00b_string_starts_with(path, one)) {
            *path_loc = one;

            int            ix = n00b_string_byte_len(one);
            n00b_string_t *s  = n00b_string_slice(path,
                                                 ix,
                                                 n00b_string_codepoint_len(path));
            n00b_path_strip_slashes_both_ends(s);

            for (int j = 0; j < s->u8_bytes; j++) {
                if (s->data[j] == '/') {
                    s->data[j] = '.';
                }
            }

            return s;
        }
    }

    return NULL;
}

static inline n00b_module_t *
ctx_init_from_web_uri(n00b_compile_ctx *ctx,
                      n00b_string_t    *inpath,
                      bool              has_ext,
                      bool              https)
{
    n00b_module_t *result;
    n00b_string_t *module;
    n00b_string_t *package;
    n00b_string_t *path;

    if (n00b_string_codepoint_len(inpath) <= 8) {
        goto malformed;
    }

    char *s;

    // We know the colon is there; start on it, and look for
    // the two slashes.
    if (https) {
        s = &inpath->data[6];
    }
    else {
        s = &inpath->data[5];
    }

    if (*s++ != '/') {
        goto malformed;
    }
    if (*s++ != '/') {
        goto malformed;
    }

    if (has_ext) {
        int n  = n00b_string_rfind(inpath, n00b_cached_slash()) + 1;
        module = n00b_string_slice(inpath, n, -1);
        inpath = n00b_string_slice(inpath, 0, n - 1);
    }
    else {
        module = n00b_cstring(N00B_PACKAGE_INIT_MODULE);
    }

    package = n00b_package_from_path_prefix(inpath, &path);

    if (!package) {
        path = inpath;
    }

    result = n00b_find_module(ctx, path, module, package, NULL, NULL, NULL);
    return postprocess_module(ctx, result, NULL, !https, NULL);

malformed:

    result       = n00b_new_module_compile_ctx();
    result->path = inpath;

    n00b_module_load_error(result, n00b_err_malformed_url, !https);
    return result;
}

static n00b_module_t *
ctx_init_from_local_file(n00b_compile_ctx *ctx, n00b_string_t *inpath)
{
    n00b_string_t *module;
    n00b_string_t *package;
    n00b_string_t *path = NULL;

    inpath = n00b_resolve_path(inpath);
    n00b_path_strip_slashes_both_ends(inpath);
    int64_t n = n00b_string_rfind(inpath, n00b_cached_slash());

    if (n == -1) {
        module  = inpath;
        path    = n00b_cached_empty_string();
        package = n00b_cached_empty_string();
    }
    else {
        int l;
        l       = n00b_string_codepoint_len(inpath);
        module  = n00b_string_slice(inpath, n + 1, l);
        inpath  = n00b_string_slice(inpath, 0, n);
        l       = n00b_string_codepoint_len(module);
        n       = n00b_string_rfind(module, n00b_cached_period());
        module  = n00b_string_slice(module, 0, n);
        package = n00b_package_from_path_prefix(inpath, &path);

        if (!package) {
            path = inpath;
        }
    }

    n00b_module_t *result = n00b_find_module(ctx,
                                             path,
                                             module,
                                             package,
                                             NULL,
                                             NULL,
                                             NULL);

    return result;
}

static n00b_module_t *
ctx_init_from_module_spec(n00b_compile_ctx *ctx, n00b_string_t *path)
{
    n00b_string_t *package = NULL;
    n00b_string_t *module;
    int64_t        n = n00b_string_rfind(path, n00b_cached_period());

    if (n != -1) {
        package = n00b_string_slice(path, 0, n);
        module  = n00b_string_slice(path, n + 1, -1);
    }
    else {
        module = path;
    }

    return n00b_find_module(ctx, NULL, module, package, NULL, NULL, NULL);
}

static n00b_module_t *
ctx_init_from_package_spec(n00b_compile_ctx *ctx, n00b_string_t *path)
{
    path                  = n00b_resolve_path(path);
    char          *p      = &path->data[n00b_string_byte_len(path) - 1];
    n00b_string_t *module = n00b_cstring(N00B_PACKAGE_INIT_MODULE);
    char          *end_slash;
    n00b_string_t *package;

    // Avoid trailing slashes, including consecutive ones.
    while (p > path->data && *p == '/') {
        --p;
    }

    end_slash = p + 1;

    while (p > path->data) {
        if (*p == '/') {
            break;
        }
        --p;
    }

    // When this is true, the slash we saw was a trailing path, in
    // which case the whole thing is expected to be the package spec.
    if (*p != '/') {
        *end_slash = 0;
        package    = n00b_cstring(path->data);
        *end_slash = '/';
        path       = n00b_cached_empty_string();
    }
    else {
        package = n00b_cstring(p + 1);
        *p      = 0;
        path    = n00b_cstring_copy(path->data);
        *p      = '/';
    }

    return n00b_find_module(ctx, path, module, package, NULL, NULL, NULL);
}

static inline bool
has_n00b_extension(n00b_string_t *s)
{
    n00b_list_t *exts = n00b_get_allowed_file_extensions();

    for (int i = 0; i < n00b_list_len(exts); i++) {
        n00b_string_t *ext = n00b_list_get(exts, i, NULL);
        if (n00b_string_ends_with(s, n00b_cformat(".«#»", ext))) {
            return true;
        }
    }

    return false;
}

n00b_module_t *
n00b_init_module_from_loc(n00b_compile_ctx *ctx, n00b_string_t *path)
{
    // This function is meant for handling a module spec at the
    // command-line, not something via a 'use' statement.
    //
    // At the command line, someone could be trying to run a module
    // either the local directory or via an absolute path, or they
    // could be trying to load a package, in which case we actually
    // want to open the __init module inside that package.
    //
    // In these cases, we do NOT search the n00b path for stuff from
    // the command line.
    //
    // OR, they might specify 'package.module' in which case we *do*
    // want to go ahead and use the N00b path (though, adding the
    // CWD).
    //
    // The key distinguisher is the file extension. So here are our rules:
    //
    // 1. If it starts with http: or https:// it's a URL to either a
    //    package or a module. If it's a module, it'll have a valid
    //    file extension, otherwise it's a package.
    //
    // 2. If we see one of our valid file extensions, then it's
    //    treated as specifying a single n00b file at the path
    //    provided (whether relative or absolute). In this case, we
    //    still might have a package, IF the n00b file lives
    //    somewhere in the module path. But we do not want to take the
    //    path from the command line.
    //
    // 3. If we do NOT see a valid file extension, and there are no
    //    slashes in the spec, then we treat it like a module spec
    //    where we search the path (just like a use statement).
    //
    // 4. If there is no file extension, but there IS a slash, then we
    //    assume it is a spec to a package location. If the location
    //    is in our path somewhere, then we calculate the package
    //    relative to that path. Otherwise, we put the package in the
    //    top-level.  In either case, we ensure there is a __init
    //    file.
    n00b_module_t *result        = NULL;
    bool           has_extension = has_n00b_extension(path);

    if (n00b_string_starts_with(path, n00b_cstring("http:"))) {
        result = ctx_init_from_web_uri(ctx, path, has_extension, false);
    }

    if (!result && n00b_string_starts_with(path, n00b_cstring("https:"))) {
        result = ctx_init_from_web_uri(ctx, path, has_extension, true);
    }

    if (!result && has_extension) {
        result = ctx_init_from_local_file(ctx, path);
    }

    if (!result && n00b_string_find(path, n00b_cached_slash()) == -1) {
        result = ctx_init_from_module_spec(ctx, path);
    }

    if (!result) {
        result = ctx_init_from_package_spec(ctx, path);
    }

    return postprocess_module(ctx, result, path, false, NULL);
}

n00b_string_t *
n00b_format_module_location(n00b_module_t *ctx, n00b_token_t *tok)
{
    if (!ctx->full_uri) {
        ctx->full_uri = n00b_cformat("«#».«#»",
                                     ctx->package,
                                     ctx->name);
    }

    if (!tok) {
        return n00b_cformat("«b»«#»", ctx->full_uri);
    }
    return n00b_cformat("«b»«#»:«#:n»:«#:n»:",
                        ctx->full_uri,
                        (int64_t)tok->line_no,
                        (int64_t)tok->line_offset + 1);
}
