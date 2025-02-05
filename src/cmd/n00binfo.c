#include "n00b.h"

#define N00B_DEFAULT_UNIX_SOCKET "

int
main(int argc, char *argv[], char *envp[])
{
    n00b_string_t *socket_filename;

    if (argc <= 1) {
        socket_filename = n00b_cstring(".n00b_server");
    }
    else {
        socket_filename = n00b_cstring(argv[1]);
    }

    n00b_printf("«em1»Starting listener on unix socket: «#»", socket_filename);
}
