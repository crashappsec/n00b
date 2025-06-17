#define N00B_USE_INTERNAL_API
#include "n00b.h"

n00b_session_state_t *
n00b_session_find_state(n00b_session_t *s, n00b_string_t *name)
{
    if (!s->user_states) {
        return s->global_actions;
    }

    return hatrack_dict_get(s->user_states, name, NULL);
}

static inline n00b_string_t *
build_target_name(n00b_capture_t kind, bool regex)
{
    n00b_string_t *s;
    n00b_list_t   *parts = n00b_list(n00b_type_string());
    int            info  = N00B_CAPTURE_STDIN | N00B_CAPTURE_INJECTED
             | N00B_CAPTURE_STDOUT | N00B_CAPTURE_STDERR;

    info = kind & info;

    if (regex) {
        s = n00b_cstring("regex match");
    }
    else {
        s = n00b_cstring("exact match");
    }

    if (info != kind) {
        s = n00b_string_concat(s, n00b_cformat(" «em»(bad spec)"));
    }

    if (!info) {
        return n00b_string_concat(s, n00b_cformat(" «em»missing spec"));
    }

    if (kind & N00B_CAPTURE_STDIN) {
        n00b_private_list_append(parts, n00b_cstring("user input"));
    }
    if (kind & N00B_CAPTURE_INJECTED) {
        n00b_private_list_append(parts, n00b_cstring("injected input"));
    }
    if (kind & N00B_CAPTURE_STDOUT) {
        n00b_private_list_append(parts, n00b_cstring("output"));
    }
    if (kind & N00B_CAPTURE_STDERR) {
        n00b_private_list_append(parts, n00b_cstring("errors"));
    }

    return n00b_cformat("«#» «#»",
                        s,
                        n00b_string_join(parts, n00b_cached_comma_padded()));
}

static inline void
add_one_trigger(n00b_table_t *tbl, n00b_string_t *state_name, n00b_trigger_t *t)
{
    n00b_string_t *s;

    n00b_table_add_cell(tbl, state_name);

    switch (t->kind) {
    case N00B_TRIGGER_SUBSTRING:
        s = n00b_string_escape(t->match_info.substring);
        n00b_table_add_cell(tbl, build_target_name(t->target, false));
        n00b_table_add_cell(tbl, s);
        break;
    case N00B_TRIGGER_PATTERN:
        s = n00b_string_escape((n00b_string_t *)t->match_info.regexp);
        n00b_table_add_cell(tbl, build_target_name(t->target, true));
        n00b_table_add_cell(tbl, s);
        break;
    case N00B_TRIGGER_TIMEOUT:
        n00b_table_add_cell(tbl, n00b_cstring("Timeout"));
        n00b_table_add_cell(tbl, n00b_cstring(" "));
        break;
    case N00B_TRIGGER_ANY_EXIT:
        n00b_table_add_cell(tbl, n00b_cstring("Prompt"));
        n00b_table_add_cell(tbl, n00b_cstring(" "));
        break;
    case N00B_TRIGGER_ALWAYS:
        n00b_table_add_cell(tbl, n00b_cstring("Always"));
        n00b_table_add_cell(tbl, n00b_cstring(" "));
        break;
    case N00B_TRIGGER_RESIZE:
        n00b_table_add_cell(tbl, n00b_cstring("Resize"));
        n00b_table_add_cell(tbl, n00b_cstring(" "));
        break;
    }

    if (t->next_state) {
        n00b_table_add_cell(tbl, t->next_state);
    }
    else {
        n00b_table_add_cell(tbl, state_name);
    }

    if (t->fn) {
        n00b_table_add_cell(tbl, n00b_cformat("callback @«#:p»", t->fn));
    }
    else {
        n00b_table_add_cell(tbl, n00b_cstring(" "));
    }
}

