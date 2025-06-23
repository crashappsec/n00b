#include "n00b.h"

static inline void
set_v4(n00b_net_addr_t *obj, n00b_string_t *s, uint16_t port)
{
    if (inet_pton(AF_INET, s->data, &obj->addr.v4.sin_addr) <= 0) {
        N00B_CRAISE("Invalid ip address");
    }

    obj->addr.v4.sin_port   = htons(port);
    obj->addr.v4.sin_family = AF_INET;
    obj->family             = AF_INET;
}

static inline void
set_v6(n00b_net_addr_t *obj, n00b_string_t *s, uint16_t port)
{
    if (inet_pton(AF_INET6, s->data, &obj->addr.v6.sin6_addr) <= 0) {
        N00B_CRAISE("Invalid ip address");
    }

    obj->addr.v6.sin6_port   = htons(port);
    obj->addr.v6.sin6_family = AF_INET6;
    obj->family              = AF_INET6;
}

static inline void
set_unix(n00b_net_addr_t *obj, n00b_string_t *s)
{
    static int c = sizeof(struct sockaddr_un)
                 - sizeof(sa_family_t) - 1;

    if (!s) {
        N00B_CRAISE("Unix address instantiation requires a path");
    }
    int len = n00b_min(n00b_string_byte_len(s), c);

    memcpy(&obj->addr.sa_unix.sun_path, s->data, len);
    obj->addr.sa_unix.sun_family = AF_UNIX;
    obj->family                  = AF_UNIX;
}

static inline void
setup_from_sockaddr(n00b_net_addr_t *obj, struct sockaddr *sa)
{
    switch (sa->sa_family) {
    case AF_INET:
        obj->family = AF_INET;
        memcpy(&obj->addr.v4, sa, sizeof(struct sockaddr_in));
        break;
    case AF_INET6:
        obj->family = AF_INET6;
        memcpy(&obj->addr.v6, sa, sizeof(struct sockaddr_in6));
        break;
    case AF_UNIX:
        obj->family = AF_UNIX;
        memcpy(&obj->addr.sa_unix, sa, sizeof(struct sockaddr_un));
        break;
    default:
        N00B_CRAISE("Bad struct sockaddr");
    }
}

static void
perform_inline_resolve(n00b_net_addr_t *obj)
{
    char host[1024] = {
        0,
    };
    char service[128] = {
        0,
    };

    int err = getnameinfo(&obj->addr.generic,
                          n00b_get_sockaddr_len(obj),
                          host,
                          1024,
                          service,
                          128,
                          0); // NI_NAMEREQD ?

    if (err) {
        N00B_RAISE(n00b_cstring((char *)gai_strerror(err)));
    }

    obj->resolved_name = n00b_cstring(host);

    if (strlen(service)) {
        obj->service = n00b_cstring(service);
    }
}

static void
ipaddr_init(n00b_net_addr_t *obj, va_list args)
{
    keywords
    {
        n00b_string_t   *address              = NULL;
        int32_t          port                 = -1;
        bool             ipv6                 = false;
        bool             unix                 = false;
        bool             resolve(resolve_dns) = false;
        struct sockaddr *sockaddr             = NULL;
    }

    if (sockaddr) {
        setup_from_sockaddr(obj, sockaddr);
        if (resolve) {
            perform_inline_resolve(obj);
        }
        return;
    }

    if (ipv6 && unix) {
        N00B_CRAISE("v6 and unix are not compatable options.");
    }

    if ((!unix)
        && (port < 0 || port > 0xffff)) {
        N00B_CRAISE("Invalid port for IP address.");
    }

    if (address == NULL) {
        N00B_CRAISE("Address cannot be empty.");
    }

    if (ipv6) {
        set_v6(obj, address, (uint16_t)port);
    }
    else {
        if (unix) {
            set_unix(obj, address);
        }
        else {
            set_v4(obj, address, (uint16_t)port);
        }
    }
    n00b_assert(obj->family != 0);
    n00b_assert(obj->addr.v4.sin_family != 0);

    if (resolve) {
        perform_inline_resolve(obj);
    }
}

static inline n00b_string_t *
repr_inet(n00b_net_addr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {
        0,
    };
    struct sockaddr_in *addr = &(obj->addr.v4);

    int sz = sizeof(struct sockaddr_in);
    if (!inet_ntop(AF_INET, &(addr->sin_addr), buf, sz)) {
        n00b_raise_errno();
    }
    return n00b_cstring(buf);
}

static inline n00b_string_t *
repr_inet6(n00b_net_addr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {
        0,
    };

    struct sockaddr_in6 *addr = &(obj->addr.v6);
    int                  sz   = sizeof(struct sockaddr_in6);
    if (!inet_ntop(AF_INET6, &(addr->sin6_addr), buf, sz)) {
        n00b_raise_errno();
    }

    return n00b_cstring(buf);
}

static inline n00b_string_t *
repr_unix(n00b_net_addr_t *obj)
{
    return n00b_cformat("unix:«#»", n00b_cstring(obj->addr.sa_unix.sun_path));
}

n00b_string_t *
n00b_net_addr_repr(n00b_net_addr_t *obj)
{
    switch (obj->family) {
    case AF_INET:
        return repr_inet(obj);
    case AF_INET6:
        return repr_inet6(obj);
    case AF_UNIX:
        return repr_unix(obj);
    default:
        n00b_unreachable();
    }
}

static n00b_string_t *
ipaddr_repr(n00b_net_addr_t *obj)
{
    n00b_string_t *base = obj->resolved_name;
    int64_t        port = n00b_get_net_addr_port(obj);

    if (!base) {
        base = n00b_net_addr_repr(obj);
    }

    if (port) {
        return n00b_cformat("«#»:«#»", base, port);
    }
    return base;
}

n00b_string_t *
n00b_net_addr_dns_name(n00b_net_addr_t *obj)
{
    if (!obj->resolved_name) {
        perform_inline_resolve(obj);
    }

    return obj->resolved_name;
}

n00b_string_t *
n00b_net_addr_service_name(n00b_net_addr_t *obj)
{
    if (!obj->service) {
        perform_inline_resolve(obj);
    }

    return obj->service;
}

static inline n00b_net_addr_t *
unix_lit(n00b_string_t *in)
{
    n00b_string_t *s = n00b_string_slice(in, 5, -1);

    return n00b_new(n00b_type_address(), unix : true, address : s);
}

static n00b_net_addr_t *
ipaddr_lit(n00b_string_t        *s_u8,
           n00b_lit_syntax_t     st,
           n00b_string_t        *litmod,
           n00b_compile_error_t *err)
{
    if (n00b_string_starts_with(s_u8, n00b_cstring("unix:"))) {
        return unix_lit(s_u8);
    }

    n00b_net_addr_t *result = n00b_new(n00b_type_address());

    if (inet_pton(AF_INET, s_u8->data, result) == 1) {
        return result;
    }

    if (inet_pton(AF_INET6, s_u8->data, result) == 1) {
        return result;
    }

    *err = n00b_err_invalid_ip;

    return NULL;
}

const n00b_vtable_t n00b_ipaddr_vtable = {
    .methods = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)ipaddr_init,
        [N00B_BI_TO_STRING]    = (n00b_vtable_entry)ipaddr_repr,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)ipaddr_lit,
        [N00B_BI_FINALIZER]    = NULL,
    },
};
