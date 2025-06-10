#include "n00b.h"

int
main()
{
    n00b_terminal_app_setup();

    n00b_print(n00b_crich("example [=h1=]bar "));
    n00b_print(n00b_crich("[=em2=]example[=/=] [=em1=]bar «em2»[=bah"));
    n00b_print(n00b_crich("«i»«b»hello,[=/b=] world!"));
    n00b_print(n00b_crich("«i»hell«b»«strikethrough»o, wo[=/=]rld!"));
    n00b_print(n00b_crich("«strikethrough»hello, wo[=/=]rld!"));
    n00b_print(n00b_crich("«u»hello, wo[=/=]rld!"));
    n00b_print(n00b_crich("«uu»hello, wo[=/=]rld!"));
    n00b_print(n00b_crich("«allcaps»hello, wo[=/=]rld!"));
    n00b_print(n00b_crich("«upper»hello, wo[=/=]rld!"));

    n00b_string_t *s = n00b_crich("example [=em1=]bar ");
    n00b_print(n00b_cformat("«caps»wow, bob, wow!", s));
    n00b_print(n00b_cformat("«caps»«#0:»wow", s));
    n00b_print(n00b_cformat("«upper»«#0:»wow", s));
    n00b_print(n00b_cformat("«upper»«#0:!»wow", s));
    n00b_print(n00b_cformat("«upper»«#1!»wow", s));
}
