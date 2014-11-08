
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "buffermanager.h"
#include "stringio.h"
#include "debug.h"

#define CLASS_NAME(a,b) a## StringIO ##b
VIRTUAL(Object)
END_VIRTUAL
#undef CLASS_NAME // StringIO

#define CLASS_NAME(a,b) a## MemStringIO ##b
static buffer *METHOD_IMPL(get_buffer_at_pos, off_t *pos)
{
    buffer *b;
    list_for_each_entry(b, &this->buffers, list)
    {
        if(*pos < b->used)
            return b;
        *pos -= b->used;
    }
    *pos = 0;
    return NULL;
}

static void METHOD_IMPL(construct)
{
    SUPER_CALL(Object, this, construct);
    INIT_LIST_HEAD(&this->buffers);
}

static void METHOD_IMPL(deconstruct)
{
    buffer *i, *j;
    list_for_each_entry_safe(i, j, &this->buffers, list)
    {
        list_del(&i->list);
        buffer_recycle(i);
        i = NULL;
    }
    SUPER_CALL(Object, this, deconstruct);
}

static size_t METHOD_IMPL(read, void *buf, size_t len)
{
    buffer *b = NULL;
    size_t read_count = 0;
    while(len > 0)
    {
        if(!this->current_buf || this->current_buf->pos == this->current_buf->used)
        {
            /* check if we are the last buffer */
            if(this->current_buf->list.next == &this->buffers)
                break;
            /* otherwise, go to next buffer */
            b = this->current_buf =
                list_entry(&this->current_buf->list, buffer, list);
        }
        else
            b = this->current_buf;

        size_t avail = b->used - b->pos;
        if(avail > len)
            avail = len;
        memcpy(
            buf,
            (void*)((uintptr_t)b->ptr + b->pos),
            avail
        );
        len -= avail;
        b->pos += avail;
        *(uintptr_t*)&buf += avail;
    }

    if(read_count == 0)
    {
        errno = EAGAIN;
        return -1;
    }
    this->current_pos += read_count;
    return read_count;
}

static size_t METHOD_IMPL(write, void *buf, size_t len)
{
    buffer *b = NULL;
    /* test if we need a new buffer */
    size_t written = 0;
    while(len > 0)
    {
        if(!this->current_buf ||
            this->current_buf->pos == this->current_buf->used)
        {
            b = buffer_get(this->new_buffer_size);
            if(!b)
            {
                errno = ENOMEM;
                return -1;
            }
            list_add_tail(&b->list, &this->buffers);
            this->current_buf = b;
        }
        else
            b = this->current_buf;

        size_t avail;
        if(b == list_tail(buffer, &this->buffers, list))
            avail = b->size - b->pos;
        else
            avail = b->used - b->pos;

        if(avail > len)
            avail = len;
        memcpy(
            (void*)((uintptr_t)b->ptr + b->pos),
            buf,
            avail
        );
        len -= avail;
        b->pos += avail;
        if(b->pos > b->used)
            b->pos = b->used;
        *(uintptr_t*)&buf += avail;
    }
    this->current_pos += written;
    return written;
}

static buffer *METHOD_IMPL(read_buffer)
{
    if(this->current_buf)
    {
        buffer *ret = this->current_buf;
        this->current_pos +=
            (ret->used - ret->pos);
        if(ret->list.next == &this->buffers)
            this->current_buf = NULL;
        else
            this->current_buf =
                list_entry(&ret->list.next, buffer, list);
        ret->ref_count++;
        return ret;
    }
    errno = EAGAIN;
    return NULL;
}

static int METHOD_IMPL(write_buffer, buffer *b)
{
    buffer *w = this->current_buf;
    size_t len = b->used;

    if(!w)
    {
        list_add(&b->list, &this->buffers);
        this->total_size = b->used;
        this->current_pos = b->used;
        this->current_buf = b;
        return 0;
    }
    else if(w->pos == 0)
    {
        /* insert buffer before */
        list_add_tail(&b->list, &w->list);
    }
    else
    {
        /* insert buffer after */
        list_add(&b->list, &w->list);
        if(w->pos != w->used)
        {
            if(len < w->used - w->pos)
            {
                /* we have to memcpy it in */
                memcpy(
                    (void*)((uintptr_t)w->ptr + w->pos),
                    b->ptr,
                    len
                );
                w->pos += len;
                this->current_pos += len;
                buffer_recycle(b);
                b = NULL;
                return 0;
            }
            
            len -= w->used - w->pos;
            w->used = w->pos;
        }
        if(b->list.next == &this->buffers)
            w = NULL;
        else
            w = list_entry(&b->list.next, buffer, list);
    }

    /* remove the same amount of data as the new buffer */
    while(len > 0 && w)
    {
        if(len >= w->used)
        {
            len -= w->used;
            /* we ran out of buffer to replace */
            if(w->list.next == &this->buffers)
                break;
            buffer *b = w;
            w = list_entry(&w->list.next, buffer, list);
            list_del(&b->list);
            buffer_recycle(b);
            b = NULL;
        }
        else
        {
            *(uintptr_t*)&w->ptr += len;
            w->size -= len;
            w->used -= len;
            w->pos = 0;
            break;
        }
    }
    this->current_buf = w;

    size_t replaced_count = this->current_pos - b->used;
    if(replaced_count < b->used)
        this->total_size += b->used - replaced_count;
    this->current_pos += b->used;

    return 0;
}

