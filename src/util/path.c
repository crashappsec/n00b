#define N00B_USE_INTERNAL_API

#include "n00b.h"

n00b_utf8_t *
n00b_get_current_directory()
{
    char buf[MAXPATHLEN + 1];

    return n00b_new_utf8(getcwd(buf, MAXPATHLEN));
}

// This is private; it mutates the string, which we don't normally
// want to support, and only do so because we know it's all private.
static n00b_utf8_t *
remove_extra_slashes(n00b_utf8_t *result)
{
    int i = result->codepoints;

    while (result->data[--i] == '/') {
        result->data[i] = 0;
        result->codepoints--;
    }

    return result;
}

n00b_utf8_t *
n00b_get_user_dir(n00b_utf8_t *user)
{
    n00b_utf8_t   *result;
    struct passwd *pw;

    if (user == NULL) {
        result = n00b_get_env(n00b_new_utf8("HOME"));
        if (!result) {
            pw = getpwent();
            if (pw == NULL) {
                result = n00b_new_utf8("/");
            }
            else {
                result = n00b_new_utf8(pw->pw_dir);
            }
        }
    }
    else {
        pw = getpwnam(user->data);
        if (pw == NULL) {
            result = user;
        }
        result = n00b_new_utf8(pw->pw_dir);
    }

    return remove_extra_slashes(result);
}

static n00b_utf8_t *
internal_normalize_and_join(n00b_list_t *pieces)
{
    int partlen = n00b_list_len(pieces);
    int nextout = 0;

    for (int i = 0; i < partlen; i++) {
        n00b_utf8_t *s = n00b_to_utf8(n00b_list_get(pieces, i, NULL));

        if (s->codepoints == 0) {
            continue;
        }

        if (s->data[0] == '.') {
            switch (s->codepoints) {
            case 1:
                continue;
            case 2:
                if (s->data[1] == '.') {
                    --nextout;
                    continue;
                }
                break;
            default:
                break;
            }
        }

        if (nextout < 0) {
            N00B_CRAISE("Invalid path; backs up past the root directory.");
        }

        n00b_list_set(pieces, nextout++, s);
    }

    if (nextout == 0) {
        return n00b_get_slash_const();
    }

    n00b_utf8_t *result = NULL;

    for (int i = 0; i < nextout; i++) {
        n00b_utf8_t *s = n00b_list_get(pieces, i, NULL);

        if (!s->codepoints) {
            continue;
        }

        if (!result) {
            result = n00b_cstr_format("/{}", s);
        }
        else {
            result = n00b_cstr_format("{}/{}", result, s);
        }
    }

    return n00b_to_utf8(result);
}

static n00b_list_t *
raw_path_tilde_expand(n00b_utf8_t *in)
{
    if (!in || !in->codepoints) {
        in = n00b_get_slash_const();
    }

    if (in->data[0] != '~') {
        return n00b_str_split(in, n00b_get_slash_const());
    }

    n00b_list_t *parts = n00b_str_split(in, n00b_get_slash_const());
    n00b_utf8_t *home  = n00b_to_utf8(n00b_list_get(parts, 0, NULL));

    if (n00b_str_codepoint_len(home) == 1) {
        n00b_list_set(parts, 0, n00b_empty_string());
        parts = n00b_list_plus(n00b_str_split(n00b_get_user_dir(NULL),
                                              n00b_get_slash_const()),
                               parts);
    }
    else {
        home->data++;
        n00b_list_set(parts, 0, n00b_empty_string());
        parts = n00b_list_plus(n00b_str_split(n00b_get_user_dir(home),
                                              n00b_get_slash_const()),
                               parts);
        home->data--;
    }

    return parts;
}

n00b_utf8_t *
n00b_path_tilde_expand(n00b_utf8_t *in)
{
    return internal_normalize_and_join(raw_path_tilde_expand(in));
}

n00b_utf8_t *
n00b_resolve_path(n00b_utf8_t *s)
{
    n00b_list_t *parts;

    if (s == NULL || s->codepoints == 0) {
        return n00b_get_home_directory();
    }

    switch (s->data[0]) {
    case '~':
        return n00b_path_tilde_expand(s);
    case '/':
        return internal_normalize_and_join(
            n00b_str_split(s, n00b_get_slash_const()));
    default:
        parts = n00b_str_split(n00b_get_current_directory(),
                               n00b_get_slash_const());
        n00b_list_plus_eq(parts, n00b_str_split(s, n00b_get_slash_const()));
        return internal_normalize_and_join(parts);
    }
}

