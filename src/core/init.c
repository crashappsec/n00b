// When n00b is actually used as a library, call this, because the
// constructors are likely to not get called properly.
#define N00B_USE_INTERNAL_API
#include "n00b.h"

char              **n00b_stashed_argv;
static n00b_list_t *exit_handlers           = NULL;
static n00b_dict_t *cached_environment_vars = NULL;

static void
n00b_on_exit(void)
{
    void (*handler)(void) = n00b_list_pop(exit_handlers);

    while (handler) {
        handler();
        handler = n00b_list_pop(exit_handlers);
    }
}

void
n00b_add_exit_handler(void (*handler)(void))
{
    n00b_push_heap(n00b_default_heap);

    if (!exit_handlers) {
        exit_handlers = n00b_list(n00b_type_ref());
        atexit(n00b_on_exit);
    }

    n00b_list_append(exit_handlers, handler);
    n00b_pop_heap();
}

static void
n00b_register_builtins(void)
{
    n00b_add_static_function(n00b_cstring("n00b_clz"), n00b_clz);
    n00b_add_static_function(n00b_cstring("n00b_rand64"), n00b_rand64);
}

n00b_list_t *
n00b_get_allowed_file_extensions(void)
{
    return n00b_dict_keys(n00b_extensions);
}

n00b_list_t *
n00b_get_program_arguments(void)
{
    n00b_list_t *result = n00b_list(n00b_type_string());
    char       **cur    = n00b_stashed_argv + 1; // Skip argv0.

    while (*cur != NULL) {
        n00b_list_append(result, n00b_cstring(*cur));
        cur++;
    }

    return result;
}

n00b_string_t *
n00b_get_argv0(void)
{
    return n00b_cstring(*n00b_stashed_argv);
}

static inline int
find_env_value(char *c, char **next)
{
    char n;
    int  i = 0;

    while ((n = *c++) != 0) {
        if (n == '=') {
            *next = c;
            return i;
        }
        i++;
    }
    *next = 0;
    return 0;
}

extern char **environ;

static void
load_env(n00b_dict_t *environment_vars)
{
    char **ptr = environ;
    char  *item;
    char  *val;
    int    len1;

    while ((item = *ptr++) != NULL) {
        len1 = find_env_value(item, &val);
        if (!len1) {
            continue;
        }
        n00b_string_t *key   = n00b_utf8(item, len1);
        n00b_string_t *value = n00b_cstring(val);

        hatrack_dict_put(environment_vars, key, value);
        n00b_assert(hatrack_dict_get(environment_vars, key, NULL) == value);
    }
    n00b_gc_register_root(&cached_environment_vars, 1);
    cached_environment_vars = environment_vars;
}

n00b_string_t *
n00b_get_env(n00b_string_t *name)
{
    if (cached_environment_vars == NULL) {
        cached_environment_vars = n00b_new(n00b_type_dict(n00b_type_string(),
                                                          n00b_type_string()));
        load_env(cached_environment_vars);
    }

    return hatrack_dict_get(cached_environment_vars, name, NULL);
}

void
n00b_set_env(n00b_string_t *name, n00b_string_t *value)
{
    if (setenv(name->data, value->data, 1)) {
        n00b_raise_errno();
    }
    hatrack_dict_put(cached_environment_vars, name, value);
}

bool
n00b_remove_env(n00b_string_t *name)
{
    bool result = !unsetenv(name->data);

    if (result) {
        hatrack_dict_remove(cached_environment_vars, name);
    }

    return result;
}

n00b_dict_t *
n00b_environment(void)
{
    n00b_dict_t *result = n00b_new(n00b_type_dict(n00b_type_string(),
                                                  n00b_type_string()));

    load_env(result);

    return result;
}

static n00b_string_t *n00b_root       = NULL;
n00b_list_t          *n00b_path       = NULL;
n00b_dict_t          *n00b_extensions = NULL;

