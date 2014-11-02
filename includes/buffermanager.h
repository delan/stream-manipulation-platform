
#ifndef BUFFERMGR_H
#define BUFFERMGR_H

#include <time.h>

#include "list.h"

typedef struct
{
    struct list_head list;
    void *ptr;
    size_t size;
    size_t used;
    off_t pos;
    time_t last_used;
} buffer;

buffer *buffer_get(size_t min_size);
void buffer_recycle(buffer *buffer);
void buffer_garbage_collect(int age);

#endif // !BUFFERMGR_H
