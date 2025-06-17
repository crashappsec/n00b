#include "n00b.h"
#if defined(N00B_DEBUG)
// Formatted debug logging interface.

// Reserve more space than we need for most things; strings we will
// add in the length. We're going to overestimate in any world.
#define DEFAULT_PER_PARAM 20

typedef struct {
    char   *topic;
    int64_t value;
} topic_record_t;

static topic_record_t debug_topics[] = N00B_DLOG_GENERATED_INITS;

static inline int64_t
lookup_dlog_policy(int tid)
{
    return debug_topics[tid].value;
}

int64_t
n00b_dlog_get_topic_policy(char *topic)
{
    for (int i = 0; i < N00B_DLOG_NUM_TOPICS; i++) {
        if (!strcmp(topic, debug_topics[i].topic)) {
            return debug_topics[i].value;
        }
    }

    return -1;
}

bool
n00b_dlog_set_topic_policy(char *topic, int64_t policy)
{
    for (int i = 0; i < N00B_DLOG_NUM_TOPICS; i++) {
        if (!strcmp(topic, debug_topics[i].topic)) {
            debug_topics[i].value = policy;
            return true;
        }
    }

    return false;
}

char *
_n00b_dstrf(char *fmt, int64_t num_params, ...)
{

    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    va_list     args, copy;

    if (tsi) {
        if (tsi->dlogging) {
	    return NULL;
	}
	++tsi->dlogging;
    }

    va_start(args, num_params);
    va_copy(copy, args);

    char  *p   = fmt;
    char  *end = p + strlen(fmt);
    void  *obj;
    void **start     = (void *)args;
    int    arg_index = 0;
    int    alloc_len = strlen(fmt) + DEFAULT_PER_PARAM * num_params;

    while (p < end) {
        if (*p != '%' || (*++p == '%')) {
            p++;
            continue;
        }

        int l_ct = 0;
        while (p < end) {
            switch (*p) {
            case 'l':
                l_ct++;
                p++;
                break;
            case 'd':
            case 'i':
            case 'u':
            case 'o':
            case 'x':
            case 'X':
                if (!l_ct) {
                    va_arg(args, int32_t);
                    arg_index++;
                }
                else {
                    va_arg(args, int64_t);
                    arg_index++;
                }
                p++;
                break;
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A':
            case 'p':
                va_arg(args, int64_t);
                arg_index++;
                p++;
                break;
            case 'c':
                // Might need to IFDEF this based on platform.
                va_arg(args, int32_t);
                arg_index++;
                p++;
                break;
            case '%':
                break;
            case 'n':
                p++;
                break;
            case 's':
                obj = va_arg(args, void *);
                n00b_string_t *s;

                if (n00b_in_heap(obj)) {
                    if (n00b_type_is_string(n00b_get_my_type(obj))) {
                        s                = obj;
                        start[arg_index] = s->data;
                        alloc_len += s->u8_bytes;
                    }
                }

                p++;
                arg_index++;

                break;
            default:
                p++;
                continue;
            }
            // break from inner while loop.
            p++;
            break;
        }
    }

    char *buf = n00b_gil_alloc_len(alloc_len);
    vsnprintf(buf, alloc_len - 1, fmt, copy);

    if (tsi) {
        --tsi->dlogging;
    }

    return buf;
}

void
n00b_dlog(char *topic, int tid, int msg_priority, char *f, int l, char *msg)
{
    if (!msg) {
        return; // Happens during cases where  dlog triggers itself recursively.
    }

    int64_t policy = lookup_dlog_policy(tid);

    if (msg_priority > policy) {
        return;
    }

    n00b_m_log_string(topic, msg, msg_priority, f, l);
}

#endif