n00b_string_t *
n00b_n00b_root(void)
{
    if (n00b_root == NULL) {
        n00b_root = n00b_get_env(n00b_cstring("N00B_ROOT"));

        if (n00b_root == NULL) {
            n00b_string_t *tmp = n00b_resolve_path(n00b_app_path());
            N00B_TRY
            {
                n00b_string_t *tmp2 = n00b_cformat("«#»/../..", tmp);
                n00b_root           = n00b_resolve_path(tmp2);
            }
            N00B_EXCEPT
            {
                n00b_root = n00b_get_current_directory();
            }
            N00B_TRY_END;
        }
        else {
            n00b_root = n00b_resolve_path(n00b_root);
        }
    }

    return n00b_root;
}

n00b_string_t *
n00b_system_module_path(void)
{
    n00b_list_t *l = n00b_list(n00b_type_string());

    n00b_list_append(l, n00b_n00b_root());
    n00b_list_append(l, n00b_cstring("sys"));

    return n00b_path_join(l);
}

static void
n00b_init_path(void)
{
    n00b_list_t *parts;

    // Going to have info on the VM type too.
    n00b_extensions = n00b_dict(n00b_type_string(), n00b_type_int());

    hatrack_dict_add(n00b_extensions, n00b_cstring("n"), 0);
    hatrack_dict_add(n00b_extensions, n00b_cstring("n00b"), 0);

    n00b_string_t *extra = n00b_get_env(n00b_cstring("N00B_SRC_EXTENSIONS"));

    if (extra != NULL) {
        parts = n00b_string_split(extra, n00b_cached_colon());
        for (int i = 0; i < n00b_list_len(parts); i++) {
            hatrack_dict_put(n00b_extensions, n00b_list_get(parts, i, NULL), 0);
        }
    }

    n00b_set_package_search_path(n00b_system_module_path(),
                                 n00b_resolve_path(n00b_cached_period()));

    extra = n00b_get_env(n00b_cstring("N00B_PATH"));

    if (extra == NULL) {
        n00b_list_append(n00b_path, n00b_n00b_root());
        return;
    }

    parts = n00b_string_split(extra, n00b_cached_colon());

    // Always keep sys and cwd in the path; sys is first, . last.
    n00b_list_t *new_path = n00b_list(n00b_type_string());

    n00b_list_append(new_path, n00b_system_module_path());

    for (int i = 0; i < n00b_list_len(parts); i++) {
        n00b_string_t *s = n00b_list_get(parts, i, NULL);

        n00b_list_append(n00b_path, n00b_resolve_path(s));
    }

    n00b_list_append(new_path, n00b_resolve_path(n00b_cached_period()));

    n00b_path = new_path;
}

