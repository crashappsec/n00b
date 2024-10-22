/*
 * Copyright © 2021-2022 John Viega
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  Name:           malloc.c
 *  Description:    Memory allocation support
 *
 *  Author:         John Viega, john@zork.org
 */

#include "hatrack/malloc.h"

#include <string.h>

// For most platforms we want to reference posix_memalign and free as weak
// imports. This is why we do not include the header to get them and we include
// __attribute__((weak)) to get the desired effect.
extern int  posix_memalign(void **, size_t, size_t) __attribute__((weak));
extern void free(void *) __attribute__((weak));

static void *
hatrack_default_malloc(size_t size, void *arg HATRACK_LOC_DECL)
{
    void *ptr;
    if (posix_memalign(&ptr, 16, size) != 0) {
        return NULL;
    }
    return ptr;
}

static void *
hatrack_default_zalloc(size_t size, void *arg HATRACK_LOC_DECL)
{
    void *ptr;
    if (posix_memalign(&ptr, 16, size) != 0) {
        return NULL;
    }

    return memset(ptr, 0, size);
}

static void *
hatrack_default_realloc(void  *oldptr,
                        size_t oldsize,
                        size_t newsize,
                        void *arg
                            HATRACK_LOC_DECL)
{
    // This is not the most efficient implementation of realloc, but the whole
    // realloc thing isn't all that efficient in the first place. This ignores
    // the possibility that the original allocation may be large enough to
    // satisfy the new allocation in the case where newsize > oldsize. There
    // is no libc realloc function that allows specifying the desired alignment,
    // so generally the best offered is sizeof(void *).

    void *newptr;
    if (posix_memalign(&newptr, 16, newsize) != 0) {
        return NULL;
    }

    if (newsize > oldsize) {
        memcpy(newptr, oldptr, oldsize);
    }
    else {
        memcpy(newptr, oldptr, newsize);
    }

    free(oldptr);
    return newptr;
}

static void
hatrack_default_free(void *oldptr, size_t oldsize, void *arg HATRACK_LOC_DECL)
{
    free(oldptr);
}

static hatrack_malloc_t  hatrack_malloc_fn  = hatrack_default_malloc;
static hatrack_malloc_t  hatrack_zalloc_fn  = hatrack_default_zalloc;
static hatrack_realloc_t hatrack_realloc_fn = hatrack_default_realloc;
static hatrack_free_t    hatrack_free_fn    = hatrack_default_free;
static void             *hatrack_malloc_arg = NULL;

void
hatrack_setmallocfns(hatrack_mem_manager_t *info)
{
    hatrack_malloc_fn  = info->mallocfn;
    hatrack_zalloc_fn  = info->zallocfn;
    hatrack_realloc_fn = info->reallocfn;
    hatrack_free_fn    = info->freefn;
    hatrack_malloc_arg = info->arg;

    if (hatrack_malloc_fn == NULL) {
        hatrack_malloc_fn = hatrack_default_malloc;
    }
    if (hatrack_zalloc_fn == NULL) {
        hatrack_zalloc_fn = hatrack_default_zalloc;
    }
    if (hatrack_realloc_fn == NULL) {
        hatrack_realloc_fn = hatrack_default_realloc;
    }
    if (hatrack_free_fn == NULL) {
        hatrack_free_fn = hatrack_default_free;
    }
}

#ifdef HATRACK_ALLOC_PASS_LOCATION
#define POSSIBLE_LOCATION , file, line
#else
#define POSSIBLE_LOCATION
#endif

void *
_hatrack_malloc(size_t size HATRACK_LOC_DECL)
{
    return hatrack_malloc_fn(size, hatrack_malloc_arg POSSIBLE_LOCATION);
}

void *
_hatrack_zalloc(size_t size HATRACK_LOC_DECL)
{
    return hatrack_zalloc_fn(size, hatrack_malloc_arg POSSIBLE_LOCATION);
}

void
_hatrack_free(void *oldptr, size_t oldsize HATRACK_LOC_DECL)
{
    if (oldptr != NULL) {
        hatrack_free_fn(oldptr, oldsize, hatrack_malloc_arg POSSIBLE_LOCATION);
    }
}

void *
_hatrack_realloc(void *oldptr, size_t oldsize, size_t newsize HATRACK_LOC_DECL)
{
    if (NULL == oldptr) {
        return hatrack_malloc(newsize);
    }
    if (oldsize == newsize) {
        return oldptr;
    }
    return hatrack_realloc_fn(oldptr,
                              oldsize,
                              newsize,
                              hatrack_malloc_arg
                                  POSSIBLE_LOCATION);
}

#ifdef HATRACK_PER_INSTANCE_AUX
extern void *
_hatrack_malloc_aux(size_t size, void *aux HATRACK_LOC_DECL)
{
    return hatrack_malloc_fn(size, aux POSSIBLE_LOCATION);
}

extern void *
_hatrack_zalloc_aux(size_t size, void *aux HATRACK_LOC_DECL)
{
    return hatrack_zalloc_fn(size, aux POSSIBLE_LOCATION);
}
#endif
