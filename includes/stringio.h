
#ifndef STRINGIO_H
#define STRINGIO_H

#include "class.h"

#define CLASS_NAME(a,b) a## StringIO ##b
CLASS(Object)
    size_t METHOD(read, void *buf, size_t len);
    size_t METHOD(write, void *buf, size_t len);

    buffer *METHOD(read_buffer);
    int METHOD(write_buffer, buffer* buffer);

    off_t METHOD(seek, off_t offset, int whence);
    int METHOD(truncate, size_t len);
    int METHOD(rtruncate, size_t len);
END_CLASS
#undef CLASS_NAME

#define CLASS_NAME(a,b) a## MemStringIO ##b
CLASS(StringIO)
    struct list_head buffers;
    
    /* write pos */
    buffer *current_buf;

    off_t current_pos;
    size_t total_size;

    size_t new_buffer_size;

    buffer *METHOD(get_current_buffer);
    void METHOD(update_current_buffer, size_t len);
END_CLASS
#undef CLASS_NAME

#define CLASS_NAME(a,b) a## Pipe ##b
CLASS(StringIO)
    StringIO    base;
END_CLASS
#undef CLASS_NAME

#endif // !STRINGIO_H