n00b_utf8_t *
n00b_path_join(n00b_list_t *items)
{
    n00b_utf8_t *result;
    n00b_utf8_t *tmp;
    uint8_t     *p;
    int          len   = 0; // Total length of output.
    int          first = 0; // First array index we'll use.
    int          last  = n00b_list_len(items);
    int          tmplen;    // Length of individual strings.

    for (int i = 0; i < last; i++) {
        tmp = n00b_list_get(items, i, NULL);
        if (!n00b_str_is_u8(tmp)) {
            N00B_CRAISE("Strings passed to n00b_path_join must be utf8 encoded.");
        }

        tmplen = strlen(tmp->data);

        if (tmplen == 0) {
            continue;
        }
        if (tmp->data[0] == '/') {
            len   = tmplen;
            first = i;
        }
        else {
            len += tmplen;
        }
        if (i + 1 != last && tmp->data[tmplen - 1] != '/') {
            len++;
        }
    }

    result = n00b_new(n00b_type_utf8(), n00b_kw("length", n00b_ka(len)));
    p      = (uint8_t *)result->data;

    for (int i = first; i < last; i++) {
        tmp    = n00b_list_get(items, i, NULL);
        tmplen = strlen(tmp->data);

        if (tmplen == 0) {
            continue;
        }

        memcpy(p, tmp->data, tmplen);
        p += tmplen;

        if (i + 1 != last && tmp->data[tmplen - 1] != '/') {
            *p++ = '/';
        }
    }

    result->byte_len = len;
    n00b_internal_utf8_set_codepoint_count(result);

    return result;
}

n00b_file_kind
n00b_get_file_kind(n00b_utf8_t *p)
{
    struct stat file_info;
    p = n00b_resolve_path(p);
    if (lstat(p->data, &file_info) != 0) {
        return N00B_FK_NOT_FOUND;
    }

    switch (file_info.st_mode & S_IFMT) {
    case S_IFREG:
        return N00B_FK_IS_REG_FILE;
    case S_IFDIR:
        return N00B_FK_IS_DIR;
    case S_IFSOCK:
        return N00B_FK_IS_SOCK;
    case S_IFCHR:
        return N00B_FK_IS_CHR_DEVICE;
    case S_IFBLK:
        return N00B_FK_IS_BLOCK_DEVICE;
    case S_IFIFO:
        return N00B_FK_IS_FIFO;
    case S_IFLNK:
        if (stat(p->data, &file_info) != 0) {
            return N00B_FK_NOT_FOUND;
        }
        switch (file_info.st_mode & S_IFMT) {
        case S_IFREG:
            return N00B_FK_IS_FLINK;
        case S_IFDIR:
            return N00B_FK_IS_DLINK;
        default:
            return N00B_FK_OTHER;
        }
    default:
        return N00B_FK_OTHER;
    }
}

typedef struct {
    n00b_utf8_t *sc_proc;
    n00b_utf8_t *sc_dev;
    n00b_utf8_t *sc_cwd;
    n00b_utf8_t *sc_up;
    n00b_list_t *result;
    n00b_utf8_t *resolved;
    bool         recurse;
    bool         yield_links;
    bool         yield_dirs;
    bool         follow_links;
    bool         ignore_special;
    bool         done_with_safety_checks;
    bool         have_recursed;
} n00b_walk_ctx;

static n00b_utf8_t *
add_slash_if_needed(n00b_utf8_t *s)
{
    n00b_utf8_t *result;

    if (n00b_index(s, n00b_str_codepoint_len(s) - 1) == '/') {
        result = s;
    }
    else {
        result = n00b_cstr_format("{}/", s);
    }

    return result;
}

