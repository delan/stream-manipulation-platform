
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "pluginloader.h"
#include "eventmanager.h"

void read_ready(event e, struct event_info *info)
{
    char buffer[1024];

    while(1)
    {
        int len = read(info->fd, buffer, sizeof(buffer));
        if(len == 0)
        {
            printf("end of stream!\n");
            event_deregister(e);
            return;
        }
        if(len == -1)
        {
            if(errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return;
            }
            event_deregister(e);
            return;
        }
        write(1, buffer, len);
    }
}

void write_ready(event e, struct event_info *info)
{
    fprintf(stderr, "write on stdin!?\n");
}

void exception(event e, struct event_info *info)
{
    fprintf(stderr, "exception!\n");
}

int main(int argc, char *argv[])
{
    eventmanager_init();

    struct event_info info = {
        .fd = 0,
        .context = NULL,
        .read = read_ready,
        .write = NULL,
        .except = exception,
    };
    int flags = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, flags | O_NONBLOCK);

    event e;
    int result = event_register(&info, &e);
    printf("event_register: %s\n", eventmanager_strerror(result));

    while(1)
    {
        eventmanager_tick(1000);
        write(1, ".", 1);
    }
}
