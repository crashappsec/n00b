#define N00B_USE_INTERNAL_API
#include "n00b.h"

// n00b_debug("wait4", n00b_new_utf8("Capture RUSAGE."));

//    if (wait4((pid_t)pid, &cookie->stats, WNOHANG, &cookie->rusage) == -1) {
//        N00B_CRAISE("Provided pid is not a valid subprocess of this process.");
//    }

bool
n00b_io_exitinfo_report(n00b_stream_t *party, uint64_t ignore_always_1)
{
    return true;
}

static n00b_utf8_t *
n00b_io_exitinfo_repr(n00b_stream_t *e)
{
    n00b_exitinfo_t *psi = e->cookie;

    n00b_utf8_t *result = n00b_cstr_format("{}exitinfo: pid #{}{}",
                                           n00b_new_utf8("["),
                                           (int64_t)psi->pid,
                                           n00b_new_utf8("]"));

    return result;
}

static void *
launch_wait4(void *arg)
{
    n00b_stream_t   *party  = arg;
    n00b_exitinfo_t *cookie = party->cookie;

    n00b_thread_t *expected = NULL;
    n00b_thread_t *me       = n00b_thread_self();

    if (!CAS(&cookie->wait_thread, &expected, me)) {
        // Another thread got to do the wait.
        return NULL;
    }

    n00b_thread_leave_run_state();

    int r = wait4(cookie->pid, &cookie->stats, 0, &cookie->rusage);

    n00b_thread_enter_run_state();

    if (r == -1) {
        n00b_post_errno(party);
    }

    n00b_list_t *l;

    l = n00b_handle_read_operation(party, (void *)(int64_t)cookie->stats);

    if (l) {
        void *item = n00b_list_get(l, 0, NULL);
        n00b_post_to_subscribers(party, item, n00b_io_sk_read);
        n00b_purge_subscription_list_on_boundary(party->read_subs);
    }

    return NULL;
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
    n00b_exitinfo_t *cookie = n00b_gc_alloc_mapped(n00b_exitinfo_t,
                                                   N00B_GC_SCAN_ALL);
    n00b_stream_t *new      = n00b_alloc_party(&n00b_exitinfo_impl,
                                          cookie,
                                          n00b_io_perm_r,
                                          n00b_io_ev_exitinfo);

    cookie->pid = pid;

    if (!watch_fds) {
        return new;
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
        return new;
    }

    n00b_stream_t *callback = n00b_callback_open((void *)exitinfo_fd_drained,
                                                 new);
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

        n00b_io_set_repr(callback, n00b_new_utf8("drain_counter"));

        atomic_fetch_add(&cookie->streams_to_drain, 1);
        n00b_io_subscribe_oneshot(fd, callback, n00b_io_sk_close);
        n00b_release_party(fd);
    }

    return new;
}

bool
n00b_io_exitinfo_subscribe(n00b_stream_sub_t        *sub,
                           n00b_io_subscription_kind kind)
{
    n00b_acquire_party(sub->source);
    n00b_exitinfo_t *cookie = sub->source->cookie;

    if (!atomic_read(&cookie->streams_to_drain)) {
        return true;
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

    n00b_release_party(sub->source);

    return true;
}

n00b_io_impl_info_t n00b_exitinfo_impl = {
    .open_impl      = (void *)n00b_pid_monitor,
    .subscribe_impl = n00b_io_exitinfo_subscribe,
    .read_impl      = n00b_io_exitinfo_report,
    .repr_impl      = n00b_io_exitinfo_repr,
};
