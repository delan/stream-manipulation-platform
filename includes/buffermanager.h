
#ifndef BUFFERMGR_H
#define BUFFERMGR_H

#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include "list.h"

typedef struct
{
    struct list_head list;
    void *orig_ptr;
    void *ptr;
    size_t orig_size;
    size_t size;
    size_t used;
    off_t pos;
    time_t last_used;
    int free_buf:1;
    int ref_count;
} buffer;

buffer *buffer_get(size_t min_size);
void buffer_recycle(buffer *buffer);
void buffer_garbage_collect(int age);

static inline void *buffer_get_at_pos(buffer *b)
{
    return (void*)((uintptr_t)b->ptr + b->pos);
}

static inline void *buffer_get_at_end(buffer *b)
{
    return (void*)((uintptr_t)b->ptr + b->used);
}

#endif // !BUFFERMGR_H
