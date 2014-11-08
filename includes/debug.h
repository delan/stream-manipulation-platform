
#ifndef DEBUG_H
#define DEBUG_H

#ifdef __DEBUG__
#include <stdio.h>
#include <libgen.h>
#include <assert.h>
#define ASSERT(a) assert(a);
#define DPRINTF(fmt, ...) fprintf(stderr, "%s:%s:%d: " fmt, basename(__FILE__), __func__, __LINE__, ## __VA_ARGS__);
#else
#define DPRINTF(...)
#define ASSERT(a)
#endif

#endif // !DEBUG_H
