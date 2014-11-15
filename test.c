
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "class.h"

#include "pluginloader.h"
#include "eventmanager.h"
#include "sockets.h"
#include "buffermanager.h"
#include "http_parser.h"

#include "debug.h"

static void data_available(Socket s)
{
    Http http = (Http)s->info.context;
    if(!http)
    {
        http = NEW(Http);
        http->state = 1;
        s->info.context = http;
    }

    buffer *b;
    while( (b = CALL((StringIO)s, read_buffer)) )
    {
        //CALL((StringIO)s, write_buffer, b);
        CALL(http, feed_data, b);
        char ***headers;
        int header_count;
        headers = CALL(http, get_headers, &header_count);
        if(headers)
        {
            DPRINTF("Http headers: %d\n", header_count);
            int i;
            for(i = 0;i < header_count;i++)
            {
                DPRINTF("%s: %s\n", headers[i][0], headers[i][1]);
            }
            CALL(s, send_eof);
        }
    }

    if(CALL(s, eof))
    {
        DPRINTF("calling 'send_eof'\n");
        CALL(s, send_eof);
    }
}

static void on_free(Socket s)
{
    DPRINTF("socket is being freed!\n");
    if(s->info.context)
        DELETE(s->info.context);
}

int handle_count = 0;
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

    handle_count++;
    NEW(Socket, &sock_info);
    return EV_READ_PENDING;
}

int quit = 0;
void sigint_handler(int sig)
{
    quit = 1;
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    eventmanager_init();

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1)
    {
        DPRINTF("socket() failed: %s (%d)\n", strerror(errno), errno);
        return 1;
    }
    struct sockaddr_in self = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr = { INADDR_ANY },
    };
    if(bind(sfd, (const struct sockaddr*)&self, sizeof(self)) == -1)
    {
        DPRINTF("bind() failed: %s (%d)\n", strerror(errno), errno);
        return 1;
    }
    if(listen(sfd, 32) == -1)
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
        .alarm = NULL,
    };

    event e;
    event_register(&info, &e);

    int garbage_collect(event e, struct event_info *info)
    {
        buffer_garbage_collect(10);
        event_alarm(e, 1000);
        return EV_DONE;
    }
    info = (struct event_info){
        .fd = -1,
        .alarm = garbage_collect,
    };
    event gc;
    event_register(&info, &gc);
    event_alarm(gc, 1000);

    signal(SIGINT, sigint_handler);

    while(!quit)
    {
        eventmanager_tick(1000);
    }

    event_deregister(e);
    socket_free_all();
    buffer_garbage_collect(0);
    eventmanager_cleanup();

    DPRINTF("total connections handled: %d\n", handle_count);

    return 0;
}