n00b_string_t *
n00b_path_search(n00b_string_t *package, n00b_string_t *module)
{
    uint64_t        n_items;
    n00b_string_t  *munged     = NULL;
    n00b_string_t **extensions = (void *)hatrack_dict_keys_sort(n00b_extensions,
                                                                &n_items);

    if (package != NULL && n00b_string_codepoint_len(package) != 0) {
        n00b_list_t *parts = n00b_string_split(package, n00b_cached_period());

        n00b_list_append(parts, module);
        munged = n00b_string_join(parts, n00b_cached_slash());
    }

    int l = (int)n00b_list_len(n00b_path);

    if (munged != NULL) {
        for (int i = 0; i < l; i++) {
            n00b_string_t *dir = n00b_list_get(n00b_path, i, NULL);

            for (int j = 0; j < (int)n_items; j++) {
                n00b_string_t *ext = extensions[j];
                n00b_string_t *s   = n00b_cformat("«#»/«#».«#»",
                                                dir,
                                                munged,
                                                ext);

                struct stat info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }
    else {
        for (int i = 0; i < l; i++) {
            n00b_string_t *dir = n00b_list_get(n00b_path, i, NULL);
            for (int j = 0; j < (int)n_items; j++) {
                n00b_string_t *ext = extensions[j];
                n00b_string_t *s   = n00b_cformat("«#»/«#».«#»",
                                                dir,
                                                module,
                                                ext);
                struct stat    info;

                if (!stat(s->data, &info)) {
                    return s;
                }
            }
        }
    }

    return NULL;
}

void
_n00b_set_package_search_path(n00b_string_t *dir, ...)
{
    n00b_path = n00b_list(n00b_type_string());

    va_list args;

    va_start(args, dir);

    while (dir != NULL) {
        n00b_list_append(n00b_path, dir);
        dir = va_arg(args, n00b_string_t *);
    }

    va_end(args);
}

#ifdef N00B_STATIC_FFI_BINDING
#define FSTAT(x) n00b_add_static_function(n00b_cstring(#x), x)
#else
#define FSTAT(x)
#endif

void
n00b_add_static_symbols(void)
{
    FSTAT(n00b_list_append);
    FSTAT(n00b_wrapper_join);
    FSTAT(n00b_string_upper);
    FSTAT(n00b_string_lower);
    FSTAT(n00b_string_split);
    FSTAT(n00b_string_pad);
    FSTAT(n00b_wrapper_hostname);
    FSTAT(n00b_wrapper_os);
    FSTAT(n00b_wrapper_arch);
    FSTAT(n00b_wrapper_repr);
    FSTAT(n00b_wrapper_to_str);
    FSTAT(n00b_len);
    FSTAT(n00b_now);
    FSTAT(n00b_timestamp);
    FSTAT(n00b_process_cpu);
    FSTAT(n00b_thread_cpu);
    FSTAT(n00b_uptime);
    FSTAT(n00b_program_clock);
    FSTAT(n00b_copy);
    FSTAT(n00b_get_c_backtrace);
    FSTAT(n00b_lookup_color);
    FSTAT(n00b_to_vga);
    FSTAT(n00b_read_utf8_file);
    FSTAT(n00b_read_binary_file);
    FSTAT(n00b_list_resize);
    FSTAT(n00b_list_append);
    FSTAT(n00b_list_sort);
    FSTAT(n00b_list_pop);
    FSTAT(n00b_list_contains);
}

extern void      n00b_fd_init_io(void);
struct timespec *n00b_io_duration = NULL;

static void *
start_io(void *ignore)
{
    n00b_fd_init_io();
    n00b_fd_run_evloop(n00b_system_dispatcher, n00b_io_duration);
    return NULL;
}

static void
n00b_initialize_library(void)
{
    n00b_init_program_timestamp();
    n00b_thread_spawn((void *)start_io, NULL);
}

extern void n00b_crash_init(void);

bool n00b_startup_complete = false;

__attribute__((constructor)) void
n00b_init(int argc, char **argv, char **envp)
{
    // musl does not pass argv to attribute constructors
    // https://www.openwall.com/lists/musl/2020/07/15/1
    if (argv == NULL) {
        return;
    }

    static int inited = false;

    if (!inited) {
        inited            = true;
        n00b_stashed_argv = argv;

        // We need some basic thread data before starting the GC, but
        // the thread setup for thread initialization that uses the heap
        // has to come after the GC is initialized. Therefore, sandwich
        // the GC startup with thread stuff.
        n00b_threading_setup(); // thread.c
        n00b_initialize_gc();
        n00b_finish_main_thread_initialization();

        n00b_gc_register_root(&n00b_root, 1);
        n00b_gc_register_root(&n00b_path, 1);
        n00b_gc_register_root(&n00b_extensions, 1);
        n00b_gc_register_root(&cached_environment_vars, 1);
        n00b_gc_register_root(&mmm_free_tids, sizeof(mmm_free_tids) / 8);
        n00b_initialize_global_types();
        n00b_init_common_string_cache();
        n00b_backtrace_init(n00b_stashed_argv[0]);
        n00b_gc_set_system_finalizer((void *)n00b_finalize_allocation);
        n00b_setup_signals();
        n00b_crash_init();
        n00b_register_builtins();
        n00b_init_path();
        n00b_theme_initialization();
        n00b_assertion_init();
        n00b_long_term_pin(n00b_internal_heap);
        // n00b_internal_io_setup();
        n00b_initialize_library();

        n00b_startup_complete = true;
    }
}
