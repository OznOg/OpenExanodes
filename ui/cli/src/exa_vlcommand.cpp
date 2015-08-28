/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vlcommand.h"
#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/exa_dgcommand.h"

#include "ui/common/include/common_utils.h"
#include <iostream>

const std::string exa_vlcommand::ARG_VOLUME_CLUSTERNAME(Command::Boldify(
                                                            "CLUSTERNAME"));
const std::string exa_vlcommand::ARG_VOLUME_GROUPNAME(Command::Boldify(
                                                          "GROUPNAME"));
const std::string exa_vlcommand::ARG_VOLUME_VOLUMENAME(Command::Boldify(
                                                           "VOLUME"));

exa_vlcommand::exa_vlcommand(int argc, char *argv[])
    : Command(argc, argv)
    , _cluster_name("")
    , _group_name("")
    , _volume_name("")
{}


exa_vlcommand::~exa_vlcommand()
{}

void exa_vlcommand::init_options()
{
    Command::init_options();

    add_option('T', "timeout", "Max time for command to execute in seconds "
               "(0:infinite)", 0, false, true, TIMEOUT_ARG_NAME, "0");
    add_option('C', "no-color", "Disable color usage in terminal", 0, false,
               false);
    add_option('I', "in-progress", "Display in progress messages", 0, true,
               false);

    add_arg(ARG_VOLUME_CLUSTERNAME + ":" +
            ARG_VOLUME_GROUPNAME + ":" +
            ARG_VOLUME_VOLUMENAME, 10, false);
}


void exa_vlcommand::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    Command::parse_opt_args(opt_args);
}


void exa_vlcommand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);

    if (!column_split(":", non_opt_args.at(0), _cluster_name, _group_name,
                      _volume_name))
        throw CommandException(
            "Malformed volume name, " + ARG_VOLUME_CLUSTERNAME + ":" +
            ARG_VOLUME_GROUPNAME + ":" +
            ARG_VOLUME_VOLUMENAME + " expected.");

    exa_clcommand::check_name(_cluster_name);
    exa_dgcommand::check_name(_group_name);
    exa_vlcommand::check_name(_volume_name);
}


void exa_vlcommand::check_name(const std::string &vlname)
{
    if (vlname.empty())
        throw CommandException("Invalid empty volume name.");
    /* FIXME some check on length would probably be nice */
}


