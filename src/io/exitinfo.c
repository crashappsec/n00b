#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void *launch_wait4(void *arg);

bool
n00b_io_exitinfo_report(n00b_stream_t *party, uint64_t ignore_always_1)
{
    return true;
}

static n00b_string_t *
n00b_io_exitinfo_repr(n00b_stream_t *e)
{
    n00b_exitinfo_t *psi = e->cookie;

    n00b_string_t *result = n00b_cformat("«#»exitinfo: pid #«#»«#»",
                                         n00b_cached_lbracket(),
                                         (int64_t)psi->pid,
                                         n00b_cached_rbracket());

    return result;
}

static void *
launch_wait4(void *arg)
{
    n00b_stream_t   *party  = arg;
    n00b_exitinfo_t *cookie = party->cookie;

    n00b_thread_t *expected = NULL;
    n00b_thread_t *me       = n00b_thread_self();

    n00b_thread_async_cancelable();
    if (!CAS(&cookie->wait_thread, &expected, me)) {
        // Another thread got to do the wait.
        return NULL;
    }

    while (true) {
        n00b_nanosleep(0, N00B_CALLBACK_THREAD_POLL_INTERVAL);
        n00b_gts_suspend();

        int r = wait4(cookie->pid, &cookie->stats, WNOHANG, &cookie->rusage);
        if (r == -1) {
            n00b_gts_resume();
            n00b_post_errno(party);
            break;
        }
        if (r) {
            n00b_gts_resume();
            break;
        }
    }

    n00b_list_t *l;

    l = n00b_handle_read_operation(party, (void *)(int64_t)cookie->stats);

    if (l) {
        void *item = n00b_list_get(l, 0, NULL);
        n00b_post_to_subscribers(party, item, n00b_io_sk_read);
        n00b_purge_subscription_list_on_boundary(party->read_subs);
    }

    n00b_close(party);

    return NULL;
}

bool
n00b_io_exitinfo_subscribe(n00b_stream_sub_t        *sub,
                           n00b_io_subscription_kind kind)
{
    defer_on();
    n00b_acquire_party(sub->source);
    n00b_exitinfo_t *cookie = sub->source->cookie;

    if (!atomic_read(&cookie->streams_to_drain)) {
        Return true;
    }

    if (!atomic_read(&cookie->wait_thread)) {
        n00b_thread_spawn(launch_wait4, sub->source);
    }
    else {
        if (kind == n00b_io_sk_read) {
            n00b_list_t *l;
            l = n00b_handle_read_operation(sub->source,
                                           (void *)(int64_t)cookie->stats);
            n00b_post_to_subscribers(sub->source,
                                     n00b_list_get(l, 0, NULL),
                                     n00b_io_sk_read);
            n00b_purge_subscription_list_on_boundary(sub->source->read_subs);
        }
    }

    Return true;
    defer_func_end();
}

static void
exitinfo_fd_drained(n00b_stream_t *s, void *msg, n00b_stream_t *ei)
{
    n00b_exitinfo_t *cookie = ei->cookie;
    int              n      = atomic_fetch_add(&cookie->streams_to_drain,
                             -1);

    if (n == 1) {
        n00b_thread_spawn(launch_wait4, s);
    }
}

n00b_stream_t *
n00b_pid_monitor(int64_t pid, void *watch_fds)
{
    defer_on();

    n00b_exitinfo_t *cookie = n00b_gc_alloc_mapped(n00b_exitinfo_t,
                                                   N00B_GC_SCAN_ALL);
    n00b_stream_t *new      = n00b_alloc_party(&n00b_exitinfo_impl,
                                          cookie,
                                          n00b_io_perm_r,
                                          n00b_io_ev_exitinfo);

    cookie->pid = pid;

    if (!watch_fds) {
        Return new;
    }

    n00b_type_t *t = n00b_get_my_type(watch_fds);
    n00b_list_t *l = watch_fds;

    if (!n00b_type_is_list(t)) {
        l = n00b_list(n00b_type_stream());
        n00b_list_append(l, watch_fds);
        watch_fds = l;
    }

    int n = n00b_list_len(l);

    if (!n) {
        Return new;
    }

    n00b_stream_t *callback = n00b_callback_open((void *)exitinfo_fd_drained,
                                                 new,
                                                 n00b_cstring("exitinfo_fd_drained"));
    for (int i = 0; i < n; i++) {
        n00b_stream_t *fd = n00b_list_get(l, i, NULL);
        n00b_type_t   *t  = n00b_get_my_type(fd);

        n00b_acquire_party(fd);
        if (!(n00b_type_is_stream(t) || n00b_type_is_file(t))) {
            N00B_CRAISE("Invalid stream.");
        }
        if (fd->closed_for_read) {
            n00b_release_party(fd);
            continue;
        }

        n00b_io_set_repr(callback, n00b_cstring("drain_counter"));

        atomic_fetch_add(&cookie->streams_to_drain, 1);
        n00b_io_subscribe_oneshot(fd, callback, n00b_io_sk_close);
        n00b_release_party(fd);
        fd = NULL;
    }

    Return new;
    defer_func_end();
}

n00b_io_impl_info_t n00b_exitinfo_impl = {
    .open_impl      = (void *)n00b_pid_monitor,
    .subscribe_impl = n00b_io_exitinfo_subscribe,
    .read_impl      = n00b_io_exitinfo_report,
    .repr_impl      = n00b_io_exitinfo_repr,
};
