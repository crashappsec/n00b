
#define N00B_USE_INTERNAL_API
#include "n00b.h"

static stack_t n00b_signal_stack = {
    .ss_flags = 0,
};

typedef struct {
    n00b_list_t *list;
    siginfo_t    signal_info;
} sig_callback_info_t;

static sig_callback_info_t n00b_signal_handlers[N00B_MAX_SIGNAL] = {
    {
        .list        = NULL,
        .signal_info = {
            0,
        },
    },
};

static int              n00b_signal_pipe[2];
static n00b_spin_lock_t update_lock;

static struct pollfd pollset[1] = {
    {
        .events  = POLLIN,
        .revents = 0,
    },
};

pthread_once_t n00b_signals_inited = PTHREAD_ONCE_INIT;

static void *
n00b_signal_monitor(void *ignore)
{
    char                  sigbyte[1];
    int                   signal;
    n00b_list_t          *handlers;
    siginfo_t            *siginfo;
    n00b_signal_handler_t callback;

    while (true) {
        n00b_gts_suspend();
        int r = poll(pollset, 1, -1);
        n00b_gts_resume();

        switch (r) {
        case -1:
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            n00b_raise_errno();
        default:

            read(n00b_signal_pipe[0], &sigbyte, 1);
            signal = sigbyte[0];

            if (signal == -1) {
                break;
            }

            handlers = n00b_signal_handlers[signal].list;
            int n    = n00b_list_len(handlers);
            siginfo  = &n00b_signal_handlers[signal].signal_info;

            while (n--) {
                callback = n00b_list_get(handlers, n, NULL);
                if (callback) {
                    (*callback)(signal, siginfo);
                }
            }
        }
    }
}

void
n00b_setup_signals(void)
{
    n00b_signal_stack.ss_sp   = mmap(NULL,
                                   SIGSTKSZ,
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANON,
                                   -1,
                                   0);
    n00b_signal_stack.ss_size = SIGSTKSZ;

    if (sigaltstack(&n00b_signal_stack, NULL)) {
        n00b_raise_errno();
    }

    if (pipe(n00b_signal_pipe)) {
        n00b_raise_errno();
    }

    fcntl(n00b_signal_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(n00b_signal_pipe[1], F_SETFL, O_NONBLOCK);

    pollset[0].fd = n00b_signal_pipe[0];

    n00b_gc_register_root(&n00b_signal_handlers, sizeof(n00b_signal_handlers) / 8);
    n00b_init_spin_lock(&update_lock);
    n00b_thread_spawn(n00b_signal_monitor, NULL);
}

void
n00b_terminate_signal_handling(void)
{
    char value[1] = {-1};

    write(n00b_signal_pipe[1], value, 1);
}

static void
n00b_handle_signal(int n, siginfo_t *info, void *ignored)
{
    char value[1] = {(char)n};

    int saved_errno = errno;

    n00b_signal_handlers[n].signal_info = *info;
    write(n00b_signal_pipe[1], value, 1);
    errno = saved_errno;
}

static inline void
add_signal_handler(int n)
{
    struct sigaction info = {
        .sa_sigaction = n00b_handle_signal,
        .sa_flags     = SA_ONSTACK | SA_RESTART | SA_SIGINFO,
    };

    sigaction(n, &info, NULL);
}

static inline void
remove_signal_handler(int n)
{
    struct sigaction info = {
        .sa_handler = SIG_DFL,
    };

    sigaction(n, &info, NULL);
}

bool
n00b_signal_register(int n, n00b_signal_handler_t h)
{
    if (n < 0 || n >= N00B_MAX_SIGNAL) {
        return false;
    }

    n00b_spin_lock(&update_lock);
    sig_callback_info_t *info = &n00b_signal_handlers[n];

    n00b_list_t *l = info->list;

    if (!l) {
        l          = n00b_list(n00b_type_ref());
        info->list = l;
    }

    n00b_private_list_append(l, h);

    if (n00b_list_len(l) == 1) {
        add_signal_handler(n);
    }

    n00b_spin_unlock(&update_lock);

    return true;
}

bool
n00b_signal_unregister(int n, n00b_signal_handler_t h)
{
    bool result;

    if (n < 0 || n >= N00B_MAX_SIGNAL) {
        return false;
    }

    n00b_list_t *l = n00b_signal_handlers[n].list;

    if (!l) {
        return false;
    }

    n00b_spin_lock(&update_lock);
    result = n00b_private_list_remove_item(l, h);

    if (!n00b_list_len(l)) {
        remove_signal_handler(n);
    }

    n00b_spin_unlock(&update_lock);

    return result;
}
