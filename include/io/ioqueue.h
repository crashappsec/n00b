#include "n00b.h"

extern uint64_t n00b_get_page_size(void);

typedef struct n00b_ioqueue_t n00b_ioqueue_t;

typedef struct {
    n00b_stream_t  *recipient;
    n00b_message_t *msg;
} n00b_ioqentry_t;

struct n00b_ioqueue_t {
    _Atomic uint64_t          num;
    _Atomic(n00b_ioqueue_t *) next_page;
    n00b_ioqentry_t           q[];
};

#ifdef N00B_USE_INTERNAL_API
extern bool n00b_process_queue(void);
#endif
