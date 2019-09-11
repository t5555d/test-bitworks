#include "data.h"
#include "strbuf.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mysql/mysql.h>
#include <libmemcached/memcached.h>

typedef struct connect_t
{
    const config_t *config;
    MYSQL *         mysql;
    MYSQL_STMT *    select;
    memcached_st *  memcache;
    const char *    error;
    strbuf_t        value;
} connect_t;

#define ZERO_EXPECTED 0 ==
#define NON_ZERO_EXPECTED 0 !=

#define MYSQL_CHECK(call, cond) \
    if (!(cond(call))) { \
        connect->error = mysql_error(connect->mysql); \
        return ERROR_CONNECT; \
    }

#define MYSQL_STMT_CHECK(call, cond) \
    if (!(cond(call))) { \
        connect->error = mysql_stmt_error(connect->select); \
        return ERROR_CONNECT; \
    }

#define MEMCACHE_CHECK(call, cond) \
    if (!(cond(call))) { \
        connect->error = memcached_last_error_message(connect->memcache); \
        return ERROR_CONNECT; \
    }

#define RETURN_SUCCESS connect->error = NULL; return ERROR_NONE

#define MYSQL_SELECT_STMT "SELECT url FROM urls WHERE name = ?"

error_t data_init()
{
    if (mysql_library_init(0, NULL, NULL))
        return ERROR_CONNECT;
    return ERROR_NONE;
}

const char *data_error(connect_t *connect)
{
    return connect->error;
}

static error_t connect_mysql(connect_t *connect, const mysql_config_t *config)
{
    MYSQL_CHECK(connect->mysql = mysql_init(0), NON_ZERO_EXPECTED);
    MYSQL_CHECK(mysql_real_connect(connect->mysql, 
                config->host, 
                config->user, 
                config->pass, 
                config->database, 
                config->port, 0, 0), NON_ZERO_EXPECTED);
    MYSQL_CHECK(connect->select = mysql_stmt_init(connect->mysql), NON_ZERO_EXPECTED);
    MYSQL_STMT_CHECK(mysql_stmt_prepare(connect->select, MYSQL_SELECT_STMT, sizeof(MYSQL_SELECT_STMT)), ZERO_EXPECTED);
    return ERROR_NONE;
}

static void disconnect_mysql(connect_t *connect)
{
    if (connect->select)
        mysql_stmt_close(connect->select);
    if (connect->mysql)
        mysql_close(connect->mysql);
}

static error_t connect_memcache(connect_t *connect, const memcache_config_t *config)
{
    char options[1024];
    snprintf(options, sizeof(options), "--SERVER=%s:%d", 
            config->host, config->port);
    size_t options_len = strlen(options);
    MEMCACHE_CHECK(connect->memcache = memcached(options, options_len), NON_ZERO_EXPECTED);
    return ERROR_NONE;
}

static void disconnect_memcache(connect_t *connect)
{
    if (connect->memcache)
        memcached_free(connect->memcache);
}

connect_t *data_connect(const config_t *config)
{
    connect_t *connect = (connect_t *) malloc(sizeof(connect_t));
    if (!connect) return NULL;
    memset(connect, 0, sizeof(connect_t));
    connect->config = config;
    
    error_t error = connect_mysql(connect, &config->mysql);
    if (error != ERROR_NONE) 
        goto connect_failed;

    error = connect_memcache(connect, &config->memcache);
    if (error != ERROR_NONE) 
        goto connect_failed;

    return connect;

connect_failed:
    data_disconnect(connect);
    return NULL;
}

error_t data_disconnect(connect_t *connect)
{
    if (!connect)
        return ERROR_NOT_FOUND;
    disconnect_mysql(connect);
    disconnect_memcache(connect);
    strbuf_free(&connect->value);
    free(connect);
    return ERROR_NONE;
}

error_t select_mysql(connect_t *connect, const char *name, const char **pvalue)
{
    if(mysql_ping(connect->mysql)) {
        // reconnect
        disconnect_mysql(connect);
        error_t error = connect_mysql(connect, &connect->config->mysql);
        if (error) return error;
    }

    unsigned long param_length = strlen(name);
    unsigned long value_length = 0;

    MYSQL_BIND param = { 0 };
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = (char *) name;
    param.buffer_length = param_length;
    param.length = &param_length;

    MYSQL_BIND value = { 0 };
    value.buffer_type = MYSQL_TYPE_STRING;
    value.length = &value_length;

    MYSQL_STMT_CHECK(mysql_stmt_bind_param(connect->select, &param), ZERO_EXPECTED);
    MYSQL_STMT_CHECK(mysql_stmt_bind_result(connect->select, &value), ZERO_EXPECTED);
    MYSQL_STMT_CHECK(mysql_stmt_execute(connect->select), ZERO_EXPECTED);
  
    mysql_stmt_fetch(connect->select);
    if (value_length == 0)
        return ERROR_NOT_FOUND;

    strbuf_resize(&connect->value, value_length);
    value.buffer = connect->value.buffer;
    value.buffer_length = connect->value.length = value_length;
    MYSQL_STMT_CHECK(mysql_stmt_fetch_column(connect->select, &value, 0, 0), ZERO_EXPECTED);
    connect->value.buffer[value_length] = '\0';

    *pvalue = connect->value.buffer;
    RETURN_SUCCESS;
}

error_t select_memcache(connect_t *connect, const char *name, const char **pvalue)
{
    size_t name_len = strlen(name);
    size_t value_len;
    uint32_t flags;
    memcached_return_t result;
    char *value = memcached_get(connect->memcache, name, name_len, &value_len, &flags, &result);
    if (value == NULL) {
        connect->error = memcached_strerror(connect->memcache, result);
        return ERROR_NOT_FOUND;
    }

    if (connect->value.maxlen >= value_len) {
        strcpy(connect->value.buffer, value);
        free(value);
    }
    else {
        strbuf_free(&connect->value);
        connect->value.buffer = value;
        connect->value.length = value_len;
        connect->value.maxlen = value_len;
    }

    *pvalue = connect->value.buffer;
    RETURN_SUCCESS;
}

void update_memcache(connect_t *connect, const char *name, const char *value)
{
    memcached_return_t result;
    size_t name_len = strlen(name);
    size_t value_len = strlen(value);
    memcached_set(connect->memcache, name, name_len, value, value_len, 
            connect->config->memcache.timeout, 0);
}

