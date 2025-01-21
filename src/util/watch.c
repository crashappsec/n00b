#include "n00b.h"

#ifdef N00B_DEBUG

static n00b_watch_target n00b_watch_list[N00B_WATCH_SLOTS];
static n00b_watch_log_t  n00b_watch_log[N00B_WATCH_LOG_SZ];
static _Atomic int       next_log_id     = 0;
static _Atomic int       max_target      = 0;
static __thread bool     recursion_guard = false;

static void
watch_init()
{
    static int inited = false;

    if (!inited) {
        inited = true;
        n00b_gc_register_root(&n00b_watch_list[0], sizeof(n00b_watch_list) / 8);
        n00b_gc_register_root(&n00b_watch_log[0], sizeof(n00b_watch_log) / 8);
    }
}

void
_n00b_watch_set(void             *ptr,
                int               ix,
                n00b_watch_action ra,
                n00b_watch_action wa,
                bool              print,
                char             *file,
                int               line)
{
    watch_init();

    if (ix < 0 || ix >= N00B_WATCH_SLOTS) {
        abort();
    }

    uint64_t iv = *(uint64_t *)ptr;

    n00b_watch_list[ix] = (n00b_watch_target){
        .address      = ptr,
        .value        = iv,
        .read_action  = ra,
        .write_action = wa,
    };

    int high = atomic_read(&max_target);
    while (ix > high) {
        CAS(&max_target, &high, ix);
    }

    if (print) {
        n00b_printf("[atomic lime]Added watchpoint[/] @{:x} {}:{} (value: {:x})",
                    n00b_box_u64((uint64_t)ptr),
                    n00b_new_utf8(file),
                    n00b_box_u64(line),
                    n00b_box_u64(iv));
    }
}

static void
watch_print_log(int logid, int watchix)
{
    n00b_watch_log_t  entry = n00b_watch_log[logid % N00B_WATCH_LOG_SZ];
    n00b_watch_target t     = n00b_watch_list[watchix];

    if (entry.logid != logid) {
        n00b_printf(
            "[yellow]watch:{}:Warning:[/] log was dropped before it "
            "was reported; watch log size is too small.",
            logid);
        return;
    }

    if (!entry.changed) {
        n00b_printf("[atomic lime]{}:@{:x}: Unchanged[/] {}:{} (value: {:x})",
                    n00b_box_u64(logid),
                    n00b_box_u64((uint64_t)t.address),
                    n00b_new_utf8(entry.file),
                    n00b_box_u64(entry.line),
                    n00b_box_u64(entry.seen_value));
        return;
    }

    // If you get a crash inside the GC, comment out the printf instead.
    // printf("%p vs %p\n", (void *)entry.start_value, (void *)entry.seen_value);
    n00b_printf("[red]{}:@{:x}: Changed @{}:{} from [em]{:x}[/] to [em]{:x}",
                n00b_box_u64(logid),
                n00b_box_u64((uint64_t)t.address),
                n00b_new_utf8(entry.file),
                n00b_box_u64(entry.line),
                n00b_box_u64(entry.start_value),
                n00b_box_u64(entry.seen_value));
    return;
}

void
_n00b_watch_scan(char *file, int line)
{
    // Because we are using dynamic allocation when reporting, and
    // because we may want to stick this check inside the allocator as
    // a frequent place to look for watches, we want to prevent starting
    // one scan recursively if we already have one in progress.

    if (recursion_guard) {
        return;
    }
    else {
        recursion_guard = true;
    }

    int max = atomic_read(&max_target);
    int logid;
    int log_slot;

    for (int i = 0; i < max; i++) {
        n00b_watch_target t = n00b_watch_list[i];

        // If you want to ignore when it changes, you're turning the
        // whole watchpoint off; doesn't make much sense to
        // report on reads but ignore writes.
        if (!t.address || t.write_action == n00b_wa_ignore) {
            continue;
        }

        uint64_t cur     = *(uint64_t *)t.address;
        bool     changed = cur != t.value;

        logid                    = atomic_fetch_add(&next_log_id, 1);
        log_slot                 = logid % N00B_WATCH_LOG_SZ;
        n00b_watch_log[log_slot] = (n00b_watch_log_t){
            .file        = file,
            .line        = line,
            .start_value = t.value,
            .seen_value  = cur,
            .logid       = logid,
            .changed     = changed,
        };

        switch (changed ? t.write_action : t.read_action) {
        case n00b_wa_abort:
            watch_print_log(logid, i);
            n00b_printf("[red]Aborting for debugging.");
            abort();
        case n00b_wa_print:
            watch_print_log(logid, i);
            break;
        default:
            break;
        }

        if (changed) {
            t.value = cur;
        }
    }

    recursion_guard = false;
}

#endif
