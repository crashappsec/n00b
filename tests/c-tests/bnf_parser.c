#include "n00b.h"

int main(void)
{
    n00b_terminal_app_setup();
    n00b_list_t *errors = NULL;
    n00b_string_t *text = n00b_cstring("<test> ::= \"a\"");
    n00b_dict_t *dict = n00b_bnf_parse(text, &errors);

    return 0;
}