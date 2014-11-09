
#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "buffermanager.h"
#include "class.h"
#include "stringio.h"

typedef struct http *http;
typedef void (*recycle_func)(buffer *buffer);

#define CLASS_NAME(a,b) a## Http ##b
CLASS(Object)
    StringIO buffer;
    struct search_state *search;
    int state;
    char **headers[128];
    int header_count;
END_CLASS
#undef CLASS_NAME // Http

#if 0
http http_new(recycle_func recycler);
void http_feed_data(http http, buffer *buffer);
char ***http_get_headers(http http, int *header_count);
#endif

#endif // !HTTP_PARSER_H
