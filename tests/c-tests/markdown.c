#include "n00b.h"
const char *MD_EX =
    "# This is a title.\n"
    "\n"
    "## This is a subtitle.\n"
    "Here's a block of *normal* text with some _highlights_.\n"
    "It runs a couple of lines, but not **too** long.\n"
    "\n\n"
    "Here's the next block of text, which **should** \n"
    "show up as its own separate paragraph.\n"
    "\n"
    "1. Hello\n"
    "2. Sailor\n"
    "\n"
    "- Hi\n"
    "- Mom\n"
    "\n\n"
    "### Bye!\n";

int
main()
{
    n00b_terminal_app_setup();

    n00b_string_t *md = n00b_cstring(MD_EX);
    //    n00b_table_t  *t  = n00b_repr_md_parse(n00b_parse_markdown(md));
    // n00b_eprint(t);
    n00b_eprint(n00b_markdown_to_table(md, true));
}