n00b_table_t *
n00b_session_state_repr(n00b_session_t *s)
{
    n00b_set_t     *memos    = n00b_set(n00b_type_string());
    n00b_table_t   *tbl      = n00b_table(columns : 5);
    n00b_list_t    *worklist = n00b_list(n00b_type_string());
    n00b_trigger_t *t;

    n00b_table_add_cell(tbl, n00b_cstring("Name"));
    n00b_table_add_cell(tbl, n00b_cstring("Trigger"));
    n00b_table_add_cell(tbl, n00b_cstring("Argument"));
    n00b_table_add_cell(tbl, n00b_cstring("Next State"));
    n00b_table_add_cell(tbl, n00b_cstring("Callback"));

    if (!s->start_state) {
        goto add_global;
    }

    n00b_private_list_append(worklist, s->start_state);
    hatrack_set_put(memos, s->start_state);

    while (n00b_list_len(worklist)) {
        n00b_string_t        *state_name = n00b_list_pop(worklist);
        n00b_session_state_t *state      = hatrack_dict_get(s->user_states,
                                                       state_name,
                                                       NULL);

        if (!state) {
            n00b_table_add_cell(tbl, state_name);
            n00b_table_add_cell(tbl,
                                n00b_cstring("«red»Error:«/» no state info "
                                             "found."),
                                -1);
            continue;
        }

        int n = n00b_list_len((n00b_list_t *)state);

        for (int i = 0; i < n; i++) {
            t = n00b_list_get((n00b_list_t *)state, i, NULL);
            add_one_trigger(tbl, state_name, t);
            if (t->next_state && !hatrack_set_contains(memos, t->next_state)) {
                hatrack_set_put(memos, t->next_state);
                n00b_private_list_append(worklist, t->next_state);
            }
        }
    }

    n00b_list_t *items = n00b_dict_keys(s->user_states);
    int          n     = n00b_list_len(items);

    for (int i = 0; i < n; i++) {
        n00b_string_t *name = n00b_list_get(items, i, NULL);

        if (!hatrack_set_contains(memos, name)) {
            n00b_table_add_cell(tbl,
                                n00b_cformat("«red»Error:«/» state «#» "
                                             "is defined, but unreachable.",
                                             name),
                                -1);
        }
    }

add_global:
    if (s->global_actions) {
        n00b_table_add_cell(tbl, n00b_crich("«em4»Global defaults"), -1);
        n                    = n00b_list_len((n00b_list_t *)s->global_actions);
        n00b_string_t *empty = n00b_cstring(" ");

        for (int i = 0; i < n; i++) {
            t = n00b_list_get((n00b_list_t *)s->global_actions, i, NULL);
            add_one_trigger(tbl, empty, t);
        }
    }

    return tbl;
}

void
n00b_truncate_all_match_data(n00b_session_t *s,
                             n00b_string_t  *trunc,
                             n00b_capture_t  kind)
{
    // TODO: replace for kind.
    // ALSO, somewhere else, enforce a max buffer size.
    //
    if (kind == N00B_CAPTURE_STDIN) {
        s->input_match_buffer = trunc;
    }
    else {
        s->input_match_buffer = NULL;
    }

    if (kind == N00B_CAPTURE_INJECTED) {
        s->injection_match_buffer = trunc;
    }
    else {
        s->injection_match_buffer = NULL;
    }
    if (kind == N00B_CAPTURE_STDOUT) {
        s->stdout_match_buffer = trunc;
    }
    else {
        s->stdout_match_buffer = NULL;
    }
    if (kind == N00B_CAPTURE_STDERR) {
        s->stderr_match_buffer = trunc;
    }
    else {
        s->stderr_match_buffer = NULL;
    }
    s->launch_command = NULL;
    s->got_user_input = false;
    s->got_injection  = false;
    s->got_stdout     = false;
    s->got_stderr     = false;
    s->got_prompt     = false;
    s->got_run        = false;
    s->got_winch      = false;
    s->got_timeout    = false;
}

static void
successful_match(n00b_session_t *session,
                 n00b_trigger_t *trigger,
                 n00b_string_t  *str,
                 int             last,
                 n00b_capture_t  kind,
                 void           *match)
{
    trigger->cur_match = match;

    if (trigger->fn) {
        (*trigger->fn)(session, trigger, match);
    }

    if (trigger->next_state) {
        n00b_session_state_t *l = n00b_session_find_state(session,
                                                          trigger->next_state);
        if (l) {
            session->cur_user_state = l;
            session->cur_state_name = trigger->next_state;
        }
        else {
            N00B_RAISE(n00b_cformat("No such state: «#»",
                                    trigger->next_state));
        }
        n00b_dlog_io("session: Moving to state %s due to match.",
                     trigger->next_state->data);
    }
    else {
        if (trigger->kind == N00B_TRIGGER_TIMEOUT) {
            session->early_exit = true;
        }
    }

    if (!str || last == n00b_string_codepoint_len(str)) {
        n00b_truncate_all_match_data(session, NULL, 0);
    }
    else {
        n00b_string_t *s = n00b_string_slice(str, last, -1);
        n00b_truncate_all_match_data(session, s, kind);
    }
}

