
#ifndef SOCKETS_H
#define SOCKETS_H

#include "buffermanager.h"
#include "class.h"
#include "stringio.h"
#include "eventmanager.h"

DECLARE_CLASS(Socket);
struct socket_info
{
    int sock_fd;
    void *context;
    void (*data_available)(Socket socket);
    void (*on_free)(Socket socket);
};

#define CLASS_NAME(a,b) a## Socket ##b
CLASS(StringIO)
    struct list_head list;

    MemStringIO __read_buffers;
    MemStringIO __write_buffers;
    Pipe read_queue;
    Pipe write_queue;

    event event;

    char flag_eof:1,
         write_closed:1;

    char METHOD(eof);
    void METHOD(send_eof);

    struct socket_info info;
END_CLASS
#undef CLASS_NAME

#if 0
smpsocket socket_new(struct socket_info *info);
size_t socket_read(smpsocket s, void *buffer, size_t size);
buffer *socket_read_buffer(smpsocket s);
char socket_eof(smpsocket s);
int socket_write(smpsocket s, void *buffer, size_t size);
int socket_write_buffer(smpsocket s, buffer *b);
void socket_send_eof(smpsocket s);
void socket_free(smpsocket s);
#endif
void socket_free_all(void);

#endif // !SOCKETS_H
