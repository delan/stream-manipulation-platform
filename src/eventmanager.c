
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>

#include "eventmanager.h"

#include "debug.h"

struct event
{
    struct list_head list;
    struct list_head pending_removal;
    struct list_head pending_read;
    struct list_head pending_write;
    struct event_info info;
};

static LIST_HEAD(events);

static LIST_HEAD(pending_read);
static LIST_HEAD(pending_write);
static LIST_HEAD(pending_removal);

static int epoll_fd = -1;

const char *error_strings[] =
{
    "Success",
    "Unused",
    "EPOLL Create call failed",
    "EPOLL Add call failed",
    "EPOLL Del call failed",
    "EPOLL Wait call failed",
    "Eventmanager is not initialized",
};

const char *eventmanager_strerror(int err)
{
    if(err < 0 || err >= EVENTMGR_MAX)
        return NULL;
    return error_strings[err];
}

int eventmanager_init(void)
{
    epoll_fd = epoll_create1(0);
    if(epoll_fd == -1)
    {
        return EVENTMGR_EPOLL_CREATE_FAILED;
    }
    return EVENTMGR_SUCCESS;
}

static inline char check_initialized(void)
{
    return epoll_fd != -1;
}

int event_register(struct event_info *event_info, struct event **event)
{
    if(!check_initialized())
        return EVENTMGR_NOT_INITIALIZED;

    struct event *e = (struct event*)malloc(sizeof(struct event));
    e->info = *event_info;
    INIT_LIST_HEAD(&e->pending_read);
    INIT_LIST_HEAD(&e->pending_write);
    INIT_LIST_HEAD(&e->pending_removal);

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLET;
    if(event_info->events & EV_READ)
        epoll_event.events |= EPOLLIN;
    if(event_info->events & EV_WRITE)
        epoll_event.events |= EPOLLOUT;
    if(event_info->events & EV_EXCEPT)
        epoll_event.events |= EPOLLERR;
    epoll_event.data.ptr = e;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_info->fd, &epoll_event) == -1)
    {
        return EVENTMGR_EPOLL_ADD_FAILED;
    }
    *event = e;
    list_add(&e->list, &events);

    return EVENTMGR_SUCCESS;
}

int event_modify(struct event *event, int events)
{
    if(!check_initialized())
        return EVENTMGR_NOT_INITIALIZED;

    struct event_info *event_info = &event->info;

    int action = events & EV_CTL_MASK;
    events &= EV_MASK;

    int orig_events = event_info->events;
    if(action == EV_ADD)
        event_info->events |= events;
    else if(action == EV_REMOVE)
        event_info->events &= ~events;
    else if(action == EV_SET)
        event_info->events = events;

    /* guarantee 'write' gets called initially (epoll documentation is unclear on this) */
    if(event_info->events & EV_WRITE && list_empty(&event->pending_write))
        list_add_tail(&event->pending_write, &pending_write);

    /* if there's no change, don't waste a syscall */
    if(orig_events == event_info->events)
        return EVENTMGR_SUCCESS;

    DPRINTF("Modifying EPOLL\n");

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLET;
    if(event_info->events & EV_READ)
        epoll_event.events |= EPOLLIN;
    if(event_info->events & EV_WRITE)
        epoll_event.events |= EPOLLOUT;
    if(event_info->events & EV_EXCEPT)
        epoll_event.events |= EPOLLERR;
    epoll_event.data.ptr = event;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event_info->fd, &epoll_event) == -1)
    {
        return EVENTMGR_EPOLL_MOD_FAILED;
    }
    return EVENTMGR_SUCCESS;
}

int event_deregister(struct event *event)
{
    if(!check_initialized())
        return EVENTMGR_NOT_INITIALIZED;

    if(list_empty(&event->pending_removal))
        list_add_tail(&event->pending_removal, &pending_removal);
    if(!epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->info.fd, NULL) == -1)
    {
        return EVENTMGR_EPOLL_DEL_FAILED;
    }
    return EVENTMGR_SUCCESS;
}

static int trigger_event(event e, int (*callback)(event e, struct event_info*))
{
    int result = callback(e, &e->info);
    if((result & EV_READ_PENDING))
    {
        if(list_empty(&e->pending_read))
            list_add(&e->pending_read, &pending_read);
    }
    else if(callback == e->info.read)
        list_del_init(&e->pending_read);
    if((result & EV_WRITE_PENDING))
    {
        if(list_empty(&e->pending_write))
            list_add(&e->pending_write, &pending_write);
    }
    else if(callback == e->info.write)
        list_del_init(&e->pending_write);

    return result;
}

int eventmanager_tick(int milliseconds)
{
    if(!check_initialized())
        return EVENTMGR_NOT_INITIALIZED;

    struct event *x, *y;
    int pending = 0;
    list_for_each_entry_safe(x, y, &pending_removal, pending_removal)
    {
        list_del(&x->pending_removal);
        list_del(&x->pending_read);
        list_del(&x->pending_write);
        list_del(&x->list);
        free(x);
    }
    list_for_each_entry_safe(x, y, &pending_read, pending_read)
    {
        if(x->info.events & EV_READ)
            pending |= trigger_event(x, x->info.read);
    }
    list_for_each_entry_safe(x, y, &pending_write, pending_write)
    {
        if(x->info.events & EV_WRITE)
            pending |= trigger_event(x, x->info.write);
    }

    /* if something somewhere is pending, no sleep */
    if(pending)
        milliseconds = 0;

    struct epoll_event events[32];
    int ready_count = epoll_wait(
            epoll_fd,
            events,
            32,
            milliseconds);

    if(ready_count == -1)
    {
        return EVENTMGR_EPOLL_WAIT_FAILED;
    }
    int i;
    DPRINTF("ready_count: %d\n", ready_count);
    for(i = 0;i < ready_count;i++)
    {
        struct event *e = (struct event*)events[i].data.ptr;

        int event_flags = events[i].events;
        if(event_flags & EPOLLIN && list_empty(&e->pending_read))
        {
            list_add(&e->pending_read, &pending_read);
        }
        if(event_flags & EPOLLOUT && list_empty(&e->pending_write))
        {
            list_add(&e->pending_write, &pending_write);
        }
        if(event_flags & EPOLLERR && e->info.except)
        {
            trigger_event(e, e->info.except);
        }
    }
    return EVENTMGR_SUCCESS;
}

void eventmanager_cleanup()
{
    if(!check_initialized())
        return;

    event i, j;
    list_for_each_entry_safe(i, j, &pending_removal, pending_removal)
    {
        list_del(&i->pending_removal);
        list_del(&i->pending_read);
        list_del(&i->pending_write);
        list_del(&i->list);
        free(i);
    }
}

