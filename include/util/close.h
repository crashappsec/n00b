#pragma once

#define n00b_raw_fd_close(x) \
    {                        \
        if (x >= 0) {        \
            close(x);        \
            x = -1;          \
        }                    \
    }
