
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "http_parser.h"
#include "buffermanager.h"

#define STATE_HEADERS       0
#define STATE_BODY          1
#define STATE_EOF           2

#define MAX_HEADERS         128

struct http
{
    recycle_func recycler;
    struct list_head buffers;
    int state;
    char **headers[128];
    int header_count;
};

http http_new(recycle_func recycler)
{
    http h = (http)malloc(sizeof(struct http));

    h->recycler = recycler;
    INIT_LIST_HEAD(&h->buffers);
    h->state = STATE_HEADERS;
    h->header_count = 0;

    return h;
}

/* basically strtok's the string, but doesn't modify it */
static char **parse_header_string(char *c, unsigned int len)
{
    DPRINTF("Parsing HTTP Header line: %*s\n", (int)len, c);

    char *end = c + len;
    char *colon = c;
    char *value;
    unsigned int i;
    for(i = 0;i < len;i++, colon++)
    {
        if(*colon == ':')
            break;
    }
    /* make sure we didn't get to end of string */
    if(colon == end)
        return NULL;

    int name_len = colon - c;
    size_t required_size = name_len;

    value = colon + 1;
    for(i = required_size;i < len;i++, value++)
    {
        if(*value != ' ' && *value != '\t')
            break;
    }
    if(value == end)
        return NULL;

    int value_len = end - value;
    required_size += end - value;
    required_size += 2 + 2 * sizeof(char*); // null chars, 2 ptrs

    char *hdr = (char*)malloc(required_size);

    memcpy(hdr + 2, c, name_len);
    *(hdr + name_len) = '\0';

    memcpy(hdr+name_len+3, value, value_len);
    *(hdr + name_len + 1 + value_len) = '\0';

    char **hdrs = (char**)hdr;
    hdrs[0] = hdr + 2;
    hdrs[1] = hdrs[0] + name_len + 1;

    return hdrs;
}

static void read_headers(http http, buffer *b)
{
    char *pos = (char*)buffer_get_at_pos(b);
    char *end = (char*)buffer_get_at_end(b);

    char *line_start = pos;
    for(;pos != end;pos++,b->pos++)
    {
        if(*pos == '\r' || *pos == '\n')
        {
            if(*(pos+1) == '\n')
                continue;

            uintptr_t len = pos - line_start;
            if(pos > line_start && *(pos-1) == '\r')
                len--;

            DPRINTF("HTTP Delimiter Found\n");
            DPRINTF("HTTP Header Line: %*s\n", (int)len, line_start);

            /* a len of 0 indicates 2 delimiters in a row */
            if(len == 0)
            {
                http->state = STATE_BODY;
                break;
            }
           
            char free_line = 0;
            if(!list_empty(&http->buffers))
            {
                free_line = 1;
                buffer *i, *j;

                uintptr_t len_new = len;
                list_for_each_entry(i, &http->buffers, list)
                {
                    len_new += (uintptr_t)i->used - (uintptr_t)i->pos;
                }
                char *line_new = (char*)malloc(len_new);

                off_t p = 0;
                list_for_each_entry_safe(i, j, &http->buffers, list)
                {
                    memcpy(line_new + p, i->ptr, i->used);
                    p += i->used;

                    list_del(&i->list);
                    http->recycler(i);
                }
                memcpy(line_new + p, line_start, len);
                line_start = line_new;
                len = len_new;
            }
            char **hdrs = parse_header_string(line_start, len);
            http->headers[http->header_count++] = hdrs;

            if(free_line)
                free(line_start);

            DPRINTF("new headers: %s: %s\n", hdrs[0], hdrs[1]);

            line_start = pos + 1;
        }
    }

    if(http->state == STATE_HEADERS && b->pos != b->used)
    {
        list_add_tail(&b->list, &http->buffers);
    }
}

void http_feed_data(http http, buffer *b)
{
    switch(http->state)
    {
    case STATE_HEADERS:
        read_headers(http, b);
        break;
    }
}