static off_t METHOD_IMPL(seek, off_t pos, int whence)
{
    if(whence == SEEK_END)
    {
        pos += this->total_size;
        whence = SEEK_SET;
    }
    else if(whence == SEEK_CUR)
    {
        pos += this->current_pos;
        whence = SEEK_SET;
    }

    if(whence == SEEK_SET)
    {
        if(pos == this->current_pos)
            return pos;
        if(pos < 0)
            pos = 0;
        if(pos > this->total_size)
            CALL((StringIO)this, truncate, pos);
        if(pos == 0)
        {
            if(!list_empty(&this->buffers))
            {
                this->current_buf = list_first(buffer, &this->buffers, list);
                this->current_buf->pos = 0;
            }
            this->current_pos = 0;
            return 0;
        }
        else if(pos == this->total_size)
        {
            this->current_buf = list_tail(buffer, &this->buffers, list);
            this->current_buf->pos =
                this->current_buf->used;
            return 0;
        }
        this->current_pos = pos;
        this->current_buf = PRIV_CALL(this, get_buffer_at_pos, &pos);
        this->current_buf->pos = pos;

        return this->current_pos;
    }
    errno = EINVAL;
    return -1;
}

static int METHOD_IMPL(truncate, off_t size)
{
    if(size < this->total_size)
    {
        size_t remove = this->total_size - size;
        /* iterate backwards */
        struct list_head *l, *i, *head = &this->buffers;
        buffer *b;

        /* list_for_each_prev_safe(l, i, head) */
        for (l = (head)->next, i = l->next; l != (head); \
                l = i, i = l->next)
        {
            b = list_entry(l, buffer, list);
            if(b->used < remove)
            {
                remove -= b->used;
                list_del(&b->list);
                buffer_recycle(b);
                b = NULL;
            }
            else
            {
                b->used -= remove;
                break;
            }
        }
    }
    else
    {
        size_t extension = size - this->total_size;

        /* max out all buffer space in tail buffer */
        if(!list_empty(&this->buffers))
        {
            buffer *b = list_tail(buffer, &this->buffers, list);
            off_t len = b->size - b->used;
            if(extension < len)
                len = extension;
            b->used = len;
            extension -= len;
        }
        /* add more buffers until we get there */
        while(extension > 0)
        {
            buffer *b = buffer_get(this->new_buffer_size);
            if(!b)
            {
                errno = ENOMEM;
                return -1;
            }
            list_add_tail(&b->list, &this->buffers);

            if(extension > b->size)
                b->used = b->size;
            else
                b->used = extension;

            /* calculate this as we go in case we fail with ENOMEM */
            this->total_size += b->used;

            extension -= b->size;
        }
    }
    this->total_size = size;
    return 0;
}

/* reverse truncate - adds / removes from the start */
static int METHOD_IMPL(rtruncate, off_t len)
{
    if(len == 0)
    {
        buffer *i, *j;
        list_for_each_entry_safe(i, j, &this->buffers, list)
        {
            DPRINTF("%p\n", i);
            list_del(&i->list);
            buffer_recycle(i);
            i = NULL;
        }
        this->total_size = 0;
        this->current_buf = NULL;
        this->current_pos = 0;
        return 0;
    }
    off_t diff = len - this->total_size;
    if(len < this->total_size)
    {
        size_t remove = this->total_size - len;
        DPRINTF("removing %lu bytes from start of stringio\n", remove);
        /* iterate backwards */
        buffer *b, *i;

        list_for_each_entry_safe(b, i, &this->buffers, list)
        {
            if(b->used <= remove)
            {
                remove -= b->used;
                list_del(&b->list);
                buffer_recycle(b);
                b = NULL;
            }
            else
            {
                b->used -= remove;
                *(uintptr_t*)&b->ptr += remove;
                b->size -= remove;
                break;
            }
        }
    }
    else
    {
        size_t extension = len - this->total_size;
        while(extension > 0)
        {
            buffer *b = buffer_get(this->new_buffer_size);
            if(!b)
            {
                off_t pos = this->current_pos;
                this->current_buf = PRIV_CALL(this, get_buffer_at_pos, &pos);
                this->current_buf->pos = pos;
                errno = ENOMEM;
                return -1;
            }
            list_add(&b->list, &this->buffers);

            if(extension > b->size)
                b->used = b->size;
            else
                b->used = extension;

            /* calculate this as we go in case we fail with ENOMEM */
            this->total_size += b->used;
            this->current_pos += b->used;

            extension -= b->size;
        }
    }
    this->total_size = len;
    this->current_pos += diff;
    if(this->current_pos < 0)
        this->current_pos = 0;
    off_t pos = this->current_pos;
    this->current_buf = PRIV_CALL(this, get_buffer_at_pos, &pos);
    this->current_buf->pos = pos;
    return 0;
}

