#define N00B_USE_INTERNAL_API

#include "n00b.h"

n00b_string_t *
n00b_get_current_directory(void)
{
    char buf[MAXPATHLEN + 1];

    return n00b_cstring(getcwd(buf, MAXPATHLEN));
}

static n00b_string_t *n00b_base_tmp_dir;
static pthread_once_t tmpdir_setup = PTHREAD_ONCE_INIT;

static void
n00b_acquire_base_tmp_dir(void)
{
    n00b_gc_register_root(&n00b_base_tmp_dir, 1);
    if ((n00b_base_tmp_dir = n00b_get_env(n00b_cstring("TMPDIR")))) {
        return;
    }
    if ((n00b_base_tmp_dir = n00b_get_env(n00b_cstring("TMP")))) {
        return;
    }
    if ((n00b_base_tmp_dir = n00b_get_env(n00b_cstring("TEMP")))) {
        return;
    }
    n00b_base_tmp_dir = n00b_cstring("/tmp/");
}

static inline n00b_string_t *
construct_random_name(n00b_string_t *prefix, n00b_string_t *suffix)
{
    pthread_once(&tmpdir_setup, n00b_acquire_base_tmp_dir);

    uint64_t       bytes         = n00b_rand64();
    n00b_string_t *random_string = n00b_bytes_to_hex((char *)&bytes, 8);

    if (prefix || suffix) {
        if (!prefix) {
            prefix = n00b_cached_empty_string();
        }
        if (!suffix) {
            suffix = n00b_cached_empty_string();
        }
        random_string = n00b_cformat("[|#|][|#|][|#|]",
                                     prefix,
                                     random_string,
                                     suffix);
    }

    return n00b_path_simple_join(n00b_base_tmp_dir, random_string);
}

n00b_string_t *
n00b_new_temp_dir(n00b_string_t *prefix, n00b_string_t *suffix)
{
    n00b_string_t *dirname = construct_random_name(prefix, suffix);
    if (mkdir(dirname->data, 0774)) {
        n00b_raise_errno();
    }

    return dirname;
}

n00b_string_t *
n00b_get_temp_root(void)
{
    pthread_once(&tmpdir_setup, n00b_acquire_base_tmp_dir);
    return n00b_base_tmp_dir;
}

void *
n00b_tempfile(n00b_string_t *prefix, n00b_string_t *suffix)
{
    n00b_string_t *filename = construct_random_name(prefix, suffix);
    return n00b_new(n00b_type_file(),
                    filename,
                    n00b_fm_rw,
                    n00b_kw("allow_file_creation", n00b_ka(true)));
}

// This is private; it mutates the string, which we don't normally
// want to support, and only do so because we know it's all private.
static n00b_string_t *
remove_extra_slashes(n00b_string_t *result)
{
    int i = result->codepoints;

    while (result->data[--i] == '/') {
        result->data[i] = 0;
        result->codepoints--;
    }

    return result;
}

n00b_string_t *
n00b_get_user_dir(n00b_string_t *user)
{
    n00b_string_t *result;
    struct passwd *pw;

    if (user == NULL) {
        result = n00b_get_env(n00b_cstring("HOME"));
        if (!result) {
            pw = getpwent();
            if (pw == NULL) {
                result = n00b_cached_slash();
            }
            else {
                result = n00b_cstring(pw->pw_dir);
            }
        }
    }
    else {
        pw = getpwnam(user->data);
        if (pw == NULL) {
            result = user;
        }
        result = n00b_cstring(pw->pw_dir);
    }

    return remove_extra_slashes(result);
}

static n00b_string_t *
internal_normalize_and_join(n00b_list_t *pieces)
{
    int partlen = n00b_list_len(pieces);
    int nextout = 0;

    for (int i = 0; i < partlen; i++) {
        n00b_string_t *s = n00b_list_get(pieces, i, NULL);

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
        return n00b_cached_slash();
    }

    n00b_string_t *result = NULL;

    for (int i = 0; i < nextout; i++) {
        n00b_string_t *s = n00b_list_get(pieces, i, NULL);

        if (!s->codepoints) {
            continue;
        }

        if (!result) {
            result = n00b_cformat("/«#»", s);
        }
        else {
            result = n00b_cformat("«#»/«#»", result, s);
        }
    }

    return result;
}

static n00b_list_t *
raw_path_tilde_expand(n00b_string_t *in)
{
    if (!in || !in->codepoints) {
        in = n00b_cached_slash();
    }

    if (in->data[0] != '~') {
        return n00b_string_split(in, n00b_cached_slash());
    }

    n00b_list_t   *parts = n00b_string_split(in, n00b_cached_slash());
    n00b_string_t *home  = n00b_list_get(parts, 0, NULL);

    if (n00b_string_codepoint_len(home) == 1) {
        n00b_list_set(parts, 0, n00b_cached_empty_string());
        parts = n00b_list_plus(n00b_string_split(n00b_get_user_dir(NULL),
                                                 n00b_cached_slash()),
                               parts);
    }
    else {
        home->data++;
        n00b_list_set(parts, 0, n00b_cached_empty_string());
        parts = n00b_list_plus(n00b_string_split(n00b_get_user_dir(home),
                                                 n00b_cached_slash()),
                               parts);
        home->data--;
    }

    return parts;
}

