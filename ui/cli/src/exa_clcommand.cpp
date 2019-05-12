/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clcommand.h"

#include <iostream>

const std::string exa_clcommand::ARG_CLUSTERNAME(Command::Boldify("CLUSTERNAME"));

void exa_clcommand::init_options()
{
    Command::init_options();

    add_option('T', "timeout", "Max time for command to execute in seconds"
                               " (0:infinite)", 0, false, true,
               TIMEOUT_ARG_NAME, "0");
    add_option('C', "no-color", "Disable color usage in terminal", 0, false,
               false);
    add_option('I', "in-progress", "Display in progress messages", 0, true,
               false);

    /* assign arbitratily group number to 100, preserving some */
    /* margin for potential args inserted before this */
    /* one in derived classes */
    add_arg(ARG_CLUSTERNAME, 100, false);
}


void exa_clcommand::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    Command::parse_opt_args(opt_args);
}


void exa_clcommand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);
    _cluster_name = non_opt_args.at(0);
    check_name(_cluster_name);
}


void exa_clcommand::check_name(const std::string &clname)
{
    if (clname.empty())
        throw CommandException("Invalid empty cluster name.");
    /* FIXME some check on length would probably be nice */
}


