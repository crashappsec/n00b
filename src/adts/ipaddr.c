#include "n00b.h"

void
n00b_ipaddr_set_address(n00b_ipaddr_t *obj, n00b_str_t *s, uint16_t port)
{
    s = n00b_to_utf8(s);

    obj->port = port;

    if (inet_pton(obj->af, s->data, (void *)obj->addr) <= 0) {
        N00B_CRAISE("Invalid ip address");
    }

    if (obj->af == AF_INET) {
        ((struct sockaddr_in *)&(obj->addr))->sin_port = port;
    }
    else {
        ((struct sockaddr_in6 *)&(obj->addr))->sin6_port = port;
    }
}

static void
ipaddr_init(n00b_ipaddr_t *obj, va_list args)
{
    n00b_str_t *address = NULL;
    int32_t    port    = -1;
    bool       ipv6    = false;

    n00b_karg_va_init(args);
    n00b_kw_ptr("address", address);
    n00b_kw_int32("port", port);
    n00b_kw_bool("ipv6", ipv6);

    if (ipv6) {
        obj->af = AF_INET6;
    }
    else {
        obj->af = AF_INET;
    }

    if (address != NULL) {
        if (port < 0 || port > 0xffff) {
            N00B_CRAISE("Invalid port for IP address.");
        }
        n00b_ipaddr_set_address(obj, address, (uint16_t)port);
    }
}

static n00b_str_t *
ipaddr_repr(n00b_ipaddr_t *obj)
{
    char buf[INET6_ADDRSTRLEN + 1] = {
        0,
    };

    if (!inet_ntop(obj->af, &obj->addr, buf, sizeof(struct sockaddr_in6))) {
        N00B_CRAISE("Unable to format ip address");
    }

    if (obj->port == 0) {
        return n00b_new(n00b_type_utf8(), n00b_kw("cstring", n00b_ka(buf)));
    }

    return n00b_str_concat(n00b_new(n00b_type_utf8(), n00b_kw("cstring", n00b_ka(buf))),
                          n00b_str_concat(n00b_get_colon_no_space_const(),
                                         n00b_str_from_int((int64_t)obj->port)));
}

static n00b_ipaddr_t *
ipaddr_lit(n00b_utf8_t          *s_u8,
           n00b_lit_syntax_t     st,
           n00b_utf8_t          *litmod,
           n00b_compile_error_t *err)
{
    n00b_ipaddr_t *result = n00b_new(n00b_type_ip());

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
    .num_entries = N00B_BI_NUM_FUNCS,
    .methods     = {
        [N00B_BI_CONSTRUCTOR]  = (n00b_vtable_entry)ipaddr_init,
        [N00B_BI_TO_STR]       = (n00b_vtable_entry)ipaddr_repr,
        [N00B_BI_GC_MAP]       = (n00b_vtable_entry)N00B_GC_SCAN_NONE,
        [N00B_BI_FROM_LITERAL] = (n00b_vtable_entry)ipaddr_lit,
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `n00b_stream_t` on my mac).
        [N00B_BI_FINALIZER]    = NULL,
    },
};
