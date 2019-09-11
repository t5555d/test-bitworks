#pragma once
#include <stdio.h>

typedef struct mysql_config_t
{
    char    host[32];
    int     port;
    char    user[32];
    char    pass[32];
    char    database[32];
} mysql_config_t;

typedef struct memcache_config_t
{
    char    host[32];
    int     port;
    int     timeout;
} memcache_config_t;

typedef struct config_t
{
    mysql_config_t    mysql;
    memcache_config_t memcache;
    char    socket[32];
    int     thread_count;
    int     queue_size;
} config_t;

typedef enum error_t
{
    ERROR_NONE = 0,
    ERROR_OUT_OF_MEM,
    ERROR_NOT_FOUND,
    ERROR_CONNECT,
    ERROR_INVALID,
} error_t;

error_t read_config(config_t *config, const char *filepath);
error_t validate_config(const config_t *config);
void print_config(const config_t *config, FILE *file);

