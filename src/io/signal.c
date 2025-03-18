#define N00B_USE_INTERNAL_API
#include "n00b.h"

static void
sighandle(int signal, short ignoring, void *also_ignoring)
{
    int64_t             i64         = signal;
    n00b_stream_base_t *base        = n00b_get_ev_base(&n00b_signal_impl);
    n00b_stream_t      *signal_info = hatrack_dict_get(base->io_signal_cache,
                                                  (void *)i64,
                                                  NULL);

    // No handlers anymore.
    if (!signal_info || !signal_info->read_subs) {
        return;
    }

    n00b_post_to_subscribers(signal_info,
                             (void *)(int64_t)signal,
                             n00b_io_sk_read);
    n00b_purge_subscription_list_on_boundary(signal_info->read_subs);
    n00b_ev2_post_read_cleanup(signal_info);
}

static n00b_stream_t *
n00b_io_signal_open(int64_t signal, n00b_io_impl_info_t *impl)
{
    n00b_ev2_cookie_t  *cookie = n00b_new_ev2_cookie();
    n00b_stream_base_t *base   = n00b_get_ev_base(impl);
    n00b_stream_t *new         = n00b_alloc_party(impl,
                                          cookie,
                                          n00b_io_perm_r,
                                          n00b_io_ev_signal);

    cookie->signal = true;
    cookie->id     = signal;

    n00b_lock_acquire(&new->lock);
    n00b_stream_t *result = n00b_add_or_replace(new,
                                                base->io_signal_cache,
                                                (void *)signal);

    if (result != new) {
        n00b_lock_release(&new->lock);
        return result;
    }

    switch (signal) {
    case SIGKILL:
    case SIGSTOP:
        N00B_CRAISE("Signal cannot be caught.");
    default:
        cookie->read_event = evsignal_new(base->event_ctx,
                                          signal,
                                          sighandle,
                                          result);
        n00b_lock_release(&new->lock);
        return result;
    }
}

static bool
n00b_io_signal_subscribe(n00b_stream_sub_t        *info,
                         n00b_io_subscription_kind kind)
{
    n00b_lock_acquire(&info->source->lock);
    info->source->read_active = true;
    n00b_lock_release(&info->source->lock);

    return true;
}

n00b_stream_t *
n00b_io_get_signal_event(int signal)
{
    return n00b_io_signal_open(signal, &n00b_signal_impl);
}

n00b_string_t *
n00b_get_signal_name(int64_t signal)
{
    switch (signal) {
    case SIGHUP:
        return n00b_cstring("SIGHUP");
    case SIGINT:
        return n00b_cstring("SIGINT");
    case SIGQUIT:
        return n00b_cstring("SIGQUIT");
    case SIGILL:
        return n00b_cstring("SIGILL");
    case SIGTRAP:
        return n00b_cstring("SIGTRAP");
    case SIGABRT:
        return n00b_cstring("SIGABRT");
    case SIGFPE:
        return n00b_cstring("SIGFPE");
    case SIGKILL:
        return n00b_cstring("SIGKILL");
    case SIGBUS:
        return n00b_cstring("SIGBUS");
    case SIGSEGV:
        return n00b_cstring("SIGSEGV");
    case SIGSYS:
        return n00b_cstring("SIGSYS");
    case SIGPIPE:
        return n00b_cstring("SIGPIPE");
    case SIGALRM:
        return n00b_cstring("SIGALRM");
    case SIGTERM:
        return n00b_cstring("SIGTERM");
    case SIGURG:
        return n00b_cstring("SIGURG");
    case SIGSTOP:
        return n00b_cstring("SIGSTOP");
    case SIGTSTP:
        return n00b_cstring("SIGTSTP");
    case SIGCONT:
        return n00b_cstring("SIGCONT");
    case SIGCHLD:
        return n00b_cstring("SIGCHLD");
    case SIGTTIN:
        return n00b_cstring("SIGTTIN");
    case SIGTTOU:
        return n00b_cstring("SIGTTOU");
    case SIGIO:
        return n00b_cstring("SIGIO");
    case SIGXCPU:
        return n00b_cstring("SIGXCPU");
    case SIGXFSZ:
        return n00b_cstring("SIGXFSZ");
    case SIGVTALRM:
        return n00b_cstring("SIGVTALRM");
    case SIGPROF:
        return n00b_cstring("SIGPROF");
    case SIGWINCH:
        return n00b_cstring("SIGWINCH");
    case SIGUSR1:
        return n00b_cstring("SIGUSR1");
    case SIGUSR2:
        return n00b_cstring("SIGUSR2");
#if !defined(__linux__)	
    case SIGEMT:
        return n00b_cstring("SIGEMT");
    case SIGINFO:
        return n00b_cstring("SIGINFO");
#endif	
    default:
        N00B_CRAISE("Unknown signal");
    }
}

static n00b_string_t *
n00b_io_signal_repr(n00b_stream_t *e)
{
    n00b_ev2_cookie_t *cookie = e->cookie;

    return n00b_cformat("«#»signal «#» («#»)«#»",
                        n00b_cached_lbracket(),
                        cookie->id,
                        n00b_get_signal_name(cookie->id),
                        n00b_cached_rbracket());
}

n00b_stream_sub_t *
_n00b_io_register_signal_handler(int                 signal,
                                 n00b_io_callback_fn fn,
                                 ...)
{
    va_list arg;
    va_start(arg, fn);

    void          *a  = va_arg(arg, void *);
    n00b_stream_t *ev = n00b_io_get_signal_event(signal);
    n00b_stream_t *cb = n00b_callback_open(fn, a);
    n00b_io_set_repr(cb,
                     n00b_cformat("«#»«#» handler«#»",
                                  n00b_cached_lbracket(),
                                  n00b_get_signal_name(signal),
                                  n00b_cached_rbracket()));
    return n00b_io_subscribe_to_reads(ev, cb, a);
}

n00b_io_impl_info_t n00b_signal_impl = {
    .open_impl        = (void *)n00b_io_signal_open,
    .subscribe_impl   = n00b_io_signal_subscribe,
    .unsubscribe_impl = n00b_io_ev_unsubscribe,
    .repr_impl        = n00b_io_signal_repr,
    .use_libevent     = true,
};
