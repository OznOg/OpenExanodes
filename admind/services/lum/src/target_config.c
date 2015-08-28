/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/lum/include/target_config.h"

#include "common/include/exa_error.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"

#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_network.h"
#include "os/include/os_string.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

typedef struct
{
    in_addr_t listen_addr;
} target_config_t;

static target_config_t target_config_params;
static bool target_config_set = false;

void target_config_init_defaults(void)
{
    target_config_params.listen_addr = htonl(INADDR_ANY);

    target_config_set = true;
}

int target_config_parse_line(const char *line)
{
    char *key;
    char *value;
    char *ptr;
    int err;
    char line_cpy[EXA_MAXSIZE_LINE];

    if (line == NULL)
        return -LUM_ERR_INVALID_TARGET_CONFIG_PARAM;

    os_strlcpy(line_cpy, line, sizeof(line_cpy));

    ptr = strchr(line_cpy, '\n');
    if (ptr != NULL)
        *ptr = '\0';
    ptr = strchr(line_cpy, '\r');
    if (ptr != NULL)
        *ptr = '\0';

    os_str_trim(line_cpy);
    if (strlen(line_cpy) == 0)
        return 0;

    ptr = strchr(line_cpy, '=');
    if (ptr == NULL)
        return -LUM_ERR_INVALID_TARGET_CONFIG_PARAM;

    key = line_cpy;
    value = ptr + 1;
    *ptr  = '\0';

    key = os_str_trim(key);
    value = os_str_trim(value);

    if (!strcmp(key, "listen_address"))
    {
        struct in_addr tmp;

        if (!os_net_ip_is_valid(value))
            return -LUM_ERR_INVALID_TARGET_CONFIG_PARAM;

        err = os_inet_aton(value, &tmp);
        if (err == 0) /* os_inet_aton returns 0 on failure... */
            return -LUM_ERR_INVALID_TARGET_CONFIG_PARAM;
        target_config_params.listen_addr = tmp.s_addr;
        return 0;
    }

    return -LUM_ERR_UNKNOWN_TARGET_CONFIG_PARAM;
}

int target_config_load(const char *file)
{
    FILE * fp;
    struct stat st;
    char line[EXA_MAXSIZE_LINE];
    int err;

    if (file == NULL)
        return -LUM_ERR_INVALID_TARGET_CONFIG_FILE;

    if (stat(file, &st) == -1)
    {
        if (errno == ENOENT)
            return 0;

        return -LUM_ERR_INVALID_TARGET_CONFIG_FILE;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG)
        return -LUM_ERR_INVALID_TARGET_CONFIG_FILE;

    fp = fopen(file, "rb");
    if (fp == NULL)
        return -errno;

    err = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        err = target_config_parse_line(line);
        if (err != 0)
            break;
    }

    fclose(fp);

    return err;
}

in_addr_t target_config_get_listen_address(void)
{
    EXA_ASSERT(target_config_set);

    return target_config_params.listen_addr;
}
