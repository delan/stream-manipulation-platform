
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "pluginloader.h"
#include "eventmanager.h"

char buffer[1024];
int buffer_len = 0;

event stdin_event;
event stdout_event;

int read_ready(event e, struct event_info *info)
{
    if(sizeof(buffer) == buffer_len)
        return EV_READ_PENDING;
    int len = read(info->fd, buffer+buffer_len, sizeof(buffer)-buffer_len);
    if(len == 0)
    {
        fprintf(stderr, "end of stream!\n");
        event_deregister(e);
        return EV_DONE;
    }
    if(len == -1)
    {
        if(errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return EV_DONE;
        }
        event_deregister(e);
        return EV_DONE;
    }
    buffer_len += len;
    
    event_modify(stdout_event, EV_ADD | EV_WRITE);

    return EV_READ_PENDING | EV_WRITE_PENDING;
}

int write_ready(event e, struct event_info *info)
{
    fprintf(stderr, "write_ready\n");
    if(buffer_len == 0)
    {
        //event_modify(e, EV_REMOVE | EV_WRITE);
        return EV_DONE;
    }
    int result = 1; //write(1, buffer, 1);
    if(result == -1)
    {
        return EV_DONE;
    }
    buffer_len -= result;
    memmove(buffer, buffer+result, buffer_len);
    return EV_DONE;
}

int exception(event e, struct event_info *info)
{
    fprintf(stderr, "exception!\n");
    return EV_DONE;
}

int main(int argc, char *argv[])
{
    eventmanager_init();

    struct event_info info_stdin = {
        .fd = 0,
        .events = EV_READ | EV_EXCEPT,
        .context = NULL,
        .read = read_ready,
        .write = NULL,
        .except = exception,
    };
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);
    struct event_info info_stdout = {
        .fd = 1,
        .events = EV_EXCEPT,
        .context = NULL,
        .read = NULL,
        .write = write_ready,
        .except = exception,
    };
    flags = fcntl(1, F_GETFL, 0);
    fcntl(1, F_SETFL, flags | O_NONBLOCK);

    printf("registering event: %s\n", eventmanager_strerror(event_register(&info_stdin, &stdin_event)));
    printf("registering event: %s\n", eventmanager_strerror(event_register(&info_stdout, &stdout_event)));

    while(1)
    {
        eventmanager_tick(1000);
        fprintf(stderr, "'");
    }
}