static bool
try_pattern_match(n00b_session_t *session,
                  n00b_string_t  *str,
                  n00b_trigger_t *trigger,
                  n00b_capture_t  kind)
{
    n00b_regex_t *re    = trigger->match_info.regexp;
    n00b_match_t *match = n00b_match(re, str);

    if (!match) {
        return false;
    }
    successful_match(session, trigger, str, match->end, kind, match);

    return true;
}

static bool
try_substring_match(n00b_session_t *session,
                    n00b_string_t  *str,
                    n00b_trigger_t *trigger,
                    n00b_capture_t  kind)
{
    if (!str) {
        return false;
    }

    n00b_string_t *sub   = trigger->match_info.substring;
    int64_t        where = n00b_string_find(str, sub);

    if (where == -1) {
        return false;
    }

    int last = where + n00b_string_codepoint_len(sub);
    successful_match(session, trigger, str, last, kind, sub);

    return true;
}

static n00b_trigger_t *
match_base(n00b_session_t       *s,
           n00b_string_t        *str,
           n00b_capture_t        kind,
           n00b_session_state_t *l)
{
    int n = n00b_list_len((n00b_list_t *)l);

    for (int i = 0; i < n; i++) {
        n00b_trigger_t *trigger = n00b_list_get((n00b_list_t *)l, i, NULL);

        if (trigger->kind == N00B_TRIGGER_ALWAYS) {
            successful_match(s, trigger, NULL, 0, kind, trigger->thunk);
            return trigger;
        }
        if (trigger->kind == N00B_TRIGGER_ANY_EXIT) {
            successful_match(s, trigger, NULL, 0, kind, trigger->thunk);
            return trigger;
        }
        if (trigger->kind == N00B_TRIGGER_RESIZE) {
            successful_match(s, trigger, NULL, 0, kind, trigger->thunk);
            return trigger;
        }
        if (trigger->target == kind) {
            if (trigger->kind == N00B_TRIGGER_SUBSTRING) {
                if (try_substring_match(s, str, trigger, kind)) {
                    return trigger;
                }
            }
            else {
                if (trigger->kind == N00B_TRIGGER_PATTERN
                    && try_pattern_match(s, str, trigger, kind)) {
                    return trigger;
                }
            }
        }

        if (trigger->kind == N00B_TRIGGER_TIMEOUT
            && kind == N00B_CAPTURE_TIMEOUT) {
            successful_match(s, trigger, NULL, 0, kind, trigger->thunk);
            return trigger;
        }
    }

    return NULL;
}

static n00b_trigger_t *
try_match(n00b_session_t *s, n00b_string_t *str, n00b_capture_t kind)
{
    n00b_trigger_t *result = NULL;

    if (s->early_exit) {
        return NULL;
    }

    if (s->cur_user_state) {
        result = match_base(s, str, kind, s->cur_user_state);
    }
    if (!result && s->global_actions) {
        result = match_base(s, str, kind, s->global_actions);
    }

    return result;
}

static inline bool
handle_input_matching(n00b_session_t *s)
{
    // This is more complicated than most matching, because we wait
    // until after the state machine processes it to decide what
    // to proxy down to the subprocess (in case some user input is
    // meant to be a control command only).
    n00b_string_t  *to_write = s->input_match_buffer;
    n00b_trigger_t *trigger  = try_match(s,
                                        s->input_match_buffer,
                                        N00B_CAPTURE_STDIN);
    if (!s->passthrough_input) {
        return trigger != NULL;
    }
    if (!trigger || !trigger->quiet) {
        n00b_queue(s->subprocess->subproc_stdin, to_write);
        s->got_user_input = false;
        return true;
    }

    int64_t mstart, mend;
    if (trigger->kind == N00B_TRIGGER_SUBSTRING) {
        mstart = n00b_string_find(to_write, trigger->match_info.substring);
        mend   = mstart + trigger->match_info.substring->codepoints;
    }
    else {
        n00b_match_t *m = trigger->cur_match;
        mstart          = m->start;
        mend            = m->end;
    }

    trigger->cur_match = NULL;

    n00b_string_t *slice;

    if (mstart != 0) {
        assert(mstart != -1);
        slice = n00b_string_slice(to_write, 0, mstart);
        n00b_queue(s->subprocess->subproc_stdin, slice);
    }
    if (mend != to_write->codepoints) {
        slice = n00b_string_slice(to_write, mend, -1);
        n00b_queue(s->subprocess->subproc_stdin, slice);
    }

    return true;
}

