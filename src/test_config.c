#include "config.h"

int main(int argc, const char *argv[])
{
    config_t config;
    read_config(&config, argv[1]);
    print_config(&config, stdout);
    validate_config(&config);
    return 0;
}
