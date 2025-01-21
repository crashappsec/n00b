#include "n00b.h"

#define N00B_DEFAULT_UNIX_SOCKET "

int
main(int argc, char *argv[], char *envp[])
{
    n00b_utf8_t *socket_filename;

    if (argc <= 1) {
        socket_filename = n00b_new_utf8(".n00b_server");
    }
    else {
        socket_filename = n00b_new_utf8(argv[1]);
    }

    n00b_printf("[h1]Starting listener on unix socket:: {}", socket_filename);
}