n00b_string_t *
n00b_path_tilde_expand(n00b_string_t *in)
{
    return internal_normalize_and_join(raw_path_tilde_expand(in));
}

n00b_string_t *
n00b_resolve_path(n00b_string_t *s)
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
            n00b_string_split(s, n00b_cached_slash()));
    default:
        parts = n00b_string_split(n00b_get_current_directory(),
                                  n00b_cached_slash());
        n00b_list_plus_eq(parts, n00b_string_split(s, n00b_cached_slash()));
        return internal_normalize_and_join(parts);
    }
}

n00b_string_t *
n00b_path_join(n00b_list_t *items)
{
    n00b_string_t *result;
    n00b_string_t *tmp;
    uint8_t       *p;
    int            len   = 0; // Total length of output.
    int            first = 0; // First array index we'll use.
    int            last  = n00b_list_len(items);
    int            tmplen;    // Length of individual strings.

    for (int i = 0; i < last; i++) {
        tmp = n00b_list_get(items, i, NULL);
        n00b_string_sanity_check(tmp);

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

    result = n00b_alloc_utf8_to_copy(len);
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

    n00b_string_set_codepoint_count(result);

    return result;
}

n00b_file_kind
n00b_get_file_kind(n00b_string_t *p)
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
    n00b_string_t *sc_proc;
    n00b_string_t *sc_dev;
    n00b_string_t *sc_cwd;
    n00b_string_t *sc_up;
    n00b_list_t   *result;
    n00b_string_t *resolved;
    bool           recurse;
    bool           yield_links;
    bool           yield_dirs;
    bool           follow_links;
    bool           ignore_special;
    bool           done_with_safety_checks;
    bool           have_recursed;
} n00b_walk_ctx;

static n00b_string_t *
add_slash_if_needed(n00b_string_t *s)
{
    n00b_string_t *result;

    if (n00b_string_index(s, n00b_string_codepoint_len(s) - 1) == '/') {
        result = s;
    }
    else {
        result = n00b_cformat("«#»/", s);
    }

    return result;
}

static void
internal_path_walk(n00b_walk_ctx *ctx)
{
    DIR           *dirobj;
    struct dirent *entry;
    n00b_string_t *saved;
    struct stat    file_info;

    if (!ctx->done_with_safety_checks) {
        if (n00b_string_starts_with(ctx->resolved, ctx->sc_proc)) {
            return;
        }
        if (n00b_string_starts_with(ctx->resolved, ctx->sc_dev)) {
            return;
        }
        if (n00b_string_codepoint_len(ctx->resolved) != 1) {
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

            ctx->resolved = n00b_string_concat(saved,
                                               n00b_cstring(entry->d_name));

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
                                 n00b_resolve_path(n00b_cstring(buf)));
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

            ctx->resolved = n00b_resolve_path(n00b_cstring(buf));
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
n00b_string_t *
n00b_app_path(void)
{
    char buf[PATH_MAX];
    char proc_path[PATH_MAX];
    snprintf(proc_path, PATH_MAX, "/proc/%d/exe", getpid());
    buf[readlink(proc_path, buf, PATH_MAX)] = 0;

    return n00b_resolve_path(n00b_cstring(buf));
}
#elif defined(__MACH__)
n00b_string_t *
n00b_app_path(void)
{
    char buf[PROC_PIDPATHINFO_MAXSIZE];
    proc_pidpath(getpid(), buf, PROC_PIDPATHINFO_MAXSIZE);

    return n00b_resolve_path(n00b_cstring(buf));
}
#else
#error "Unsupported platform"
#endif

n00b_list_t *
_n00b_path_walk(n00b_string_t *dir, ...)
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
        .sc_proc                 = n00b_cstring("/proc/"),
        .sc_dev                  = n00b_cstring("/dev/"),
        .sc_cwd                  = n00b_cached_period(),
        .sc_up                   = n00b_cstring(".."),
        .recurse                 = recurse,
        .yield_links             = yield_links,
        .yield_dirs              = yield_dirs,
        .follow_links            = follow_links,
        .ignore_special          = ignore_special,
        .done_with_safety_checks = false,
        .have_recursed           = false,
        .result                  = n00b_list(n00b_type_string()),
        .resolved                = n00b_resolve_path(dir),
    };

    internal_path_walk(&ctx);

    return ctx.result;
}

n00b_string_t *
n00b_path_trim_trailing_slashes(n00b_string_t *s)
{
    int b_len = n00b_string_byte_len(s);

    if (!b_len) {
        return s;
    }

    if (s->data[--b_len] != '/') {
        return s;
    }

    s = n00b_string_copy_text(s);

    do {
        s->data[b_len] = 0;
        s->codepoints--;
        s->u8_bytes--;
    } while (b_len && s->data[--b_len] == '/');

    return s;
}

