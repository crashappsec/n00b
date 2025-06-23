#include "n00b.h"

static inline n00b_string_t *
n00b_token_get_location_str(n00b_token_t *t)
{
    if (t->module) {
        return n00b_cformat("«#»:«#»:«#»",
                            t->module->full_uri,
                            (int64_t)t->line_no,
                            (int64_t)t->line_offset + 1);
    }

    return n00b_cformat("«#»:«#»",
                        (int64_t)t->line_no,
                        (int64_t)t->line_offset + 1);
}
