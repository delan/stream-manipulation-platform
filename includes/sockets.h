
#ifndef SOCKETS_H
#define SOCKETS_H

#include "buffermanager.h"

typedef struct socket *smpsocket;

struct socket_info
{
    int sock_fd;
    void *context;
    void (*data_available)(smpsocket socket, struct socket_info *info);
    void (*on_free)(smpsocket socket, struct socket_info *info);
};

smpsocket socket_new(struct socket_info *info);
size_t socket_read(smpsocket s, void *buffer, size_t size);
buffer *socket_read_buffer(smpsocket s);
char socket_eof(smpsocket s);
int socket_write(smpsocket s, void *buffer, size_t size);
int socket_write_buffer(smpsocket s, buffer *b);
void socket_send_eof(smpsocket s);
void socket_free(smpsocket s);
void socket_free_all(void);

#endif // !SOCKETS_H