static void
internal_path_walk(n00b_walk_ctx *ctx)
{
    DIR           *dirobj;
    struct dirent *entry;
    n00b_utf8_t   *saved;
    struct stat    file_info;

    if (!ctx->done_with_safety_checks) {
        if (n00b_str_starts_with(ctx->resolved, ctx->sc_proc)) {
            return;
        }
        if (n00b_str_starts_with(ctx->resolved, ctx->sc_dev)) {
            return;
        }
        if (n00b_str_codepoint_len(ctx->resolved) != 1) {
            ctx->done_with_safety_checks = true;
        }
    }

    if (lstat(ctx->resolved->data, &file_info) != 0) {
        return;
    }

    switch (file_info.st_mode & S_IFMT) {
    case S_IFREG:
        n00b_list_append(ctx->result, ctx->resolved);
        return;

    case S_IFDIR:
        if (ctx->yield_dirs) {
            n00b_list_append(ctx->result, ctx->resolved);
            return;
        }

actual_directory:
        if (!ctx->recurse) {
            if (ctx->have_recursed) {
                return;
            }
            ctx->have_recursed = true;
        }

        ctx->resolved = add_slash_if_needed(ctx->resolved);

        dirobj = opendir(ctx->resolved->data);

        if (dirobj == NULL) {
            return;
        }

        saved = ctx->resolved;

        while (true) {
            entry = readdir(dirobj);

            if (entry == NULL) {
                ctx->resolved = saved;
                return;
            }

            if (!strcmp(entry->d_name, "..")) {
                continue;
            }

            if (!strcmp(entry->d_name, ".")) {
                continue;
            }

            ctx->resolved = n00b_to_utf8(
                n00b_str_concat(saved, n00b_new_utf8(entry->d_name)));

            internal_path_walk(ctx);
        }
        ctx->resolved = saved;
        return;

    case S_IFLNK:
        if (stat(ctx->resolved->data, &file_info) != 0) {
            return;
        }

        switch (file_info.st_mode & S_IFMT) {
        case S_IFREG:
            if (ctx->follow_links && ctx->yield_links) {
                char buf[PATH_MAX + 1] = {
                    0,
                };

                int n = readlink(ctx->resolved->data, buf, PATH_MAX);

                if (n == -1) {
                    N00B_CRAISE("readlink() failed.");
                }

                ctx->resolved->data[n] = 0;

                n00b_list_append(ctx->result,
                                 n00b_resolve_path(n00b_new_utf8(buf)));
            }
            else {
                if (ctx->yield_links) {
                    n00b_list_append(ctx->result, ctx->resolved);
                }
            }
            return;
        case S_IFDIR:

            if (ctx->yield_dirs && ctx->yield_links) {
                n00b_list_append(ctx->result, ctx->resolved);
            }

            if (!ctx->follow_links || !ctx->recurse) {
                return;
            }

            saved                  = ctx->resolved;
            char buf[PATH_MAX + 1] = {
                0,
            };
            int n = readlink(ctx->resolved->data, buf, PATH_MAX);

            if (n == -1) {
                N00B_CRAISE("readlink() failed.");
            }

            ctx->resolved->data[n] = 0;

            ctx->resolved = n00b_resolve_path(n00b_new_utf8(buf));
            if (ctx->yield_dirs && !ctx->yield_links) {
                n00b_list_append(ctx->result, ctx->resolved);
            }

            goto actual_directory;

        default:
            if (!ctx->ignore_special) {
                n00b_list_append(ctx->result, ctx->resolved);
            }
            return;
        }
    case S_IFSOCK:
    case S_IFCHR:
    case S_IFBLK:
    case S_IFIFO:
    default:
        if (!ctx->ignore_special) {
            n00b_list_append(ctx->result, ctx->resolved);
        }
        return;
    }
}

#ifdef __linux__
n00b_utf8_t *
n00b_app_path()
{
    char buf[PATH_MAX];
    char proc_path[PATH_MAX];
    snprintf(proc_path, PATH_MAX, "/proc/%d/exe", getpid());
    buf[readlink(proc_path, buf, PATH_MAX)] = 0;

    return n00b_new_utf8(buf);
}
#elif defined(__MACH__)
n00b_utf8_t *
n00b_app_path()
{
    char buf[PROC_PIDPATHINFO_MAXSIZE];
    proc_pidpath(getpid(), buf, PROC_PIDPATHINFO_MAXSIZE);

    return n00b_new_utf8(buf);
}
#else
#error "Unsupported platform"
#endif

n00b_list_t *
_n00b_path_walk(n00b_utf8_t *dir, ...)
{
    bool recurse        = true;
    bool yield_links    = false;
    bool yield_dirs     = false;
    bool ignore_special = true;
    bool follow_links   = false;

    n00b_karg_only_init(dir);
    n00b_kw_bool("recurse", recurse);
    n00b_kw_bool("yield_links", yield_links);
    n00b_kw_bool("yield_dirs", yield_dirs);
    n00b_kw_bool("follow_links", follow_links);
    n00b_kw_bool("ignore_special", ignore_special);

    n00b_walk_ctx ctx = {
        .sc_proc                 = n00b_new_utf8("/proc/"),
        .sc_dev                  = n00b_new_utf8("/dev/"),
        .sc_cwd                  = n00b_new_utf8("."),
        .sc_up                   = n00b_new_utf8(".."),
        .recurse                 = recurse,
        .yield_links             = yield_links,
        .yield_dirs              = yield_dirs,
        .follow_links            = follow_links,
        .ignore_special          = ignore_special,
        .done_with_safety_checks = false,
        .have_recursed           = false,
        .result                  = n00b_list(n00b_type_utf8()),
        .resolved                = n00b_resolve_path(dir),
    };

    internal_path_walk(&ctx);

    return ctx.result;
}

n00b_utf8_t *
n00b_path_trim_slashes(n00b_str_t *s)
{
    if (!n00b_str_codepoint_len(s)) {
        return s;
    }

    n00b_utf8_t *n     = n00b_to_utf8(s);
    int          b_len = n00b_str_byte_len(n);

    if (n->data[--b_len] != '/') {
        return n;
    }

    if (n == s) {
        n = n00b_str_copy(s);
    }

    do {
        n->data[b_len] = 0;
        n->codepoints--;
        n->byte_len--;
    } while (b_len && n->data[--b_len] == '/');

    return n;
}
