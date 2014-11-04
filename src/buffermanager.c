
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "list.h"
#include "buffermanager.h"

#include "debug.h"

/** TODO:
  sort these in a self balancing binary tree by buffer size
*/

static LIST_HEAD(free_buffers);

buffer *buffer_get(size_t min_size)
{
    buffer *i;
    list_for_each_entry(i, &free_buffers, list)
    {
        if(min_size <= i->size)
        {
            list_del(&i->list);
            return i;
        }
    }

    /* round up to nearest 4KB */
    size_t buffer_size = ((min_size-1) & (~0xFFF)) + 0x1000;
    i = (buffer*)malloc(sizeof(buffer)+buffer_size);
    if(i == NULL)
    {
        DPRINTF("malloc returned null: %s (%d)\n", strerror(errno), errno);
        return NULL;
    }
    memset(i, '\0', sizeof(buffer));
    i->ptr = (void*)(i+1);
    i->size = buffer_size;
    return i;
}

void buffer_recycle(buffer *buf)
{
    buf->last_used = time(NULL);
    buf->used = 0;
    buf->pos = 0;
    list_add_tail(&buf->list, &free_buffers);
}

static void buffer_free(buffer *b)
{
    list_del(&b->list);

    /* the buffer struct and actual space is allocated in the same
       malloc, so the ptr inside buffer isn't free'd first */
    free(b);
}

void buffer_garbage_collect(int age)
{
    time_t current_time = time(NULL);

#ifdef __DEBUG__
    size_t count = 0;
#endif

    buffer *i, *j;
    list_for_each_entry_safe(i, j, &free_buffers, list)
    {
        DPRINTF("age: %lu (%d)\n", current_time - i->last_used, age);
        if(!age || current_time - i->last_used > age)
        {
#ifdef __DEBUG__
            count += i->size;
#endif
            buffer_free(i);
        }
        else if(age)
            break;
    }
#ifdef __DEBUG__
    if(count > 0)
        DPRINTF("Garbage collected %luKiB\n", count >> 10);
#endif
}
