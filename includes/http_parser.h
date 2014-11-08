
#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "buffermanager.h"

typedef struct http *http;
typedef void (*recycle_func)(buffer *buffer);

http http_new(recycle_func recycler);
void http_feed_data(http http, buffer *buffer);
char ***http_get_headers(http http, int *header_count);

#endif // !HTTP_PARSER_H
