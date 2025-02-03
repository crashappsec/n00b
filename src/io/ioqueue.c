#define N00B_USE_INTERNAL_API
#include "n00b.h"

static uint64_t                  msgs_per_page;
static _Atomic(n00b_ioqueue_t *) in_queue;
static n00b_ioqueue_t           *private_queue;
static n00b_list_t              *io_callbacks = NULL;
static n00b_notifier_t          *cb_notifier;
static _Atomic(n00b_thread_t *)  cur_callback_thread = NULL;

static void
n00b_run_one_io_callback(n00b_iocb_info_t *cb_info)
{
    (*cb_info->cookie->fn)(cb_info->stream, cb_info->msg, cb_info->cookie->aux);

    if (cb_info->cookie->notify) {
        n00b_notify(cb_info->cookie->notifier, 0xdadadadadadadadaULL);
    }
}

static void *
n00b_run_callbacks(void *ignored)
{
    n00b_iocb_info_t *cb_info;

    n00b_duration_t sleeptime = {
        .tv_sec  = 0,
        .tv_nsec = N00B_CALLBACK_THREAD_POLL_INTERVAL,
    };

    atomic_store(&cur_callback_thread, n00b_thread_self());

    while (true) {
        n00b_gts_checkin();

        if (atomic_read(&cur_callback_thread) != n00b_thread_self()) {
#ifdef N00B_ASYNC_IO_CALLBACKS
            n00b_thread_exit(NULL);
#else
            atomic_store(&cur_callback_thread, n00b_thread_self());
#endif
        }

        if (!n00b_list_len(io_callbacks)) {
            if (n00b_io_has_exited) {
                n00b_thread_exit(NULL);
            }

            n00b_gts_suspend();
            nanosleep(&sleeptime, NULL);
            n00b_gts_resume();
            continue;
        }

        cb_info = n00b_list_dequeue(io_callbacks);

        if (!cb_info) {
            continue;
        }
#ifdef N00B_ASYNC_IO_CALLBACKS
        atomic_store(&cur_callback_thread, NULL);
#endif
        n00b_run_one_io_callback(cb_info);
        bzero((void *)cb_info, sizeof(n00b_iocb_info_t));
#ifdef N00B_ASYNC_IO_CALLBACKS
        n00b_thread_t *expected = NULL;
        CAS(&cur_callback_thread, &expected, n00b_thread_self());
#endif
    }

    n00b_unreachable();
}

void
n00b_ioqueue_launch_callback_thread(void)
{
    n00b_thread_t *t = atomic_read(&cur_callback_thread);

    // We'll let the callback thread create a successor if needed.
    if (t != NULL && t != n00b_thread_self()) {
        return;
    }

    n00b_thread_spawn(n00b_run_callbacks, NULL);
}

void
n00b_ioqueue_dont_block_callbacks(void)
{
    n00b_thread_t *thread;

    thread = atomic_read(&cur_callback_thread);

    if (!thread) {
        return;
    }
    if (thread == n00b_thread_self()) {
        // We already have the lock.
        atomic_store(&cur_callback_thread, NULL);
    }
}

void
n00b_ioqueue_enqueue_callback(n00b_iocb_info_t *cb_info)
{
    n00b_list_append(io_callbacks, cb_info);
}

