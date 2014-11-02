
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>

#include "eventmanager.h"

struct event
{
    struct list_head list;
    struct event_info info;
    int pending_removal;
    int pending_read;
    int pending_write;
};

static LIST_HEAD(events);

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
    if(!check_initialized)
        return EVENTMGR_NOT_INITIALIZED;

    struct event *e = (struct event*)malloc(sizeof(struct event));
    memcpy(&e->info, event_info, sizeof(*event_info));

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
    if(!check_initialized)
        return EVENTMGR_NOT_INITIALIZED;

    struct event_info *event_info = &event->info;

    int action = events & EV_CTL_MASK;
    events &= EV_MASK;

    if(action == EV_ADD)
        event_info->events |= events;
    else if(action == EV_REMOVE)
        event_info->events &= ~events;
    else if(action == EV_SET)
        event_info->events = events;

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
    if(!check_initialized)
        return EVENTMGR_NOT_INITIALIZED;

    event->pending_removal = 1;
    if(!epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->info.fd, NULL) == -1)
    {
        return EVENTMGR_EPOLL_DEL_FAILED;
    }
    return EVENTMGR_SUCCESS;
}

static int trigger_event(event e, int (*callback)(event e, struct event_info*))
{
    int result = callback(e, &e->info);
    if(result & EV_READ_PENDING)
        e->pending_read = 1;
    else if(callback == e->info.read)
        e->pending_read = 0;
    if(result & EV_WRITE_PENDING)
        e->pending_write = 1;
    else if(callback == e->info.write)
        e->pending_write = 0;

    return result;
}

int eventmanager_tick(int milliseconds)
{
    if(!check_initialized)
        return EVENTMGR_NOT_INITIALIZED;

    struct event *x, *y;
    int pending = 0;
    list_for_each_entry_safe(x, y, &events, list)
    {
        if(x->pending_removal)
        {
            list_del(&x->list);
            free(x);
            continue;
        }
        if(x->pending_read && x->info.events & EV_READ)
        {
            pending |= trigger_event(x, x->info.read);
        }
        if(x->pending_write && x->info.events & EV_WRITE)
        {
            pending |= trigger_event(x, x->info.write);
        }
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
    for(i = 0;i < ready_count;i++)
    {
        struct event *e = (struct event*)events[i].data.ptr;
        struct event_info *info = &e->info;
        if(e->pending_removal == 1)
        {
            list_del(&e->list);
            free(e);
            continue;
        }

        /* here we squash events that are already marked as pending,
           as they will have already been called in the pre-epoll loop. */

        int event_flags = events[i].events;
        if(event_flags == EPOLLIN && info->read && !e->pending_read)
            trigger_event(e, info->read);
        if(event_flags == EPOLLOUT && info->write && !e->pending_write)
            trigger_event(e, info->write);
        if(event_flags == EPOLLERR && info->except)
            trigger_event(e, info->except);
    }
    return EVENTMGR_SUCCESS;
}