void
n00b_path_strip_slashes_both_ends(n00b_string_t *s)
{
    // Strips in place (it's internal).
    while (s->u8_bytes && s->data[0] == '/') {
        s->data++;
        s->u8_bytes--;
        s->codepoints--;
    }

    while (s->u8_bytes && s->data[s->u8_bytes - 1] == '/') {
        s->data[--s->u8_bytes] = 0;
        s->codepoints--;
    }
}

n00b_string_t *
n00b_filename_from_path(n00b_string_t *s)
{
    int64_t n = n00b_string_find(s, n00b_cached_slash());

    if (n == -1) {
        return s;
    }

    n00b_string_t *news = n00b_resolve_path(s);

    if (news == s) {
        s = n00b_string_copy_text(s);
    }
    else {
        s = news;
    }

    n00b_path_strip_slashes_both_ends(s);

    n = n00b_string_rfind(s, n00b_cached_slash());
    if (n == -1) {
        return s;
    }

    return n00b_string_slice(s, n + 1, -1);
}

n00b_list_t *
n00b_find_file_in_program_path(n00b_string_t *cmd, n00b_list_t *path_list)
{
    // This is a building block for find_command_paths. It simply checks
    // for existance of a file or symlink to a file.

    n00b_list_t *result = n00b_list(n00b_type_string());

    if (!path_list) {
        path_list = n00b_get_program_search_path();
    }

    cmd = n00b_filename_from_path(cmd);

    int n = n00b_list_len(path_list);

    for (int i = 0; i < n; i++) {
        n00b_string_t *dir       = n00b_list_get(path_list, i, NULL);
        dir                      = n00b_resolve_path(dir);
        n00b_string_t *full_path = n00b_path_simple_join(dir, cmd);

        switch (n00b_get_file_kind(full_path)) {
        case N00B_FK_IS_REG_FILE:
        case N00B_FK_IS_FLINK:
            n00b_private_list_append(result, full_path);
            continue;
        default:
            continue;
        }
    }

    return result;
}

n00b_list_t *
n00b_find_command_paths(n00b_string_t *cmd,
                        n00b_list_t   *path_list,
                        bool           self_ok)
{
    // Returns all executables in the given path that we have permission
    // to execute. If self_ok is 'false' then this function will not
    // return the current executable.
    //
    // Note that, as with any file system API stuff, without holding a
    // file descriptor, we cannot escape possible race conditions; we
    // can only comment on what file we saw and what its perms were at
    // the time we checked.
    //
    // We should eventually do that anywhere that supports execveat()

    n00b_list_t   *result     = n00b_find_file_in_program_path(cmd, path_list);
    int            n          = n00b_list_len(result);
    n00b_string_t *my_path    = NULL;
    uid_t          my_euid    = geteuid();
    int            num_groups = -1;
    gid_t          groups[NGROUPS_MAX];

    if (self_ok) {
        my_path = n00b_app_path();
    }

    while (n--) {
        n00b_string_t *path = n00b_private_list_get(result, n, NULL);
        // Don't return ourselves if self_ok is false.
        if (self_ok && n00b_string_eq(path, my_path)) {
            n00b_private_list_remove(result, n);
            continue;
        }

        struct stat file_info;

        // It's possible there was a race and the item got deleted.

        if (stat(path->data, &file_info) != 0) {
            n00b_private_list_remove(result, n);
            continue;
        }

        int exe_bits = file_info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);

        if (!exe_bits) {
            n00b_private_list_remove(result, n);
            continue;
        }

        if (exe_bits & S_IXOTH) {
            continue;
        }

        if ((exe_bits & S_IXUSR) && file_info.st_uid == my_euid) {
            continue;
        }

        if (num_groups < 0) {
            num_groups = getgroups(NGROUPS_MAX, groups);
        }

        gid_t program_gid = file_info.st_gid;

        for (int i = 0; i < num_groups; i++) {
            if (program_gid == groups[i]) {
                goto on_success;
            }
        }
        n00b_private_list_remove(result, n);
on_success:
        continue;
    }

    return result;
}

n00b_string_t *
n00b_rename(n00b_string_t *from, n00b_string_t *to)
{
    from = n00b_resolve_path(from);
    to   = n00b_resolve_path(to);

    if (!n00b_file_exists(from)) {
        N00B_CRAISE("Source file cannot be found");
    }
    if (n00b_file_exists(to)) {
        n00b_list_t *parts = n00b_path_parts(to);

        n00b_eprint(parts);

        n00b_string_t *ext  = n00b_list_pop(parts);
        n00b_string_t *base = n00b_path_join(parts);
        int            i    = 0;

        if (n00b_string_codepoint_len(ext)) {
            ext = n00b_string_concat(n00b_cached_period(), ext);
        }
        do {
            to = n00b_cformat("[|#|].[|#|][|#|]", base, ++i, ext);
        } while (n00b_file_exists(to));
    }

    if (rename(from->data, to->data)) {
        n00b_raise_errno();
    }

    return to;
}
