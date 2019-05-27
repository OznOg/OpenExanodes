/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fscommand.h"
#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/exa_dgcommand.h"

#include "ui/common/include/common_utils.h"
#include <iostream>

const std::string exa_fscommand::ARG_FILESYSTEM_CLUSTERNAME(Command::Boldify(
                                                                "CLUSTERNAME"));
const std::string exa_fscommand::ARG_FILESYSTEM_GROUPNAME(Command::Boldify(
                                                              "GROUPNAME"));
const std::string exa_fscommand::ARG_FILESYSTEM_FSNAME(Command::Boldify(
                                                           "FSNAME"));

exa_fscommand::exa_fscommand()
    : _cluster_name("")
    , _group_name("")
    , _fs_name("")
{
    add_option('T', "timeout", "Max time for command to execute in seconds "
               "(0:infinite)", 0, false, true, TIMEOUT_ARG_NAME, "0");
    add_option('C', "no-color", "Disable color usage in terminal", 0, false,
               false);
    add_option('I', "in-progress", "Display in progress messages", 0, true,
               false);

    /* assign arbitratily group number to 100, preserving some */
    /* margin for potential args inserted before this */
    /* one in derived classes */
    add_arg(ARG_FILESYSTEM_CLUSTERNAME + ":" +
            ARG_FILESYSTEM_GROUPNAME + ":" +
            ARG_FILESYSTEM_FSNAME, 100, false);
}


void exa_fscommand::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    Command::parse_opt_args(opt_args);
}


void exa_fscommand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);

    if (!column_split(":", non_opt_args.at(0), _cluster_name, _group_name,
                      _fs_name))
        throw CommandException(
            "Malformed filesystem name, " + ARG_FILESYSTEM_CLUSTERNAME + ":" +
            ARG_FILESYSTEM_GROUPNAME + ":" +
            ARG_FILESYSTEM_FSNAME + " expected.");

    exa_clcommand::check_name(_cluster_name);
    exa_dgcommand::check_name(_group_name);
    check_name(_fs_name);
}


void exa_fscommand::check_name(const std::string &fsname)
{
    if (fsname.empty())
        throw CommandException("Invalid empty file system name.");
    /* FIXME some check on length would probably be nice */
}


