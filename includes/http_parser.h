
#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stdint.h>

#include "buffermanager.h"
#include "class.h"
#include "stringio.h"

typedef struct http *http;
typedef void (*recycle_func)(buffer *buffer);

#define HTTP_REQUEST    0
#define HTTP_RESPONSE   1

struct http_message
{
    int direction;
    char *http_version;
    char *request_path;
    char *request_type;
    int response_code;
    char *response_msg;
    char ***headers;
    int header_count;
    uint64_t content_length;
};

#define CLASS_NAME(a,b) a## Http ##b
CLASS(Object)
    StringIO buffer;
    struct search_state *search;
    int state;

    struct http_message msg;

    void METHOD(feed_data, buffer *b);
    char ***METHOD(get_headers, int *header_count);
END_CLASS
#undef CLASS_NAME // Http

#if 0
http http_new(recycle_func recycler);
void http_feed_data(http http, buffer *buffer);
char ***http_get_headers(http http, int *header_count);
#endif

#endif // !HTTP_PARSER_H
