
#include <string.h>

#include "util.h"

void *memdup(void *s, size_t n)
{
    void *d = malloc(n);
    memcpy(d, s, n);
    return d;
}
