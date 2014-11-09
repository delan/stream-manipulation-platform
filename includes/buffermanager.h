
#ifndef BUFFERMGR_H
#define BUFFERMGR_H

#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include "list.h"

typedef struct
{
    struct list_head list;
    void *ptr;
    off_t pos;
    size_t size;
    size_t used;
    struct buffer_const *orig;
} buffer;

struct buffer_const
{
    struct list_head list;
    void *const_ptr;
    size_t const_size;
    int free_buf:1;
    int ref_count;
    time_t last_used;
    buffer b;
};

buffer *buffer_get(size_t min_size);
buffer *buffer_dup(buffer *b);
void buffer_recycle(buffer *buffer);
void buffer_garbage_collect(int age);

#endif // !BUFFERMGR_H
