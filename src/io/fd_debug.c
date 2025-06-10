#define N00B_USE_INTERNAL_API
#include "n00b.h"

void
n00b_dump_read_subscriptions(n00b_event_loop_t *hll)
{
    if (!hll) {
        hll = n00b_system_dispatcher;
    }

    n00b_pevent_loop_t *l = &hll->algo.poll;

    for (int i = 0; i < l->pollset_last; i++) {
        n00b_fd_stream_t *s = l->monitored_fds[i];
        if (!s) {
            continue;
        }

        int nr = 0;

        if (s->read_subs) {
            nr = n00b_list_len(s->read_subs);
        }

        n00b_string_t *cstr;
        cstr = n00b_cformat("fd [=#=] has [=#=] read subs\n",
                            (int64_t)s->fd,
                            (int64_t)nr,
                            0);
        printf("%s", cstr->data);

        for (int j = 0; j < nr; j++) {
            n00b_fd_sub_t *sub = n00b_list_get(s->read_subs, j, NULL);
            cstr               = n00b_cformat("Subscriber param: [=#=]\n", sub->thunk);
            printf("%s", cstr->data);
        }
    }
}
