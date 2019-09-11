#include "strbuf.h"
#include <string.h>
#include <stdlib.h>

void strbuf_init(strbuf_t *str)
{
    memset(str, 0, sizeof(strbuf_t));
}

void strbuf_free(strbuf_t *str)
{
    if (str->buffer)
        free(str->buffer);
    strbuf_init(str);
}

int strbuf_resize(strbuf_t *str, size_t length)
{
    if (str->maxlen >= length)
        return 0;
    void *buffer = realloc(str->buffer, length + 1);
    if (!buffer)
        return 1;
    str->buffer = (char *) buffer;
    str->maxlen = length;
    return 0;
}

int strbuf_copy(strbuf_t *str, const char *value)
{
    size_t length = strlen(value);
    if (strbuf_resize(str, length))
        return 1;
    strcpy(str->buffer, value);
    str->length = length;
    return 0;
}

