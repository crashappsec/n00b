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

n00b_utf8_t *
n00b_get_signal_name(int64_t signal)
{
    switch (signal) {
    case SIGHUP:
        return n00b_new_utf8("SIGHUP");
    case SIGINT:
        return n00b_new_utf8("SIGINT");
    case SIGQUIT:
        return n00b_new_utf8("SIGQUIT");
    case SIGILL:
        return n00b_new_utf8("SIGILL");
    case SIGTRAP:
        return n00b_new_utf8("SIGTRAP");
    case SIGABRT:
        return n00b_new_utf8("SIGABRT");
    case SIGEMT:
        return n00b_new_utf8("SIGEMT");
    case SIGFPE:
        return n00b_new_utf8("SIGFPE");
    case SIGKILL:
        return n00b_new_utf8("SIGKILL");
    case SIGBUS:
        return n00b_new_utf8("SIGBUS");
    case SIGSEGV:
        return n00b_new_utf8("SIGSEGV");
    case SIGSYS:
        return n00b_new_utf8("SIGSYS");
    case SIGPIPE:
        return n00b_new_utf8("SIGPIPE");
    case SIGALRM:
        return n00b_new_utf8("SIGALRM");
    case SIGTERM:
        return n00b_new_utf8("SIGTERM");
    case SIGURG:
        return n00b_new_utf8("SIGURG");
    case SIGSTOP:
        return n00b_new_utf8("SIGSTOP");
    case SIGTSTP:
        return n00b_new_utf8("SIGTSTP");
    case SIGCONT:
        return n00b_new_utf8("SIGCONT");
    case SIGCHLD:
        return n00b_new_utf8("SIGCHLD");
    case SIGTTIN:
        return n00b_new_utf8("SIGTTIN");
    case SIGTTOU:
        return n00b_new_utf8("SIGTTOU");
    case SIGIO:
        return n00b_new_utf8("SIGIO");
    case SIGXCPU:
        return n00b_new_utf8("SIGXCPU");
    case SIGXFSZ:
        return n00b_new_utf8("SIGXFSZ");
    case SIGVTALRM:
        return n00b_new_utf8("SIGVTALRM");
    case SIGPROF:
        return n00b_new_utf8("SIGPROF");
    case SIGWINCH:
        return n00b_new_utf8("SIGWINCH");
    case SIGINFO:
        return n00b_new_utf8("SIGINFO");
    case SIGUSR1:
        return n00b_new_utf8("SIGUSR1");
    case SIGUSR2:
        return n00b_new_utf8("SIGUSR2");
    default:
        N00B_CRAISE("Unknown signal");
    }
}

static n00b_utf8_t *
n00b_io_signal_repr(n00b_stream_t *e)
{
    n00b_ev2_cookie_t *cookie = e->cookie;

    return n00b_cstr_format("{}signal {} ({}){}",
                            n00b_new_utf8("["),
                            cookie->id,
                            n00b_get_signal_name(cookie->id),
                            n00b_new_utf8("]"));
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
                     n00b_cstr_format("{}{} handler{}",
                                      n00b_new_utf8("["),
                                      n00b_get_signal_name(signal),
                                      n00b_new_utf8("]")));
    return n00b_io_subscribe_to_reads(ev, cb, a);
}

n00b_io_impl_info_t n00b_signal_impl = {
    .open_impl        = (void *)n00b_io_signal_open,
    .subscribe_impl   = n00b_io_signal_subscribe,
    .unsubscribe_impl = n00b_io_ev_unsubscribe,
    .repr_impl        = n00b_io_signal_repr,
    .use_libevent     = true,
};
