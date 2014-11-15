
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "class.h"
#include "http_parser.h"
#include "buffermanager.h"

#define STATE_REQUEST       0
#define STATE_RESPONSE      1
#define STATE_HEADERS       2
#define STATE_BODY          3
#define STATE_EOF           4

#define MAX_HEADERS         128

struct search_state
{
    char needle[128];
    int pos;
};

#define CLASS_NAME(a,b) a## Http ##b
static Http METHOD_IMPL(construct)
{
    SUPER_CALL(Object, this, construct);
    this->buffer = (StringIO)NEW(MemStringIO);
    this->msg.headers = (char***)malloc(128 * sizeof(char**));
    return this;
}

static void METHOD_IMPL(deconstruct)
{
    DELETE(this->buffer);
    if(this->search)
        free(this->search);
}

#if 0 // no longer required - we're operating on a different buffer */
/* basically strtok's the string, but doesn't modify it */
static char **METHOD_IMPL(parse_header_string, char *c, unsigned int len)
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
#endif

static void METHOD_IMPL(read_headers, buffer *b)
{
    off_t pos = CALL(this->buffer, seek, 0, SEEK_CUR);
    CALL(this->buffer, write_buffer, buffer_dup(b));
    struct search_state *state = this->search;
    if(!state)
    {
        state = this->search = (struct search_state*)malloc(
            sizeof(struct search_state));
        strcpy(state->needle, "\r\n");
        state->pos = 0;
    }
    char *ptr = (char*)b->ptr;
    for(;b->pos < b->used;b->pos++, ptr++, pos++)
    {
        if(*ptr == state->needle[state->pos])
        {
            if(state->needle[++state->pos] == '\0')
            {
                size_t line_len = pos+1;
                if(line_len == strlen(state->needle))
                {
                    DPRINTF("end of headers found\n");
                    this->state = STATE_BODY;
                }

                char *hdr_str = (char*)malloc(line_len);
                CALL(this->buffer, seek, 0, SEEK_SET);
                CALL(this->buffer, read, hdr_str, line_len);
                hdr_str[line_len-2] = '\0';

                off_t end = CALL(this->buffer, seek, 0, SEEK_END);
                ASSERT(end >= line_len);
                CALL(this->buffer, rtruncate, 
                    end - line_len);

                if(this->state == STATE_REQUEST)
                {
                    char *request_type = strtok(hdr_str, " \t");
                    char *request_path = strtok(NULL, " \t");
                    char *http_version = strtok(NULL, " \t");
                    this->msg.request_type = strdup(request_type);
                    this->msg.request_path = strdup(request_path);
                    this->msg.http_version = strdup(http_version);
                    this->state = STATE_HEADERS;
                    DPRINTF("request: %s %s %s\n",
                            request_type,
                            request_path,
                            http_version);
                }
                else if(this->state == STATE_RESPONSE)
                {
                    char *http_version = strtok(hdr_str, " \t");
                    char *response_code = strtok(NULL, " \t");
                    char *response_msg = strtok(NULL, " \t");
                    //ASSERT(strcmp(http_version, this->msg.http_version) == 0);
                    this->msg.response_code = strtoll(response_code, NULL, 0);
                    this->msg.response_msg = strdup(response_msg);
                    this->state = STATE_HEADERS;
                    DPRINTF("response: %s %d %s\n",
                            http_version,
                            this->msg.response_code,
                            response_msg);
                }
                else
                {
                    char *name = hdr_str, *value = NULL, *i;
                    for(i = hdr_str;*i != '\0';i++)
                    {
                        if(*i == ':')
                        {
                            *i = '\0';
                            value = i+1;
                            break;
                        }
                    }
                    if(value)
                    {
                        for(;(*value == ' '  || *value == '\t') &&  
                              *value != '\0'; value++);
                        this->msg.headers[this->msg.header_count] =
                            (char**)malloc(2 * sizeof(char*));
                        this->msg.headers[this->msg.header_count][0] =
                            strdup(name);
                        this->msg.headers[this->msg.header_count][1] =
                            strdup(value);
                        this->msg.header_count++;
                    }
                }
                free(hdr_str);
                state->pos = 0;
                pos = -1;
            }
        }
        else
        {
            state->pos = 0;
        }
    }
    buffer_recycle(b);
}

static void METHOD_IMPL(feed_data, buffer *b)
{
    DPRINTF("state: %d\n", this->state);
    switch(this->state)
    {
    case STATE_REQUEST:
    case STATE_RESPONSE:
    case STATE_HEADERS:
        PRIV_CALL(this, read_headers, b);
        break;
    }
}

static char ***METHOD_IMPL(get_headers, int *header_count)
{
    if(this->state <= STATE_HEADERS)
        return NULL;
    *header_count = this->msg.header_count;
    return this->msg.headers;
}

VIRTUAL(Object)
    VMETHOD_BASE(Object, construct);
    VMETHOD_BASE(Object, deconstruct);
    VMETHOD(feed_data);
    VMETHOD(get_headers);

    VFIELD(buffer) = NULL;
    VFIELD(search) = NULL;
    VFIELD(state) = STATE_REQUEST;
    memset(&this->msg, '\0', sizeof(struct http_message));
END_VIRTUAL
#undef CLASS_NAME // Http

