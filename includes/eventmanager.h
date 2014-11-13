
#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include "list.h"

#define EVENTMGR_SUCCESS                0
#define EVENTMGR_EPOLL_MOD_FAILED       1
#define EVENTMGR_EPOLL_CREATE_FAILED    2
#define EVENTMGR_EPOLL_ADD_FAILED       3
#define EVENTMGR_EPOLL_DEL_FAILED       4
#define EVENTMGR_EPOLL_WAIT_FAILED      5
#define EVENTMGR_NOT_INITIALIZED        6
#define EVENTMGR_MAX                    7

#define EV_READ         (1<<0)
#define EV_WRITE        (1<<1)
#define EV_EXCEPT       (1<<2)
#define EV_MASK         ((1<<3)-1)

/* add these events to the existing events */
#define EV_ADD          (0<<16)
/* remove these events from the existing events */
#define EV_REMOVE       (1<<16)
/* set the events, with no consideration for any previous settings */
#define EV_SET          (2<<16)
#define EV_CTL_MASK     (3<<16)

#define EV_DONE             (0)
#define EV_READ_PENDING     (1<<0)
#define EV_WRITE_PENDING    (1<<1)

typedef struct event *event;

struct event_info
{
    int fd;
    int events;
    void *context;
    int (*read)(event e, struct event_info*);
    int (*write)(event e,struct event_info*);
    int (*except)(event e, struct event_info*);
    int (*alarm)(event e, struct event_info*);
};

const char *eventmanager_strerror(int err);

int eventmanager_init(void);
int event_register(struct event_info *event_info, struct event **event);
int event_modify(struct event *event, int events);
int event_alarm(struct event *event, int milliseconds);
int event_deregister(struct event *event);
int eventmanager_tick(int milliseconds);
void eventmanager_cleanup(void);

#endif // !EVENTMANAGER_H