void
n00b_write(n00b_stream_t *recipient, void *m)
{
    n00b_ioqentry_t entry = {
        .recipient = recipient,
        .msg       = m,
    };

    n00b_ioqueue_t *top         = atomic_read(&in_queue);
    n00b_ioqueue_t *cur         = top;
    uint64_t        write_index = atomic_fetch_add(&top->num, 1);
    int             page        = write_index / msgs_per_page;
    int             offset      = write_index % msgs_per_page;

    for (int i = 0; i < page - 1; i++) {
        cur = atomic_read(&cur->next_page);
    }

    if (page) {
        n00b_ioqueue_t *new    = n00b_unprotected_mempage();
        n00b_ioqueue_t *expect = NULL;

        n00b_heap_register_dynamic_root(n00b_internal_heap,
                                        new,
                                        n00b_get_page_size() / 8);

        if (!CAS(&cur->next_page, &expect, new)) {
            n00b_heap_remove_root(n00b_internal_heap, new);
            n00b_delete_mempage(new);
            cur = expect;
        }
        else {
            cur = new;
        }
    }

    n00b_assert(n00b_type_is_stream(n00b_get_my_type(entry.recipient)));

    cur->q[offset] = entry;

#ifdef N00B_INTERNAL_DEBUG
    printf("\n->q msg %llu for %s (%p); msg was:\n %s",
           write_index + 1,
           n00b_repr(recipient)->data,
           entry.recipient,
           n00b_repr(entry.msg)->data);
#endif
}

extern void         n00b_handle_one_delivery(n00b_stream_t *, void *);
extern n00b_utf8_t *n00b_stream_repr(n00b_stream_t *);
// The main queue is processed AFTER one I/O event loop, but in the
// same thread. Callbacks, however, run on a different thread.
void
n00b_process_queue(void)
{
    n00b_ioqueue_t *processing = atomic_read(&in_queue);
    n00b_ioqueue_t *cur        = private_queue;
    n00b_ioqentry_t empty      = {NULL, NULL};
    n00b_ioqentry_t entry;

    // Only the IO thread should change in_queue, so we can just
    // swap them manually.
    atomic_store(&in_queue, cur);
    private_queue = processing;

    uint64_t total     = atomic_read(&processing->num);
    int      pages     = total / msgs_per_page;
    uint64_t delivered = 0;

    for (int i = 0; i < pages; i++) {
        for (unsigned int j = 0; j < msgs_per_page; j++) {
            // Spin-wait for slow writers.
            do {
                entry = processing->q[j];
            } while (!entry.recipient);
            delivered++;
            processing->q[j] = empty;
            n00b_assert(n00b_type_is_stream(n00b_get_my_type(entry.recipient)));
            n00b_handle_one_delivery(entry.recipient, entry.msg);
#ifdef N00B_INTERNAL_DEBUG
            printf("\nq-> deliver msg slot %llu to recipient %s (%p); msg was:\n%s\n",
                   delivered,
                   n00b_repr(entry.recipient)->data,
                   entry.recipient,
                   n00b_repr(entry.msg)->data);
#endif
        }
        processing = atomic_read(&processing->next_page);
    }

    while (delivered < total) {
        do {
            entry = processing->q[delivered];
        } while (!entry.recipient);

        processing->q[delivered] = empty;

        n00b_handle_one_delivery(entry.recipient, entry.msg);
        delivered++;
    }

    atomic_store(&private_queue->num, 0);
}

void
n00b_ioqueue_setup(void)
{
    msgs_per_page = (n00b_get_page_size() - sizeof(n00b_ioqueue_t))
                  / sizeof(n00b_ioqentry_t);

    n00b_ioqueue_t *q = n00b_unprotected_mempage();

    n00b_heap_register_root(n00b_internal_heap,
                            q,
                            n00b_get_page_size() / 8);

    atomic_store(&q->num, 0);
    atomic_store(&q->next_page, NULL);
    atomic_store(&in_queue, q);
    private_queue            = n00b_unprotected_mempage();
    private_queue->num       = 0;
    private_queue->next_page = NULL;

    n00b_heap_register_root(n00b_internal_heap,
                            private_queue,
                            n00b_get_page_size() / 8);

    // The main queue uses its own memory management.
    n00b_gc_register_root(&io_callbacks, 1);
    n00b_gc_register_root(&private_queue, 1);
    n00b_gc_register_root(&in_queue, 1);
    n00b_gc_register_root(&cb_notifier, 1);
    n00b_gc_register_root(&cur_callback_thread, 1);

    io_callbacks = n00b_list(n00b_type_ref());
    cb_notifier  = n00b_new_notifier();
}
