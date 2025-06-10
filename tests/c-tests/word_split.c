#include "n00b.h"

int
main()
{
    n00b_terminal_app_setup();

    n00b_string_t *for_testing = n00b_cstring(
        "  I   do not know-- what's the answer?!?!?!!!\n"
        "I know I'm asking for the 3,100,270.002th time!!\n");

    n00b_list_t *l1 = n00b_string_split_words(for_testing);

    for (int i = 0; i < n00b_list_len(l1); i++) {
        n00b_printf("Word [=#:i=]: [=#=]",
                    (int64_t)i,
                    n00b_list_get(l1, i, NULL));
    }
}
