#pragma once

#define __n00b_decl(name)                   \
    int  n00b_current_##name##_limit(void); \
    int  n00b_max_##name##_limit(void);     \
    bool n00b_set_##name##_limit(int);

__n00b_decl(fd);
__n00b_decl(cpu);
__n00b_decl(core_size);
__n00b_decl(file_size);
__n00b_decl(num_procs);
__n00b_decl(memlock);

#if defined(__linux__)
__n00b_decl(nice);
#endif

#undef __n00b_decl
