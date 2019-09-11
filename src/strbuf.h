#pragma once
#include <stddef.h>

typedef struct strbuf_t
{
    char *          buffer;
    size_t          length;
    size_t          maxlen;
} strbuf_t;

void strbuf_init(strbuf_t *str);
void strbuf_free(strbuf_t *str);
int strbuf_copy(strbuf_t *str, const char *value);
int strbuf_resize(strbuf_t *str, size_t length);

