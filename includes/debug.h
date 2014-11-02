
#ifndef DEBUG_H
#define DEBUG_H

#ifdef __DEBUG__
#include <stdio.h>
#define DPRINTF(fmt, ...) fprintf(stderr, "%s:%s:%d: " fmt, __FILE__, __func__, __LINE__, ## __VA_ARGS__);
#else
#define DPRINTF(...)
#endif

#endif // !DEBUG_H
