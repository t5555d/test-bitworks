#include "config.h"

#include <stdio.h>  // fopen, etc
#include <ctype.h>  // isspace
#include <string.h> // strncmp, strncpy
#include <stdlib.h> // atoi

typedef struct entry_t entry_t;
typedef void (*read_entry_t)(void *address, const char *line, const entry_t *entry);
typedef void (*print_entry_t)(char *text, const void *address, const entry_t *entry);
typedef int  (*valid_entry_t)(const void *address, const entry_t *entry);

typedef struct entry_t
{
    const char *    name;
    size_t          name_size;
    size_t          offset;
    size_t          size;
    read_entry_t    read;
    print_entry_t   print;
    valid_entry_t   valid;
} entry_t;

static void read_str(void *address, const char *line, const entry_t *entry)
{
    char *buffer = (char *) address;
    size_t last_non_space = -1;
    for (size_t i = 0; i < entry->size; i++) {
        if (line[i] == 0) break;
        buffer[i] = line[i];
	if (!isspace(line[i]))
	    last_non_space = i;
    }
    buffer[last_non_space + 1] = '\0';
}

static void read_int(void *address, const char *line, const entry_t *entry)
{
    *(int *)address = atoi(line);
}

static void print_str(char *text, const void *address, const entry_t *entry)
{
    strncpy(text, (const char *) address, entry->size);
}

static void print_int(char *text, const void *address, const entry_t *entry)
{
    sprintf(text, "%d", *(int *)address);
}

static int non_empty_str(const void *address, const entry_t *entry)
{
    const char *value = (const char *) address;
    return value[0] != 0;
}

static int positive_int(const void *address, const entry_t *entry)
{
    const int *value = (const int *) address;
    return *value > 0;
}

#define ROOT ((config_t *)NULL)
#define CFG_ENTRY(name, member, read, print, valid) \
    { name, sizeof(name) - 1, (size_t) (&ROOT->member), sizeof(ROOT->member), read, print, valid }
#define INT_ENTRY(name, member, valid) CFG_ENTRY(name, member, read_int, print_int, valid)
#define STR_ENTRY(name, member, valid) CFG_ENTRY(name, member, read_str, print_str, valid)

static const entry_t config_entries[] = {
    INT_ENTRY("app.thread.count", thread_count, positive_int),
    STR_ENTRY("app.socket", socket, non_empty_str),
    STR_ENTRY("app.mysql.host", mysql.host, non_empty_str),
    INT_ENTRY("app.mysql.port", mysql.port, positive_int),
    STR_ENTRY("app.mysql.user", mysql.user, non_empty_str),
    STR_ENTRY("app.mysql.password", mysql.pass, non_empty_str),
    STR_ENTRY("app.mysql.database", mysql.database, non_empty_str),
    STR_ENTRY("app.memcache.host", memcache.host, non_empty_str),
    INT_ENTRY("app.memcache.port", memcache.port, positive_int),
    INT_ENTRY("app.memcache.timeout", memcache.timeout, positive_int),
    { 0 }
};

error_t read_config(config_t *config, const char *filepath)
{
    memset(config, 0, sizeof(config_t));
    char buffer[260];
    FILE *file = fopen(filepath, "r");
    if (file == NULL) return ERROR_NOT_FOUND;
    size_t address = (size_t) config;
    while (fgets(buffer, sizeof(buffer), file)) {
	const char *line = buffer;
	for (const entry_t *entry = config_entries; entry->name; entry++) {
            if (strncmp(line, entry->name, entry->name_size))
                continue;
            line += entry->name_size;
            while (isspace(*line)) line++; // \s*
            if (*line++ != '=') continue;  // [=]
            while (isspace(*line)) line++; // \s*
            
            entry->read((void *)(address + entry->offset), line, entry);
            break;
	}
    }
    fclose(file);
    return ERROR_NONE;
}

error_t validate_config(const config_t *config)
{
    error_t result = ERROR_NONE;
    size_t address = (size_t) config;
    for (const entry_t *entry = config_entries; entry->name; entry++) {
        if (!entry->valid) continue;
        const void *field = (const void *)(address + entry->offset);
        int valid = entry->valid(field, entry);
        if (!valid) {
            char buffer[128];
            entry->print(buffer, field, entry);
            fprintf(stderr, "Invalid value: %s = %s\n", entry->name, buffer);
            result = ERROR_INVALID;
        }
    }
    return result;
}

void print_config(const config_t *config, FILE *file)
{
    size_t address = (size_t) config;
    for (const entry_t *entry = config_entries; entry->name; entry++) {
        char buffer[128];
        entry->print(buffer, (const void *)(address + entry->offset), entry);
        fprintf(file, "%s = %s\n", entry->name, buffer);
    }
}