void
n00b_session_scan_for_matches(n00b_session_t *s)
{
    n00b_trigger_t *last = NULL;
    n00b_trigger_t *cur;

    while (true) {
        // Skip through "any" matches.
        cur = try_match(s, NULL, N00B_CAPTURE_ANY);
        if (last == cur) {
            break;
        }
        last = cur;
    }

    if (s->got_winch) {
        try_match(s, NULL, N00B_CAPTURE_WINCH);
        s->got_winch = false;
    }

    // The first shell prompt we get is from us starting up, it's not
    // an exit event. We track it by removing the unneeded reference
    // to the control stream (since we're already subscribed).
    if (s->got_prompt && s->subproc_ctrl_stream) {
        s->subproc_ctrl_stream = NULL;
        return;
    }

    // When we get here, we're looking at any data that came in async
    // since the last scan.
    //
    // For each item, we look at the current state to see if there
    // might be something to check. If not, we truncate the associated
    // buffer.  If there is, we check all appropriate rules. Currently
    // we are not combining regexes.
    if (s->got_user_input) {
        if (handle_input_matching(s)) {
            return;
        }
    }
    else {
        s->got_user_input = false;
    }

    // No state, no scanning. This does need to happen after we
    // process user input though, because we don't proxy user input
    // until after the state machine gets a chance to pass on the
    // input.
    if (!s->cur_user_state) {
        n00b_truncate_all_match_data(s, NULL, 0);
        return;
    }

    if (s->got_injection) {
        if (try_match(s, s->injection_match_buffer, N00B_CAPTURE_INJECTED)) {
            return;
        }
        else {
            s->got_injection = false;
        }
    }

    if (s->got_stdout) {
        if (try_match(s, s->stdout_match_buffer, N00B_CAPTURE_STDOUT)) {
            while (try_match(s, s->stdout_match_buffer, N00B_CAPTURE_STDOUT))
                ;
            return;
        }
        else {
            s->got_stdout = false;
        }
    }

    if (s->got_stderr) {
        if (try_match(s, s->stderr_match_buffer, N00B_CAPTURE_STDERR)) {
            while (try_match(s, s->stderr_match_buffer, N00B_CAPTURE_STDERR))
                ;
            return;
        }
        else {
            s->got_stderr = false;
        }
    }

    if (s->got_prompt) {
        if (try_match(s, NULL, N00B_CAPTURE_CMD_EXIT)) {
            return;
        }
        else {
            s->got_prompt = false;
        }
    }

    if (s->got_run) {
        s->got_run = false;
        if (try_match(s, s->launch_command, N00B_CAPTURE_CMD_RUN)) {
            return;
        }

        s->got_run = false;
    }

    if (s->got_timeout) {
        if (try_match(s, NULL, N00B_CAPTURE_TIMEOUT)) {
            return;
        }

        // Unhandled timeout; that leads to an exit.
        s->early_exit = true;
    }
}

