#include "n00b.h"

int
main()
{
    n00b_terminal_app_setup();
    n00b_tree_node_t *layout = n00b_new_layout();
    n00b_new_layout_cell(layout,
                         n00b_kw("min",
                                 n00b_ka(10),
                                 "preference",
                                 n00b_ka(20)));
    n00b_new_layout_cell(layout,
                         n00b_kw("flex",
                                 n00b_ka(1),
                                 "preference",
                                 n00b_ka(20)));
    n00b_new_layout_cell(layout,
                         n00b_kw("flex",
                                 n00b_ka(1),
                                 "preference",
                                 n00b_ka(30)));
    n00b_new_layout_cell(layout,
                         n00b_kw("min",
                                 n00b_ka(5),
                                 "max",
                                 n00b_ka(10)));

    n00b_tree_node_t *t = n00b_layout_calculate(layout, 120);
    n00b_string_t    *s = n00b_to_string(t);
    n00b_eprintf("layout has «#:i» kids.\n", (int64_t)layout->num_kids);
    n00b_eprintf("res 1 has «#:i» kids.\n", (int64_t)t->num_kids);
    n00b_eprintf("«em2»Layout 1");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 20);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 2");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 19);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 3");
    n00b_eprint(s);
    t = n00b_layout_calculate(layout, 10);
    s = n00b_to_string(t);
    n00b_eprintf("«em2»Layout 4«/»\n«#»", s);
    n00b_debug_log_dump();
}
