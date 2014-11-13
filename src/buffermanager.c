
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
static LIST_HEAD(avail_meta);

static buffer *get_meta()
{
    if(list_empty(&avail_meta))
    {
        return (buffer*)malloc(sizeof(buffer));
    }
    else
    {
        buffer *b = list_first(buffer, &avail_meta, list);
        ASSERT((void*)b != (void*)&avail_meta)
        list_del(&b->list);
        return b;
    }
}

buffer *buffer_get(size_t min_size)
{
    struct buffer_const *i;
    list_for_each_entry(i, &free_buffers, list)
    {
        if(min_size <= i->const_size)
        {
            i->ref_count = 1;
            list_del(&i->list);
            return &i->b;
        }
    }

    /* round up to nearest 4KB */
    size_t buffer_size = ((min_size-1) & (~0xFFF)) + 0x1000;
    i = (struct buffer_const*)malloc(
        sizeof(struct buffer_const) +
        buffer_size);

    if(i == NULL)
    {
        DPRINTF("malloc returned null: %s (%d)\n", strerror(errno), errno);
        return NULL;
    }

    buffer *b = &i->b;
    i->const_ptr  = b->ptr  = (void*)(b+1);
    i->const_size = b->size = buffer_size;
    i->free_buf = 0; // buf is part of this allocation
    i->ref_count = 1;
    i->list.next = i->list.prev = NULL;

    b->pos = 0;
    b->used = 0;
    b->orig = i;

    return b;
}

buffer *buffer_wrap(void *p, size_t len)
{
    struct buffer_const *i = (struct buffer_const*)malloc(
        sizeof(struct buffer_const));
    buffer *b = &i->b;
    i->const_ptr  = b->ptr  = p;
    i->const_size = b->size = len;
    i->free_buf = 1;
    i->ref_count = 1;
    INIT_LIST_HEAD(&i->list);

    b->used = b->size;
    b->orig = i;

    return b;
}

buffer *buffer_dup(buffer *b)
{
    buffer *dup = get_meta();
    memcpy(dup, b, sizeof(buffer));
    b->orig->ref_count++;
    dup->list.next = dup->list.prev = NULL;
    return dup;
}

void buffer_recycle(buffer *buf)
{
    ASSERT(buf->list.next == NULL && buf->list.prev == NULL);
    struct buffer_const *b = buf->orig;
    buf->ptr = b->const_ptr;
    buf->pos = 0;
    buf->size = b->const_size;
    buf->used = 0;
    if(--buf->orig->ref_count == 0)
    {
        DPRINTF("recycling buffer %p\n", buf->orig->const_ptr);
        b->last_used = time(NULL);
        list_add_tail(&buf->orig->list, &free_buffers);
    }
    if(buf != &buf->orig->b)
    {
        list_add(&buf->list, &avail_meta);
    }
}

static void buffer_free(struct buffer_const *b)
{
    list_del(&b->list);

    if(b->free_buf)
        free(b->const_ptr);
    free(b);
}

void buffer_garbage_collect(int age)
{
    time_t current_time = time(NULL);

#ifdef __DEBUG__
    size_t count = 0;
#endif

    struct buffer_const *i, *j;
    list_for_each_entry_safe(i, j, &free_buffers, list)
    {
        if(!age || current_time - i->last_used > age)
        {
#ifdef __DEBUG__
            count += i->const_size;
#endif
            buffer_free(i);
        }
        else if(age)
            break;
    }
    if(!age)
    {
        buffer *i, *j;
        list_for_each_entry_safe(i, j, &avail_meta, list)
        {
            list_del(&i->list);
            free(i);
        }
    }
#ifdef __DEBUG__
    if(count > 0)
        DPRINTF("Garbage collected %luKiB\n", count >> 10);
#endif
}
