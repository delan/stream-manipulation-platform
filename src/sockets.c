
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "sockets.h"
#include "eventmanager.h"
#include "buffermanager.h"

#define SOCKET_BUFFER_SIZE      (128*1024) // 128KiB
#define SOCKET_DEFAULT_MAX_MEM  (1024*1024)

static LIST_HEAD(sockets);

static int read_callback(event e, struct event_info *info)
{
    DPRINTF("read_callback\n");
    Socket this = (Socket)info->context;
    CALL((StringIO)this->__read_buffers, seek, 0, SEEK_END);
    buffer *b = CALL(this->__read_buffers, get_current_buffer);
    char new_buffer = 0;
    /* check to see if our current buffer has space left */
    if(b == NULL)
    {
        b = buffer_get(SOCKET_BUFFER_SIZE);
        if(b == NULL)
        {
            DPRINTF("buffer_get gave us a NULL buffer\n");
            DELETE(this);
            return EV_DONE;
        }
        new_buffer = 1;
    }

    int read_count = recv(
        info->fd, 
        (void*)((uintptr_t)b->ptr + b->pos),
        b->size - b->pos, MSG_DONTWAIT);

    DPRINTF("recv() returned: %d\n", read_count);

    if(read_count == -1)
    {
        if(new_buffer)
        {
            buffer_recycle(b);
            b = NULL;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK)
            return EV_DONE;
        if(errno == EINTR)
            return EV_READ_PENDING;
        /* TODO: error handling? */
        DPRINTF("Error while reading: %s (%d)\n", strerror(errno), errno);
        DELETE(this);
        return EV_DONE;
    }
    if(read_count == 0)
    {
        if(new_buffer)
        {
            buffer_recycle(b);
            b = NULL;
        }
        /* EOF */
        DPRINTF("EOF RECEIVED\n");
        this->flag_eof = 1;
        if(this->write_closed)
            DELETE(this);
        else
            event_modify(e, EV_REMOVE | EV_READ);
        return EV_DONE;
    }
    if(new_buffer)
    {
        b->used += read_count;
        CALL((StringIO)this->read_queue, write_buffer, b);
    }
    else
        CALL(this->__read_buffers, update_current_buffer, read_count);

    this->info.data_available(this);
    return EV_READ_PENDING;
}

static int write_callback(event e, struct event_info *info)
{
    DPRINTF("write_callback\n");
    Socket this = (Socket)info->context;
    CALL((StringIO)this->__write_buffers, seek, 0, SEEK_SET);
    buffer *b = CALL(this->__write_buffers, get_current_buffer);

    if(b == NULL)
    {
        event_modify(e, EV_REMOVE | EV_WRITE);
        if(this->write_closed)
        {
            DPRINTF("SENDING EOF\n");
            int result = shutdown(info->fd, SHUT_WR);
            if(result == -1 && errno != ENOTCONN)
                DPRINTF("Error sending EOF: %s (%d)\n", strerror(errno), errno);
            if(this->flag_eof)
                DELETE(this);
        }
        DPRINTF("write queue is empty\n");
        return EV_DONE;    
    }

    int write_size = b->used - b->pos;
    ASSERT(write_size > 0);
    if(write_size == 0)
    {
        DELETE(this);
        return EV_DONE;
    }
    int result = send(
            info->fd, 
            (void*)((uintptr_t)b->ptr + b->pos), 
            b->used - b->pos, 
            MSG_DONTWAIT | MSG_NOSIGNAL);

    if(result == -1)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
            return EV_DONE;
        if(errno == EINTR)
            return EV_WRITE_PENDING;
        /* TODO: error handling */
        DPRINTF("Received error on send: %s (%d)\n", strerror(errno), errno);
        DELETE(this);
        return EV_DONE;
    }
    /* remove written data from start of stringio */
    CALL((StringIO)this->__write_buffers, rtruncate,
        this->__write_buffers->total_size - result);
    return EV_WRITE_PENDING;
}

static int except_callback(event e, struct event_info *info)
{
    DPRINTF("Exception!\n");
    return EV_DONE;
}

