/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TARGET_CONFIG_H__
#define __TARGET_CONFIG_H__

#include "os/include/os_network.h"

#define TARGET_CONF "target.conf"

/**
 * Set the default target configuration.
 */
void target_config_init_defaults(void);

/**
 * Parse a configuration line of the format 'key=value'
 *
 * @param line      The line to parse
 *
 * @return 0 if successful, a negative error code otherwise.
 */
int target_config_parse_line(const char *line);

/**
 * Set default parameters and load the target configuration
 *
 * @param[in] file      File to read
 *
 * @return 0 if successful, a negative error code otherwise.
 * @Note: Values of settings are undefined if a failure happens.
 *        Call target_config_init_defaults() or assert.
 *        The configuration file format is expected to be a list
 *        of key=value pairs, separated by newlines.
 */
int target_config_load(const char *file);

/**
 * Get the target listen address.
 *
 * @return an IP address in network byte order.
 */
in_addr_t target_config_get_listen_address(void);

#endif /* __TARGET_CONFIG_H__ */