buffer *METHOD_IMPL(get_current_buffer)
{
    buffer *w = this->current_buf;
    if(w && w->used == w->pos)
    {
        if(w->list.next == &this->buffers)
        {
            return NULL;
        }
        else
        {
            w = list_entry(w->list.next, buffer, list);
        }
        w->pos = 0;
        this->current_buf = w;
    }
    return w;
}

void METHOD_IMPL(update_current_buffer, size_t len)
{
    buffer *w = this->current_buf;
    w->pos += len;
    this->current_pos += len;

    size_t remove_count = w->pos - w->used;
    if(w->used < w->pos)
        w->used = w->pos;

    buffer *pending_free = NULL;
    while(remove_count > 0)
    {
        if(w->list.next == &this->buffers)
        {
            this->total_size += remove_count;
            break;
        }
        else
        {
            w = list_entry(w->list.next, buffer, list);
            if(pending_free)
            {
                list_del(&pending_free->list);
                buffer_recycle(pending_free);
                pending_free = NULL;
            }
            if(w->used > remove_count)
            {
                *(uintptr_t*)w->ptr += remove_count;
                w->used -= remove_count;
                w->size -= remove_count;
                w->pos = 0;
                break;
            }
            else
            {
                remove_count -= w->used;
                pending_free = w;
            }
        }
    }
    this->current_buf = w;
}

VIRTUAL(StringIO)
    VMETHOD_BASE(Object, construct);
    VMETHOD_BASE(Object, deconstruct);
    VMETHOD_BASE(StringIO, write);
    VMETHOD_BASE(StringIO, read);
    VMETHOD_BASE(StringIO, read_buffer);
    VMETHOD_BASE(StringIO, write_buffer);
    VMETHOD_BASE(StringIO, seek);
    VMETHOD_BASE(StringIO, truncate);
    VMETHOD_BASE(StringIO, rtruncate);

    VMETHOD(get_current_buffer);
    VMETHOD(update_current_buffer);

    VFIELD(current_buf) = NULL;
    VFIELD(current_pos) = 0;
    VFIELD(total_size) = 0;
    VFIELD(new_buffer_size) = 4096;
END_VIRTUAL
#undef CLASS_NAME

#define CLASS_NAME(a,b) a## Pipe ##b
static Pipe METHOD_IMPL(construct, StringIO base)
{
    SUPER_CALL(Object, this, construct);
    this->base = base;
    return this;
}

static size_t METHOD_IMPL(read, void *buf, size_t len)
{
    CALL(this->base, seek, 0, SEEK_SET);
    len = CALL(this->base, read, buf, len);
    size_t total_len = CALL(this->base, seek, 0, SEEK_END);
    CALL(this->base, rtruncate, total_len - len);
    return len;
}

static size_t METHOD_IMPL(write, void *buf, size_t len)
{
    CALL(this->base, seek, 0, SEEK_END);
    return CALL(this->base, write, buf, len);
}

static buffer *METHOD_IMPL(read_buffer)
{
    CALL(this->base, seek, 0, SEEK_SET);
    buffer *b = CALL(this->base, read_buffer);
    if(b)
    {
        size_t total_len = CALL(this->base, seek, 0, SEEK_END);
        CALL(this->base, rtruncate, total_len - b->used);
    }
    return b;
}

static int METHOD_IMPL(write_buffer, buffer *b)
{
    CALL(this->base, seek, 0, SEEK_END);
    return CALL(this->base, write_buffer, b);
}

static off_t METHOD_IMPL(seek, off_t offset, int whence)
{
    errno = ESPIPE;
    return -1;
}

static int METHOD_IMPL(truncate, size_t len)
{
    errno = EINVAL;
    return -1;
}

static int METHOD_IMPL(rtruncate, size_t len)
{
    errno = EINVAL;
    return -1;
}

VIRTUAL(StringIO)
    VMETHOD_BASE(Object, construct);
    VMETHOD_BASE(StringIO, read);
    VMETHOD_BASE(StringIO, write);
    VMETHOD_BASE(StringIO, read_buffer);
    VMETHOD_BASE(StringIO, write_buffer);
    VMETHOD_BASE(StringIO, seek);
    VMETHOD_BASE(StringIO, truncate);
    VMETHOD_BASE(StringIO, rtruncate);

    VFIELD(base) = NULL;
END_VIRTUAL
#undef CLASS_NAME // Pipe
