#define N00B_USE_INTERNAL_API
#include "n00b.h"

#define SLOTS_PER_PAGE (n00b_page_bytes / sizeof(void *) - 1)

static n00b_lock_t   **n00b_lock_registry   = NULL;
static pthread_mutex_t n00b_lock_reg_edit   = PTHREAD_MUTEX_INITIALIZER;
static int             n00b_used_lock_slots = 0;

static void
migrate_registry(void)
{
    int n             = n00b_used_lock_slots / SLOTS_PER_PAGE;
    int l             = n * n00b_get_page_size();
    n00b_lock_t **new = mmap(NULL,
                             l + n00b_get_page_size(),
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANON,
                             0,
                             0);
    memcpy(new, n00b_lock_registry, l);

    n00b_heap_register_dynamic_root(n00b_internal_heap,
                                    new,
                                    l + n00b_get_page_size() / 8);

    n00b_heap_remove_root(n00b_internal_heap, n00b_lock_registry);
    munmap(n00b_lock_registry, l);

    n00b_lock_registry = new;
}

void
n00b_lock_register(n00b_lock_t *l)
{
    pthread_mutex_lock(&n00b_lock_reg_edit);
    // The value is the lock type.
    if (n00b_lock_registry) {
        for (int i = 0; i < n00b_used_lock_slots; i++) {
            if (!n00b_lock_registry[i]) {
                n00b_lock_registry[i] = l;
                l->slot               = i;
                pthread_mutex_unlock(&n00b_lock_reg_edit);
                return;
            }
        }

        if (!(n00b_used_lock_slots % SLOTS_PER_PAGE)) {
            if (!n00b_used_lock_slots) {
                n00b_lock_registry = n00b_unprotected_mempage();
                n00b_heap_register_dynamic_root(n00b_internal_heap,
                                                n00b_lock_registry,
                                                n00b_get_page_size() / 8);
            }
            else {
                migrate_registry();
            }
        }

        n00b_lock_registry[n00b_used_lock_slots] = l;
        l->slot                                  = n00b_used_lock_slots;
        n00b_used_lock_slots++;
    }

    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

void
n00b_lock_unregister(n00b_lock_t *l)
{
    pthread_mutex_lock(&n00b_lock_reg_edit);

    if (n00b_lock_registry) {
        n00b_lock_registry[l->slot] = NULL;
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

void
n00b_setup_lock_registry(void)
{
    n00b_lock_registry = n00b_gc_alloc_mapped(crown_t, N00B_GC_SCAN_ALL);

    n00b_heap_register_root(n00b_internal_heap, &n00b_lock_registry, 1);
}

void
n00b_thread_unlock_all(void)
{
    if (!n00b_lock_registry) {
        return;
    }

    n00b_thread_t *self = n00b_thread_self();

    pthread_mutex_lock(&n00b_lock_reg_edit);
    n00b_lock_t **p = n00b_lock_registry;
    n00b_lock_t  *l;

    for (int i = 0; i < n00b_used_lock_slots; i++) {
        l = *p++;
        if (l && l->thread == self) {
            n00b_lock_release_all(l);
        }
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

#ifdef N00B_DEBUG_LOCKS
void
n00b_debug_held_locks(char *file, int line)
{
    if (!n00b_lock_registry) {
        return;
    }

    n00b_thread_t *self = n00b_thread_self();

    pthread_mutex_lock(&n00b_lock_reg_edit);
    n00b_lock_t **p = n00b_lock_registry;
    n00b_lock_t  *l;

    printf("%s:%d: Dumping held locks for thread w/ mmm #%lld\n",
           file,
           line,
           self->mmm_info.tid);

    for (int i = 0; i < n00b_used_lock_slots; i++) {
        l = *p++;
        if (l && l->thread == self) {
            fprintf(stderr, "Not fully unlocked:\n");
            while (l->locks) {
                fprintf(stderr,
                        "  locked @%s:%d\n",
                        l->locks->file,
                        l->locks->line);
                l->locks = l->locks->prev;
            }
        }
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}

void
n00b_debug_all_held_locks(void)
{
    if (!n00b_lock_registry) {
        return;
    }

    pthread_mutex_lock(&n00b_lock_reg_edit);
    n00b_lock_t **p = n00b_lock_registry;
    n00b_lock_t  *l;

    printf("Dumping ALL held locks.\n");

    for (int i = 0; i < n00b_used_lock_slots; i++) {
        l = *p++;
        if (l && l->thread) {
            fprintf(stderr,
                    "Held by %p: (%lld)\n",
                    l->thread,
                    ((mmm_thread_t *)l->thread)->tid);

            while (l->locks) {
                fprintf(stderr,
                        "  locked @%s:%d\n",
                        l->locks->file,
                        l->locks->line);
                l->locks = l->locks->prev;
            }
        }
    }
    pthread_mutex_unlock(&n00b_lock_reg_edit);
}
#endif
