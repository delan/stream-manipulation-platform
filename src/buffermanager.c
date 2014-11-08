
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
            i->ref_count = 1;
            list_del_init(&i->list);
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
    i->orig_ptr = i->ptr = (void*)(i+1);
    i->size = i->orig_size = buffer_size;
    i->free_buf = 0; // buf is part of this allocation
    i->pos = 0;
    i->ref_count = 1;
    INIT_LIST_HEAD(&i->list);
    return i;
}

buffer *buffer_wrap(void *p, size_t len)
{
    buffer *b = (buffer*)malloc(sizeof(buffer));
    b->ptr = p;
    b->size = len;
    b->free_buf = 1;
    INIT_LIST_HEAD(&b->list);

    return b;
}

void buffer_recycle(buffer *buf)
{
    if(--buf->ref_count == 0)
    {
        buf->last_used = time(NULL);
        buf->used = 0;
        buf->pos = 0;
        buf->ptr = buf->orig_ptr;
        buf->size = buf->orig_size;
        list_add_tail(&buf->list, &free_buffers);
    }
}

static void buffer_free(buffer *b)
{
    list_del(&b->list);

    if(b->free_buf)
        free(b->ptr);
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
