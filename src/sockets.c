
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

struct socket
{
    struct list_head list;
    struct socket_info info;
    event event;
    char eof:1,
         write_closed:1;
    struct list_head read_buffers;
    struct list_head write_buffers;
};

static LIST_HEAD(sockets);

static int read_callback(event e, struct event_info *info)
{
    DPRINTF("read_callback\n");
    smpsocket s = (smpsocket)info->context;
    buffer *b = NULL;
    /* check to see if our current buffer has space left */
    if(!list_empty(&s->read_buffers))
    {
        b = list_tail(buffer, &s->read_buffers, list);
        if(b->used == b->size)
        {
            b = NULL;
        }
    }
    if(b == NULL)
    {
        b = buffer_get(SOCKET_BUFFER_SIZE);
        if(b == NULL)
        {
            DPRINTF("buffer_get gave us a NULL buffer\n");
            socket_free(s);
            return EV_DONE;
        }
        list_add(&b->list, &s->read_buffers);
    }

    int read_count = recv(info->fd, b->ptr + b->used, b->size - b->used, MSG_DONTWAIT);
    if(read_count == -1)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
            return EV_DONE;
        if(errno == EINTR)
            return EV_READ_PENDING;
        /* TODO: error handling? */
        DPRINTF("Error while reading: %s (%d)\n", strerror(errno), errno);
        socket_free(s);
        return EV_DONE;
    }
    if(read_count == 0)
    {
        /* EOF */
        s->eof = 1;
        if(s->write_closed)
            socket_free(s);
        else
            event_modify(e, EV_REMOVE | EV_READ);
    }
    b->used += read_count;

    s->info.data_available(s, &s->info);

    return EV_READ_PENDING;
}

static int write_callback(event e, struct event_info *info)
{
    DPRINTF("write_callback\n");
    smpsocket s = (smpsocket)info->context;
    buffer *b = NULL;

    if(list_empty(&s->write_buffers))
    {
        event_modify(e, EV_REMOVE | EV_WRITE);
        if(s->write_closed)
        {
            int result = shutdown(info->fd, SHUT_WR);
            if(result == -1 && errno != ENOTCONN)
                DPRINTF("Error sending EOF: %s (%d)\n", strerror(errno), errno);
            if(s->eof)
                socket_free(s);
        }
        return EV_DONE;    
    }

    b = list_first(buffer, &s->write_buffers, list);

    int write_size = b->used - b->pos;
    if(write_size == 0)
    {
        list_del(&b->list);
        buffer_recycle(b);
        return EV_WRITE_PENDING;
    }
    int result = send(info->fd, (void*)((uintptr_t)b->ptr + b->pos), b->used - b->pos, MSG_DONTWAIT);
    if(result == -1)
    {
        if(errno == EAGAIN || EWOULDBLOCK)
            return EV_DONE;
        if(errno == EINTR)
            return EV_WRITE_PENDING;
        /* TODO: error handling */
        DPRINTF("Received error on send: %s (%d)\n", strerror(errno), errno);
        socket_free(s);
        return EV_DONE;
    }
    b->pos += result;
    return EV_WRITE_PENDING;
}

static int except_callback(event e, struct event_info *info)
{
    DPRINTF("Exception!\n");
    return EV_DONE;
}

smpsocket socket_new(struct socket_info *info)
{
    smpsocket sock = (smpsocket)malloc(sizeof(struct socket));

    sock->info = *info;
    sock->eof = 0;
    sock->write_closed = 0;
    INIT_LIST_HEAD(&sock->read_buffers);
    INIT_LIST_HEAD(&sock->write_buffers);

    struct event_info event_info = {
        .fd = info->sock_fd,
        .events = EV_READ | EV_EXCEPT,
        .context = sock,
        .read = read_callback,
        .write = write_callback,
        .except = except_callback,
    };

    int result = event_register(&event_info, &sock->event);
    if(result != EVENTMGR_SUCCESS)
    {
        DPRINTF("Failed to register event: %s (%d)\n", eventmanager_strerror(result), result);
        free(sock);
        return NULL;
    }
    list_add(&sock->list, &sockets);
    return sock;
}

