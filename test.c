
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>

#include "pluginloader.h"
#include "eventmanager.h"
#include "sockets.h"
#include "buffermanager.h"

#include "debug.h"

static void data_available(smpsocket s, struct socket_info *info)
{
    DPRINTF("data available!\n");
    buffer *b;
    while((b = socket_read_buffer(s)))
    {
        DPRINTF("writing buffer\n");
        socket_write_buffer(s, b);
    }

    if(socket_eof(s))
        socket_send_eof(s);
}

static void on_free(smpsocket s, struct socket_info *info)
{
    DPRINTF("socket is being freed!\n");
}

static int accept_callback(event e, struct event_info *info)
{
    DPRINTF("accepting client\n");
    struct sockaddr_in client;
    socklen_t sockaddr_len = sizeof(client);
    int fd = accept(info->fd, (struct sockaddr*)&client, &sockaddr_len);
    if(fd == -1)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return EV_DONE;
        }
        if(errno == ECONNABORTED)
        {
            return EV_READ_PENDING;
        }
        DPRINTF("error accepting connection: %s (%d)\n", strerror(errno), errno);
        event_deregister(e);
        return EV_DONE;
    }

    struct socket_info sock_info = {
        .sock_fd = fd,
        .context = NULL,
        .data_available = data_available,
        .on_free = on_free,
    };

    socket_new(&sock_info);
    return EV_READ_PENDING;
}

int main(int argc, char *argv[])
{
    eventmanager_init();

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1)
    {
        DPRINTF("socket() failed: %s (%d)\n", strerror(errno), errno);
        return 1;
    }
    struct sockaddr_in self = {
        .sin_family = AF_INET,
        .sin_port = htons(9000),
        .sin_addr = { INADDR_ANY },
    };
    if(bind(sfd, (const struct sockaddr*)&self, sizeof(self)) == -1)
    {
        DPRINTF("bind() failed: %s (%d)\n", strerror(errno), errno);
        return 1;
    }
    if(listen(sfd, 8) == -1)
    {
        DPRINTF("listen() failed: %s (%d)\n", strerror(errno), errno);
    }

    fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);

    struct event_info info = {
        .fd = sfd,
        .events = EV_READ | EV_EXCEPT,
        .context = NULL,
        .read = accept_callback,
        .write = NULL,
        .except = NULL,
    };

    event e;
    DPRINTF("event_register: %s\n", eventmanager_strerror(event_register(&info, &e)));

    while(1)
    {
        eventmanager_tick(1000);
        DPRINTF("'\n");
    }
}
