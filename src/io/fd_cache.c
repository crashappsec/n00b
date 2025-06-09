#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_dict_t *n00b_fd_cache = NULL;

// When we add an entry for a file descriptor, we want to avoid adding
// it multiple times to the same polling context, but allow different
// contexts to keep the same FD. We could add a cache per poll
// context, but this is easier in case I see a reason to disallow the
// same fd across poll sets.
//
// The only challenge this approach gives us, is we can't quite use
// the FD as the key. We instead, want the hash key to be a fd /
// dispatcher pair.
//
// To avoid a custom hash, we need to get the key to be unique with
// high likelyhood, in 64 bits. In the same heap, we wouldn't get
// collisions from the bottom half of the pointer, but if we allow
// multiple arenas it's a possibility.
//
// One easy think would be to xor the FD into the unused bits of the
// pointer, but that only gets us 3 bits, before we risk collisions.
//
// BUT, we don't expect to have significant collisions in the high
// bits of the heap. And the object might move, so we'd have to be
// able to keep track of that (which we actually do, but accessing it
// is a bit of a pain).
//
// To make a long story short, we sidestep the issue by
// creating a random heap key for the dispatcher, and then XORing
// the fd into it.

static inline int64_t
fd_hash_key(n00b_event_loop_t *loop, int fd)
{
    if (!loop->heap_key) {
        loop->heap_key = n00b_rand64();
    }

    return loop->heap_key ^ (int64_t)fd;
}

n00b_fd_stream_t *
n00b_fd_cache_lookup(int fd, n00b_event_loop_t *loop)
{
    int64_t           key    = fd_hash_key(loop, fd);
    n00b_fd_stream_t *result = hatrack_dict_get(n00b_fd_cache,
                                                (void *)key,
                                                NULL);

    if (result && result->fd == N00B_FD_CLOSED) {
        return NULL;
    }

    return result;
}

n00b_fd_stream_t *
n00b_fd_cache_add(n00b_fd_stream_t *s)
{
    int64_t           key = fd_hash_key(s->evloop, s->fd);
    n00b_fd_stream_t *found_entry;

    while (true) {
        found_entry = hatrack_dict_get(n00b_fd_cache, (void *)key, NULL);

        // There's something there that appears to be open.
        if (found_entry && found_entry->fd >= 0) {
            return found_entry;
        }

        // Either there's nothing there, or the found entry is closed, and
        // we want to replace it.

        if (hatrack_dict_cas(n00b_fd_cache,
                             (void *)key,
                             s,
                             found_entry,
                             true)) {
            return s;
        }

        // If we didn't win the CAS, the hatrack API doesn't give us
        // the new value, so we need to re-load, so we loop.
    }
}
