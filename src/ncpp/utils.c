#include "ncpp.h"

buf_t *
concat(buf_t *b, char *start, int len)
{
    int total = len;

    if (b) {
        total += b->len;
    }
    buf_t *result = calloc(1, total + sizeof(buf_t));
    char  *p      = result->data;
    result->len   = total;

    if (b) {
        memcpy(p, b->data, b->len);
        p += b->len;
        free(b);
    }

    memcpy(p, start, len);

    return result;
}

buf_t *
concat_static(buf_t *b, char *s)
{
    return concat(b, s, strlen(s));
}

buf_t *
read_file(char *fname)
{
    FILE *f = fopen(fname, "r");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END)) {
        return NULL;
    }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET)) {
        return NULL;
    }

    buf_t *result = calloc(1, sz + sizeof(buf_t));
    result->len   = fread(result->data, 1, sz, f);

    fclose(f);

    return result;
}

bool
write_to_file(FILE *f, char *p, int len)
{
    while (len) {
        int l = fwrite(p, 1, len, f);

        if (!l) {
            return false;
        }

        p += l;
        len -= l;
    }

    return true;
}

tok_t *
alloc_tokens(buf_t *b)
{
    int len = sizeof(tok_t) * b->len;
    int ps  = getpagesize();

    if (len % ps) {
        int pages = (len / ps) + 1;
        len       = pages * ps;
    }

    return mmap(NULL,
                len,
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANON,
                -1,
                0);
}
