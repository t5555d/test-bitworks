#pragma once

#include "config.h"

typedef struct connect_t connect_t;

error_t data_init();

connect_t *data_connect(const config_t *config);
error_t data_disconnect(connect_t *connect);
const char *data_error(connect_t *connect);

error_t select_mysql(connect_t *connect, const char *name, const char **value);
error_t select_memcache(connect_t *connect, const char *name, const char **value);
void    update_memcache(connect_t *connect, const char *name, const char *value);

