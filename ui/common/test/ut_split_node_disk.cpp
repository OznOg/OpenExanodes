/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include <string>

#include "ui/common/include/split_node_disk.h"
#include "common/include/exa_config.h"

using std::string;

static bool test_split(std::string str, std::string expected_node,
                       std::string expected_disk)
{
    std::string node, disk;
    split_node_disk(str, node, disk);
    return node == expected_node && disk == expected_disk;
}

static std::string node_disk(const std::string node, const std::string disk)
{
    std::string str = node + EXA_CONF_SEPARATOR + disk;
    return str;
}

ut_test(split_empty_string_yields_empty_node_empty_disk)
{
    UT_ASSERT(test_split("", "", ""));
}

ut_test(split_one_node_only_yields_one_node_empty_disk)
{
    UT_ASSERT(test_split("sam69", "sam69", ""));
}

ut_test(split_one_node_empty_disk_yields_one_node_empty_disk)
{
    UT_ASSERT(test_split(node_disk("sam69", ""), "sam69", ""));
}

ut_test(split_node_list_only_yields_node_list_empty_disk)
{
    UT_ASSERT(test_split("sam/69:70:73-78:83/", "sam/69:70:73-78:83/", ""));
}

ut_test(split_node_list_empty_disk_yields_node_list_empty_disk)
{
    UT_ASSERT(test_split(node_disk("sam/69:70:73-78:83/", ""),
                         "sam/69:70:73-78:83/", ""));
}

ut_test(split_node_list_with_disk_yields_node_list_and_disk)
{
    UT_ASSERT(test_split(node_disk("sam/69:70:73-78:83/", "/dev/sdb"),
                         "sam/69:70:73-78:83/", "/dev/sdb"));
    UT_ASSERT(test_split(node_disk("sam/69:70:73-78:83/", "E:"),
                         "sam/69:70:73-78:83/", "E:"));
}
