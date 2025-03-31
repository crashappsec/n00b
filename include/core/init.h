#pragma once
#include "n00b.h"

__attribute__((constructor)) void n00b_init(int, char **, char **);

extern char **n00b_stashed_argv;
extern char **n00b_stashed_envp;

static inline char **
n00b_raw_argv(void)
{
    return n00b_stashed_argv;
}

static inline char **
n00b_raw_envp(void)
{
    return n00b_stashed_envp;
}

extern n00b_list_t   *n00b_get_program_arguments(void);
extern n00b_string_t *n00b_get_argv0(void);
extern n00b_string_t *n00b_path_search(n00b_string_t *, n00b_string_t *);
extern n00b_string_t *n00b_n00b_root(void);
extern n00b_string_t *n00b_system_module_path(void);
extern void           n00b_add_static_symbols(void);
extern n00b_list_t   *n00b_path;
extern n00b_dict_t   *n00b_extensions;
extern n00b_list_t   *n00b_get_allowed_file_extensions(void);
extern void           n00b_add_exit_handler(void (*)(void));

static inline n00b_list_t *
n00b_get_module_search_path(void)
{
    return n00b_path;
}
