/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_dgcommand.h"
#include "ui/cli/src/exa_clcommand.h"

#include "ui/common/include/common_utils.h"
#include <iostream>

const std::string exa_dgcommand::ARG_DISKGROUP_CLUSTERNAME(Command::Boldify(
                                                               "CLUSTERNAME"));
const std::string exa_dgcommand::ARG_DISKGROUP_GROUPNAME(Command::Boldify(
                                                             "GROUPNAME"));

exa_dgcommand::exa_dgcommand()
{
    add_option('T', "timeout", "Max time for command to execute in seconds "
               "(0:infinite)", 0, false, true, TIMEOUT_ARG_NAME, "0");
    add_option('C', "no-color", "Disable color usage in terminal", 0, false,
               false);
    add_option('I', "in-progress", "Display in progress messages", 0, true,
               false);

    add_arg(ARG_DISKGROUP_CLUSTERNAME + ":" + ARG_DISKGROUP_GROUPNAME, 10,
            false);
}

void exa_dgcommand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);

    if (!column_split(":", non_opt_args.at(0), _cluster_name, _group_name))
        throw CommandException(
            "Malformed diskgroup name, " + ARG_DISKGROUP_CLUSTERNAME + ":" +
            ARG_DISKGROUP_GROUPNAME + " expected.");
    exa_clcommand::check_name(_cluster_name);
    check_name(_group_name);
}


void exa_dgcommand::check_name(const std::string &dgname)
{
    if (dgname.empty())
        throw CommandException("Invalid empty group name.");
    /* FIXME some check on length would probably be nice */
}


