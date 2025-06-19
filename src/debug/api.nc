#define N00B_USE_INTERNAL_API
#include "n00b.h"

static once n00b_dict_t *
get_debug_namespace(void)
{
    return n00b_dict(n00b_type_string(), n00b_type_ref());
}

static void *
start_debug_workflow(n00b_topic_info_t *tinfo, void *payload, void *unused)
{
    n00b_debug_msg_t *msg = n00b_gc_alloc_mapped(n00b_debug_msg_t,
                                                 N00B_GC_SCAN_ALL);
    msg->topic            = tinfo->name;
    msg->timestamp        = n00b_current_date_time();
    msg->workflow         = n00b_get_topic_workflow(msg->topic);
    msg->payload          = payload;

    n00b_apply_debug_workflow(msg);

    return NULL;
}

n00b_stream_t *
n00b_get_debug_topic(n00b_string_t *name)
{
    n00b_dict_t *ns = get_debug_namespace();

    n00b_stream_t *result = n00b_dict_get(ns, name, NULL);
    if (result) {
        return result;
    }

    return _n00b_create_topic_stream(name, ns, start_debug_workflow, NULL);
}

void
_n00b_debug(n00b_string_t *topic, void *msg)
{
    n00b_stream_t *stream = n00b_get_debug_topic(topic);
    n00b_write(stream, msg);
}

#ifdef N00B_DEBUG
void
n00b_debug_log_dump(void)
{
    n00b_stop_the_world();
    n00b_string_t *td  = n00b_get_env(N00B_DEBUG_DIR_ENV);
    int            pid = getpid();

    if (!td || !n00b_path_is_directory(td)) {
        td = n00b_new_temp_dir(n00b_cstring("n00b-"),
                               n00b_cformat("-dbg"));
    }
    printf("Dumping debug state to directory %s for pid %d\n", td->data, pid);
#include "util/nowarn.h"
    cprintf("Dumping debug state to directory %s for pid %d\n", td, pid);
#include "util/nowarn_pop.h"
    n00b_string_t *f1 = n00b_cformat("«#»/locks.«#».log", td, pid);
    n00b_debug_all_locks(f1->data);
    n00b_string_t *f2 = n00b_cformat("«#»/mem.«#».log", td, pid);
    n00b_mdebug_file(f2->data);
    n00b_string_t *f3 = n00b_cformat("«#»/streams.«#».log", td, pid);
    n00b_log_streams_to_file(f3->data);
#if defined(N00B_BACKTRACE_SUPPORTED)
    n00b_string_t *f4   = n00b_cformat("«#»/trace.«#».log", td, pid);
    char          *s    = n00b_backtrace_cstring();
    FILE          *file = fopen(f4->data, "w");
    fprintf(file, "%s\n", s);
    fclose(file);
    n00b_restart_the_world();
#endif
}

#endif