size_t socket_read(smpsocket s, void *buff, size_t size)
{
    buffer *b;
    while(1)
    {
        if(list_empty(&s->read_buffers))
        {
            if(s->eof)
                return 0;
            errno = EAGAIN;
            return -1;
        }
        b = list_first(buffer, &s->read_buffers, list);
        if(b->pos < b->used)
            break;
        list_del(&b->list);
        buffer_recycle(b);
    }
    size_t avail = b->used - b->pos;
    if(avail > size)
        avail = size;
    memcpy(buff, (void*)((uintptr_t)b->ptr + b->pos), avail);
    b->pos += avail;
    return avail;
}

buffer *socket_read_buffer(smpsocket s)
{
    if(list_empty(&s->read_buffers))
        return NULL;
    buffer *b = list_first(buffer, &s->read_buffers, list);
    list_del(&b->list);
    return b;
}

char socket_eof(smpsocket s)
{
    if(list_empty(&s->read_buffers))
        return s->eof;
    return 0;
}

/* unless there is an error, we will always eat the entire buffer */
/* errors are probably fatal TODO figure this out later */
int socket_write(smpsocket s, void *buff, size_t size)
{
    if(s->write_closed)
        return -1;
    buffer *b = NULL;
    size_t avail;
    if(!list_empty(&s->write_buffers))
        b = list_tail(buffer, &s->write_buffers, list);
    while(1)
    {
        if(b == NULL)
        {
            b = buffer_get(size > SOCKET_BUFFER_SIZE?size:SOCKET_BUFFER_SIZE);
            if(b == NULL)
            {
                DPRINTF("Received NULL buffer!\n");
                return -1;
            }
            list_add_tail(&b->list, &s->write_buffers);
        }
        avail = b->size - b->used;
        if(avail > size)
            avail = size;
        memcpy((void*)((uintptr_t)b->ptr + b->used), buff, avail);

        size -= avail;

        if(size == 0)
            break;

        buff = (void*)((uintptr_t)buff+avail);
        b = NULL;
    }
    event_modify(s->event, EV_ADD | EV_WRITE);
    return 0;
}

int socket_write_buffer(smpsocket s, buffer *b)
{
    if(s->write_closed)
        return -1;
    list_add_tail(&b->list, &s->write_buffers);
    event_modify(s->event, EV_ADD | EV_WRITE);
    return 0;
}

/* shutdown WR */
void socket_send_eof(smpsocket s)
{
    s->write_closed = 1;
    event_modify(s->event, EV_ADD | EV_WRITE);
}

/* frees this socket with no regard to waiting data */
void socket_free(smpsocket s)
{
    event_deregister(s->event);
    s->event = NULL;

    buffer *i, *j;
    list_for_each_entry_safe(i, j, &s->read_buffers, list)
    {
        list_del(&i->list);
        buffer_recycle(i);
    }
    list_for_each_entry_safe(i, j, &s->write_buffers, list)
    {
        list_del(&i->list);
        buffer_recycle(i);
    }

    int result = shutdown(s->info.sock_fd, SHUT_RDWR);
    if(result == -1)
    {
        DPRINTF("Error shutting down socket: %s (%d)\n", strerror(errno), errno);
    }
    close(s->info.sock_fd);

    s->info.on_free(s, &s->info);

    list_del(&s->list);
    free(s);
}

void socket_free_all()
{
    smpsocket i, j;
#ifdef __DEBUG__
    int count = 0;
#endif
    list_for_each_entry_safe(i, j, &sockets, list)
    {
#ifdef __DEBUG__
        count++;
#endif
        socket_free(i);
    }
    DPRINTF("Freed %d sockets\n", count);
}