#define CLASS_NAME(a,b) a## Socket ##b
Socket METHOD_IMPL(construct, struct socket_info *info)
{
    SUPER_CALL(Object, this, construct);
    this->__read_buffers = NEW(MemStringIO);
    this->__write_buffers = NEW(MemStringIO);
    this->read_queue = NEW(Pipe, this->__read_buffers);
    this->write_queue = NEW(Pipe, this->__write_buffers);

    this->info = *info;

    struct event_info event_info = {
        .fd = info->sock_fd,
        .events = EV_READ | EV_EXCEPT,
        .context = this,
        .read = read_callback,
        .write = write_callback,
        .except = except_callback,
    };

    int result = event_register(&event_info, &this->event);
    if(result != EVENTMGR_SUCCESS)
    {
        DPRINTF("Failed to register event: %s (%d)\n",
            eventmanager_strerror(result), result);
        free(this);
        return NULL;
    }
    list_add(&this->list, &sockets);
    return this;
}

size_t METHOD_IMPL(read, void *buf, size_t size)
{
    return CALL((StringIO)this->read_queue, read, buf, size);
}

buffer *METHOD_IMPL(read_buffer)
{
    return CALL((StringIO)this->read_queue, read_buffer);
}

char METHOD_IMPL(eof)
{
    if(CALL((StringIO)this->__read_buffers, seek, 0, SEEK_END) == 0)
        return this->flag_eof;
    return 0;
}

/* unless there is an error, we will always eat the entire buffer */
/* errors are probably fatal TODO figure this out later */
int METHOD_IMPL(write, void *buff, size_t size)
{
    if(this->write_closed)
    {
        errno = EPIPE;
        return -1;
    }
    size_t len = CALL((StringIO)this->write_queue, write, buff, size);
    if(len < 0)
        return -1;
    event_modify(this->event, EV_ADD | EV_WRITE);
    return 0;
}

int METHOD_IMPL(write_buffer, buffer *b)
{
    if(this->write_closed)
    {
        errno = EPIPE;
        return -1;
    }
    CALL((StringIO)this->write_queue, write_buffer, b);
    event_modify(this->event, EV_ADD | EV_WRITE);
    return 0;
}

/* shutdown WR */
void METHOD_IMPL(send_eof)
{
    this->write_closed = 1;
    event_modify(this->event, EV_ADD | EV_WRITE);
}

off_t METHOD_IMPL(seek, off_t offset, int whence)
{
    errno = ESPIPE;
    return -1;
}

int METHOD_IMPL(truncate, size_t len)
{
    errno = EINVAL;
    return -1;
}

int METHOD_IMPL(rtruncate, size_t len)
{
    errno = EINVAL;
    return -1;
}

/* frees this socket with no regard to waiting data */
void METHOD_IMPL(deconstruct)
{
    event_deregister(this->event);
    this->event = NULL;

    DELETE(this->__read_buffers);
    DELETE(this->__write_buffers);
    DELETE(this->read_queue);
    DELETE(this->write_queue);

    int result = shutdown(this->info.sock_fd, SHUT_RDWR);
    if(result == -1)
    {
        DPRINTF("Error shutting down socket: %s (%d)\n", strerror(errno), errno);
    }
    close(this->info.sock_fd);

    this->info.on_free(this);

    list_del(&this->list);
}

VIRTUAL(StringIO)
    VMETHOD_BASE(Object, construct);
    VMETHOD_BASE(Object, deconstruct);

    VMETHOD_BASE(StringIO, read);
    VMETHOD_BASE(StringIO, write);
    VMETHOD_BASE(StringIO, read_buffer);
    VMETHOD_BASE(StringIO, write_buffer);
    VMETHOD_BASE(StringIO, seek);
    VMETHOD_BASE(StringIO, truncate);
    VMETHOD_BASE(StringIO, rtruncate);

    VMETHOD(eof);
    VMETHOD(send_eof);

    VFIELD(__read_buffers) = NULL;
    VFIELD(__write_buffers) = NULL;
    VFIELD(read_queue) = NULL;
    VFIELD(write_queue) = NULL;

    VFIELD(event) = NULL;

    VFIELD(flag_eof) = 0;
    VFIELD(write_closed) = 0;
END_VIRTUAL
#undef CLASS_NAME

void socket_free_all()
{
    Socket i, j;
#ifdef __DEBUG__
    int count = 0;
#endif
    list_for_each_entry_safe(i, j, &sockets, list)
    {
#ifdef __DEBUG__
        count++;
#endif
        DELETE(i);
    }
    DPRINTF("Freed %d sockets\n", count);
}

