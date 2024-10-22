#pragma once
#include "n00b.h"

// I realize some of this is redundant, but it's just easier.
typedef struct {
    char     addr[sizeof(struct sockaddr_in6)];
    uint16_t port;
    int32_t  af;
} n00b_ipaddr_t;

extern void
n00b_ipaddr_set_address(n00b_ipaddr_t *obj, n00b_str_t *s, uint16_t port);
