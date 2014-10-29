
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>

#include "eventmanager.h"

struct event
{
    struct event_info info;
    int pending_removal;
};

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
    if(event_info->read)
        epoll_event.events |= EPOLLIN;
    if(event_info->write)
        epoll_event.events |= EPOLLOUT;
    if(event_info->except)
        epoll_event.events |= EPOLLERR;
    epoll_event.data.ptr = e;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_info->fd, &epoll_event) == -1)
    {
        return EVENTMGR_EPOLL_ADD_FAILED;
    }
    *event = e;
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

int eventmanager_tick(int milliseconds)
{
    if(!check_initialized)
        return EVENTMGR_NOT_INITIALIZED;

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
            free(e);
            continue;
        }
        int event_flags = events[i].events;
        if(event_flags == EPOLLIN && info->read)
            info->read(e, info);
        if(event_flags == EPOLLOUT && info->write)
            info->write(e, info);
        if(event_flags == EPOLLERR && info->except)
            info->except(e, info);
    }
}
