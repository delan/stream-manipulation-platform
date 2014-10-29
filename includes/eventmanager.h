
#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "list.h"

#define EVENTMGR_SUCCESS                0
#define EVENTMGR_EPOLL_CREATE_FAILED    2
#define EVENTMGR_EPOLL_ADD_FAILED       3
#define EVENTMGR_EPOLL_DEL_FAILED       4
#define EVENTMGR_EPOLL_WAIT_FAILED      5
#define EVENTMGR_NOT_INITIALIZED        6
#define EVENTMGR_MAX                    7

typedef struct event *event;

struct event_info
{
    int fd;
    void *context;
    void (*read)(event e, struct event_info*);
    void (*write)(event e,struct event_info*);
    void (*except)(event e, struct event_info*);
};

const char *eventmanager_strerror(int err);

int eventmanager_init(void);
int event_register(struct event_info *event_info, struct event **event);
int event_deregister(struct event *event);
int eventmanager_tick(int milliseconds);

#endif // !EVENTMANAGER_H
