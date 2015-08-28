/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SPLIT_NODE_DISK_H
#define SPLIT_NODE_DISK_H

#include <string>

/**
 * Split a string into a node regex and a disk path.
 *
 * @param[in]  str         String to split
 * @param[out] node_regex  Node regex
 * @param[out] disk        Disk path if present, "" if none
 *
 * The function splits the input string at the first EXA_CONF_SEPARATOR
 * found outside of a regex.
 */
void split_node_disk(const std::string &str, std::string &node_regex,
                     std::string &disk);

#endif  /* SPLIT_NODE_DISK_H */
