#pragma once
#include "n00b.h"

// I realize some of this is redundant, but it's just easier.

typedef union {
    struct sockaddr_in  v4;
    struct sockaddr_in6 v6;
    struct sockaddr_un  unix;
    struct sockaddr     generic;
} n00b_sockaddr_t;

// My understanding is that the start of every sockaddr type is a
// short int that holds the family, but if I change this to a union,
// it barfs. Which results in double-setting the field...
typedef struct {
    short int       family;
    n00b_string_t    *resolved_name;
    n00b_string_t    *service;
    n00b_sockaddr_t addr;
} n00b_net_addr_t;

static inline int
n00b_get_sockaddr_len(n00b_net_addr_t *addr)
{
    switch (addr->family) {
    case AF_UNIX:
        return sizeof(struct sockaddr_un);
    case AF_INET:
        return sizeof(struct sockaddr_in);
    case AF_INET6:
        return sizeof(struct sockaddr_in6);
    default:
        N00B_CRAISE("Invalid internal state for network address.");
    }
}

static inline uint16_t
n00b_get_net_addr_family(n00b_net_addr_t *addr)
{
    return addr->family;
}

static inline uint16_t
n00b_get_net_addr_port(n00b_net_addr_t *addr)
{
    switch (addr->family) {
    case AF_INET:
        return htons(addr->addr.v4.sin_port);
    case AF_INET6:
        return htons(addr->addr.v6.sin6_port);
    default:
        N00B_CRAISE("Address must be either ipv4 or ipv6 to have a port.");
    }
}

// Returns the raw address only, not the resolved address (if any).
extern n00b_string_t *n00b_net_addr_repr(n00b_net_addr_t *);

extern n00b_string_t *n00b_net_addr_dns_name(n00b_net_addr_t *);
extern n00b_string_t *n00b_net_addr_service_name(n00b_net_addr_t *);