static void
n00b_session_trigger_init(n00b_trigger_t *trigger, va_list args)
{
    keywords
    {
        n00b_session_state_type *state              = NULL;
        n00b_regex_t            *regexp             = NULL;
        n00b_string_t           *substring          = NULL;
        n00b_trigger_fn          fn(callback)       = NULL;
        n00b_string_t           *next_state         = NULL;
        bool                     on_typing          = false;
        bool                     on_stdout          = false;
        bool                     on_stderr          = false;
        bool                     on_injections      = false;
        bool                     auto_trigger(auto) = false;
        bool                     on_timeout         = false;
        bool                     on_subproc_exit    = false;
        bool                     quiet              = false;
    }

    int n = (int)on_typing + (int)on_stdout + (int)on_stderr
          + (int)on_injections + (int)on_timeout + (int)on_subproc_exit
          + (int)auto_trigger;

    if (n != 1) {
        N00B_CRAISE("Trigger must have exactly one target");
    }

    if ((regexp || substring) && (on_timeout || on_subproc_exit)) {
        N00B_CRAISE("Cannot string match for that trigger type");
    }

    if (state) {
        n00b_private_list_append((n00b_list_t *)state, trigger);
    }

    if (next_state)
        n00b_assert(n00b_type_is_string(n00b_get_my_type(next_state)));
    trigger->next_state = next_state;
    trigger->fn         = fn;
    trigger->quiet      = quiet;

    if (on_timeout) {
        trigger->kind = N00B_TRIGGER_TIMEOUT;
        return;
    }
    if (on_subproc_exit) {
        trigger->kind = N00B_TRIGGER_ANY_EXIT;
        return;
    }

    if (auto_trigger) {
        trigger->kind   = N00B_TRIGGER_ALWAYS;
        trigger->target = N00B_CAPTURE_ANY;
        return;
    }

    if ((!regexp && !substring) || (regexp && substring)) {
        N00B_CRAISE("Must provide either a regex or a substring");
    }

    if (regexp) {
        trigger->kind              = N00B_TRIGGER_PATTERN;
        trigger->match_info.regexp = regexp;
    }
    else {
        trigger->kind                 = N00B_TRIGGER_SUBSTRING;
        trigger->match_info.substring = substring;
    }
    if (on_typing) {
        trigger->target = N00B_CAPTURE_STDIN;
        return;
    }
    if (on_stdout) {
        trigger->target = N00B_CAPTURE_STDOUT;
        return;
    }
    if (on_stderr) {
        trigger->target = N00B_CAPTURE_STDERR;
        return;
    }
    if (on_injections) {
        trigger->target = N00B_CAPTURE_INJECTED;
        return;
    }

    n00b_unreachable();
}

static inline n00b_builtin_t
n00b_get_action_type(void *action)
{
    if (!n00b_in_heap(action)) {
        return N00B_T_CALLBACK;
    }

    return n00b_get_my_type(action)->base_index;
}

// We provide a bunch of helpers for instantiating triggers.
n00b_trigger_t *
__n00b_trigger(n00b_session_t *session,
               char           *trigger_name,
               void           *cur_state,
               void           *match,
               void           *action,
               bool            quiet)

{
    n00b_session_state_t *cur;

    if (n00b_type_is_string(n00b_get_my_type(cur_state))) {
        cur = n00b_get_session_state_by_name(session, cur_state);
    }
    else {
        cur = cur_state;
    }

    n00b_builtin_t action_type = n00b_get_action_type(action);
    char          *atkw        = "next_state";
    char          *mkkw        = "substring";
    n00b_type_t   *t           = n00b_get_my_type(match);

    if (match) {
        if (n00b_type_is_string(t)) {
            mkkw = "substring";
        }
        else {
            mkkw = "regexp";
        }
    }

    if (action_type == N00B_T_CALLBACK) {
        atkw = "callback";
    }
    else {
        assert(action_type == N00B_T_STRING);
    }

    n00b_trigger_t *result = n00b_new(n00b_type_session_trigger(),
                                      n00b_kargs_obj("state",
                                                     (int64_t)cur,
                                                     atkw,
                                                     action,
                                                     mkkw,
                                                     match,
                                                     trigger_name,
                                                     (int64_t) true,
                                                     "quiet",
                                                     (int64_t)quiet,
                                                     NULL));

    if (!cur) {
        if (!session->global_actions) {
            session->global_actions = n00b_list(n00b_type_ref());
        }
        n00b_private_list_append(session->global_actions, result);
    }

    return result;
}

static void
n00b_session_state_init(n00b_session_state_t *state, va_list args)
{
    n00b_session_t *s = va_arg(args, n00b_session_t *);
    n00b_string_t  *n = va_arg(args, n00b_string_t *);
    n00b_string_t  *err;

    n00b_list_init((n00b_list_t *)state, args);
    if (!s->user_states) {
        s->user_states = n00b_dict(n00b_type_string(),
                                   n00b_type_session_state());
    }
    if (!hatrack_dict_add(s->user_states, n, state)) {
        err = n00b_cformat("State name «em»«#» already exists.", n);
        N00B_RAISE(err);
    }
}

const n00b_vtable_t n00b_session_state_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_state_init,
    },
};

const n00b_vtable_t n00b_session_trigger_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR] = (n00b_vtable_entry)n00b_session_trigger_init,
    },
};
