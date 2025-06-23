#include "n00b.h"

static inline n00b_tree_props_t *
n00b_get_current_tree_formatting(void)
{
    n00b_theme_t *theme = n00b_get_current_theme();
    if (theme->tree_props.pad == 0) {
        theme = n00b_lookup_theme(n00b_cstring("n00b-default"));
    }

    return &theme->tree_props;
}
