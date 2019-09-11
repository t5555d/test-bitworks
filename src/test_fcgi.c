#include <stdio.h>
#include <fcgiapp.h>
#include <pthread.h>
#include <uriparser/Uri.h>
#include "data.h"

typedef struct app_state_t {
    int             socket;
    config_t        config;
    pthread_mutex_t accept_mutex;
} app_state_t;

size_t uri_range_length(const UriTextRangeA *range)
{
    return range->afterLast - range->first;
}

int url_validate(const char *url)
{
    UriUriA uri;
    UriParserStateA state;
    state.uri = &uri;

    if (uriParseUriA(&state, url))
        return 1;

    size_t scheme = uri_range_length(&uri.scheme);
    size_t host = uri_range_length(&uri.hostText);

    uriFreeUriMembersA(&uri);
    return scheme && host ? 0 : 2;
}

UriQueryListA *query_parse(const char *query)
{
    UriQueryListA *params;
    int params_count;
    size_t query_len = strlen(query);
    int result = uriDissectQueryMallocA(&params, &params_count, query, query + query_len);
    return result == URI_SUCCESS ? params : NULL;
}

#define PARAM_USERNAME "username"
#define PARAM_FALLBACK "pageUrl"

void process(FCGX_Request *request, connect_t *connect)
{
    const char *query = FCGX_GetParam("QUERY_STRING", request->envp);

    UriQueryListA *params = query_parse(query);
    if (params == NULL) 
        goto incorrect_query;

    const char *username = NULL;
    const char *fallback = NULL;
    for (UriQueryListA *param = params; param; param = param->next) {
        if (0 == strcmp(param->key, PARAM_USERNAME))
            username = param->value;
        else if (0 == strcmp(param->key, PARAM_FALLBACK))
            fallback = param->value;
    }

    if (username == NULL || fallback == NULL)
        goto incorrect_query;

    const char *redirect = NULL;
    error_t error = select_memcache(connect, username, &redirect);
    if (error != ERROR_NONE) {
        error = select_mysql(connect, username, &redirect);
        if (error == ERROR_NONE) {
            if (url_validate(redirect))
                redirect = NULL;
        }
        else if (error != ERROR_NOT_FOUND)
            goto connect_error;
        
        if (redirect == NULL) {
            // URL missing or invalid - switch to fallback URL
            if (url_validate(fallback)) 
                goto incorrect_query;
            redirect = fallback;
        }

        update_memcache(connect, username, redirect);
    }

    FCGX_SetExitStatus(302, request->out);
    FCGX_FPrintF(request->out,
            "Content-Type: text/plain\r\n"
            "Location: %s\r\n"
            "\r\n"
            "Redirecting to %s\r\n", redirect, redirect);
    goto cleanup;

incorrect_query:
    FCGX_SetExitStatus(502, request->out);
    FCGX_FPrintF(request->out, 
            "Status: 502 Incorrect query\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Incorrect query.\r\n"
            "Correct format: /App/TestApp?username=<username>&pageUrl=<fallback URL>\r\n");
    goto cleanup;

connect_error:
    FCGX_SetExitStatus(502, request->out);
    FCGX_FPrintF(request->out,
            "Status: 502 Database connection error\r\n"
            "Connect-Type: text/plain\r\n"
            "\r\n"
            "Database connection error: %s\r\n", data_error(connect));
    goto cleanup;

cleanup:
    if (params)
        uriFreeQueryListA(params);
}

void *worker_execute(void *arg)
{
    app_state_t *state = (app_state_t *) arg;
    FCGX_Request request;
    FCGX_InitRequest(&request, state->socket, 0);

    connect_t *connect = data_connect(&state->config);
    
    while (1) {
        pthread_mutex_lock(&state->accept_mutex);
        int result = FCGX_Accept_r(&request);
        pthread_mutex_unlock(&state->accept_mutex);

        if (result < 0) {
            fprintf(stderr, "FCGX_Accept_r failed with %d\n", result);
            break;
        }

        process(&request, connect);

        FCGX_Finish_r(&request);
    }

    data_disconnect(connect);
    FCGX_Free(&request, 0);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <path/to/config.ini>\n", argv[0]);
        return 1;
    }
    
    app_state_t state;
    const char *config_path = argv[1];
    if (read_config(&state.config, config_path)) {
        fprintf(stderr, "Failed to read config file '%s'\n", config_path);
        return 1;
    }

    if (validate_config(&state.config)) {
        fprintf(stderr, "Invalid config\n");
        return 1;
    }

    FCGX_Init();
    data_init();

    state.socket = FCGX_OpenSocket(state.config.socket, 0);
    if (pthread_mutex_init(&state.accept_mutex, NULL)) {
        fprintf(stderr, "Failed pthread_mutex_init\n");
        return 2;
    }

    const int MAX_THREADS = 100;
    if (state.config.thread_count > MAX_THREADS) {
        fprintf(stderr, "Maximum supported threads: %d\n", MAX_THREADS);
        return 3;
    }

    pthread_t threads[MAX_THREADS];
    for (int i = 0; i < state.config.thread_count; i++)
        pthread_create(&threads[i], NULL, worker_execute, &state);
    
    for (int i = 0; i < state.config.thread_count; i++)
        pthread_join(threads[i], NULL);

    return 0;
}

