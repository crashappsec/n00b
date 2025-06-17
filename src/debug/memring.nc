/*
 * Copyright © 2021-2022 John Viega
 *  Name:           memring.c
 *  Description:    Debugging via in-memory ring buffer, for use when
 *                  N00B_DEBUG is on.
 *
 *  Author:         John Viega, john@zork.org
 */
#include "n00b.h"
#include "debug/memring.h"

#ifdef N00B_DEBUG

n00b_m_debug_record_t __n00b_m_debug[N00B_DEBUG_RING_SIZE] = {};

_Atomic uint64_t __n00b_m_debug_sequence         = 0;
const char       __n00b_m_hex_conversion_table[] = "0123456789abcdef";

/* n00b_mdebug_get()
 *
 * Returns a pointer to the given slot, if its present.
 * Assumes you're in a debugger.
 */

n00b_m_debug_record_t *
n00b_mdebug_get(int64_t n)
{
    int64_t i = atomic_load(&__n00b_m_debug_sequence);

    if (n < 0) {
        if (!i) {
            return NULL;
        }
        n = i - 1;
    }
    else {
        if (n >= i || n < i - N00B_DEBUG_RING_SIZE) {
            return NULL;
        }
    }

    n &= N00B_DEBUG_RING_LAST_SLOT;
    return &__n00b_m_debug[n];
}

/* n00b_mdebug_dump()
 *
 * Prints the most recent records in the ring buffer to stderr, up to
 * the specified amount.
 */
void
n00b_mdebug_dump_stream(uint64_t max_msgs, FILE *stream)
{
    int64_t oldest_sequence;
    int64_t cur_sequence;
    int64_t i;

    if (!max_msgs || max_msgs > N00B_DEBUG_RING_SIZE) {
        max_msgs = N00B_DEBUG_RING_SIZE;
    }

    cur_sequence    = atomic_load(&__n00b_m_debug_sequence);
    oldest_sequence = cur_sequence - max_msgs;

    if (!cur_sequence) {
        fprintf(stream, "No logs recorded.\n");
    }

    if (oldest_sequence < 0) {
        oldest_sequence = 0;
    }

    oldest_sequence &= N00B_DEBUG_RING_LAST_SLOT;
    cur_sequence &= N00B_DEBUG_RING_LAST_SLOT;

    if (oldest_sequence >= cur_sequence) {
        for (i = oldest_sequence; i < N00B_DEBUG_RING_SIZE; i++) {
            fprintf(stream,
                    "%06llu: %s\n",
                    (unsigned long long)__n00b_m_debug[i].sequence,
                    __n00b_m_debug[i].msg);
            fflush(stream);
        }

        i = 0;
    }
    else {
        i = oldest_sequence;
    }

    for (; i < cur_sequence; i++) {
        fprintf(stream,
                "%06llu: %s\n",
                (unsigned long long)__n00b_m_debug[i].sequence,
                __n00b_m_debug[i].msg);
        fflush(stream);
    }

    return;
}

void
n00b_mdebug_dump(uint64_t max_msgs)
{
    n00b_mdebug_dump_stream(max_msgs, stderr);
}

void
n00b_mdebug_file(char *fname)
{
    FILE *f = fopen(fname, "w");
    if (f) {
        n00b_mdebug_dump_stream(0, f);
    }
    else {
        fprintf(stderr, "Could not open file: %s", strerror(errno));
    }
    fclose(f);
}

/* n00b_mdebug_thread()
 *
 * Prints (to stderr) all messages current in the ring buffer that
 * were written by the current thread.
 */
void
n00b_mdebug_thread(void)
{
    n00b_tsi_t *tsi = n00b_get_tsi_ptr();
    n00b_mdebug_other_thread(tsi->thread_id);

    return;
}

/* n00b_mdebug_thread()
 *
 * Prints (to stderr) all messages current in the ring buffer that
 * were written by the thread with the given id. Note that we use the
 * thread IDs assigned by n00b for the purposes of thread identification.
 */
void
n00b_mdebug_other_thread(int64_t tid)
{
    int64_t start;
    int64_t i;

    start = atomic_load(&__n00b_m_debug_sequence);
    start &= N00B_DEBUG_RING_LAST_SLOT;

    for (i = start; i < N00B_DEBUG_RING_SIZE; i++) {
        if (tid == __n00b_m_debug[i].thread) {
            fprintf(stderr,
                    "%06llu: %s\n",
                    (unsigned long long)__n00b_m_debug[i].sequence,
                    __n00b_m_debug[i].msg);
        }
    }

    for (i = 0; i < start; i++) {
        if (tid == __n00b_m_debug[i].thread) {
            fprintf(stderr,
                    "%06llu: %s\n",
                    (unsigned long long)__n00b_m_debug[i].sequence,
                    __n00b_m_debug[i].msg);
        }
    }

    return;
}

/* n00b_mdebug_grep()
 *
 * Okay, this doesn't really "grep", but it searches the message field
 * of all ring buffer entries (from oldest to newest), looking for the
 * given substring.
 *
 * The searching is implemented using strstr, so definitely no regexps
 * work.
 */
void
n00b_mdebug_grep(char *s)
{
    int64_t start;
    int64_t i;

    start = atomic_load(&__n00b_m_debug_sequence);
    start &= N00B_DEBUG_RING_LAST_SLOT;

    for (i = start; i < N00B_DEBUG_RING_SIZE; i++) {
        if (strstr(__n00b_m_debug[i].msg, s)) {
            fprintf(stderr,
                    "%06llu: %s\n",
                    (unsigned long long)__n00b_m_debug[i].sequence,
                    __n00b_m_debug[i].msg);
        }
    }

    for (i = 0; i < start; i++) {
        if (strstr(__n00b_m_debug[i].msg, s)) {
            fprintf(stderr,
                    "%06llu: %s\n",
                    (unsigned long long)__n00b_m_debug[i].sequence,
                    __n00b_m_debug[i].msg);
        }
    }

    return;
}

/* n00b_mdebug_pgrep()
 *
 * If you pass in a pointer's value as an integer, constructs the
 * appropriate string for that pointer, and then calls
 * n00b_mdebug_grep() for you.
 */
void
n00b_mdebug_pgrep(uintptr_t n)
{
    char    s[N00B_PTR_CHRS + 1] = {};
    char   *p                    = s + N00B_PTR_CHRS;
    int64_t i;

    for (i = 0; i < N00B_PTR_CHRS; i++) {
        *--p = __n00b_m_hex_conversion_table[n & 0xf];
        n >>= 4;
    }

    n00b_mdebug_grep(s);

    return;
}

char *
n00b_m_log_string(char *topic, char *msg, int64_t priority, char *f, int l)
{
    uint64_t               mysequence;
    uint64_t               index;
    n00b_m_debug_record_t *record_ptr;
    n00b_tsi_t            *tsi = n00b_get_tsi_ptr();
    const char            *fmt = "tid=%x; ll=%d; [%s %s:%d]: %s";

    mysequence           = atomic_fetch_add(&__n00b_m_debug_sequence, 1);
    index                = mysequence & N00B_DEBUG_RING_LAST_SLOT;
    record_ptr           = &__n00b_m_debug[index];
    record_ptr->sequence = mysequence;
    record_ptr->thread   = tsi->thread_id;

    snprintf(record_ptr->msg,
             N00B_DEBUG_MSG_SIZE,
             fmt,
             (int)tsi->thread_id,
             priority,
             topic,
             f,
             l,
             msg);

    return record_ptr->msg;
}

#endif
