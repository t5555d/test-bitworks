#include <stdio.h>
#include "config.h"
#include "data.h"

int main(int argc, const char *argv[])
{
    if (data_init() != ERROR_NONE)
        return 1;

    config_t config;
    if (read_config(&config, "config.ini"))
        return 2;
    
    connect_t *connect = data_connect(&config);
    if (!connect)
        return 3;

    for (int i = 1; i < argc; i++) {
        const char *value = NULL;
        error_t error = select_memcache(connect, argv[i], &value);
        if (error != ERROR_NONE) {
            error = select_mysql(connect, argv[i], &value);
            if (error == ERROR_NOT_FOUND)
                value = "- no data -";
            if (value)
                update_memcache(connect, argv[i], value);
        }

        if (error == ERROR_NONE)
            printf("%s: %s\n", argv[i], value);
        else
            fprintf(stderr, "ERROR: %s - %s\n", argv[i], data_error(connect));
    }

    for (int i = 1; i < argc; i++) {
        const char *value = NULL;
        error_t error = select_memcache(connect, argv[i], &value);
        if (error != ERROR_NONE || value == NULL)
            fprintf(stderr, "ERROR: %s - memcache error %d\n", argv[i], error);
        else
            printf("CACHE: %s: %s\n", argv[i], value);
    }

    data_disconnect(connect);

    return 0;
}

