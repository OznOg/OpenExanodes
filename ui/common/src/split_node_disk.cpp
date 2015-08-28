/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/common/include/split_node_disk.h"
#include "common/include/exa_config.h"

using std::string;

void split_node_disk(const string &str, string &node_regex, string &disk)
{
    bool in_node_regex = false;
    string::size_type i;

    for (i = 0; i < str.length(); i++)
    {
        if (str[i] == '/')
            in_node_regex = !in_node_regex;

        if (in_node_regex)
            continue;

        if (str[i] == EXA_CONF_SEPARATOR)
            break;
    }

    node_regex = str.substr(0, i);
    if (i < str.length())
        disk = str.substr(i + 1);
    else
        disk = "";
}
