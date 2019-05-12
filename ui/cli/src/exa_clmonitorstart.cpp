/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clmonitorstart.h"

#include <errno.h>
#include <boost/lexical_cast.hpp>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "monitoring/common/include/md_constants.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"

#include <iostream>

using std::string;

const std::string exa_clmonitorstart::OPT_ARG_SNMPDHOST_HOST(Command::Boldify(
                                                                 "HOST"));
const std::string exa_clmonitorstart::OPT_ARG_SNMPDPORT_PORT(Command::Boldify(
                                                                 "PORT"));

exa_clmonitorstart::exa_clmonitorstart(int argc, char *argv[])
    : _snmpd_host("")
    , _snmpd_port(MD_DEFAULT_MASTER_AGENTX_PORT)
{}


void exa_clmonitorstart::init_options()
{
    exa_clcommand::init_options();

    add_option('s', "snmpdhost", "Specify the host on which snmpd is running.",
               1, false, true, OPT_ARG_SNMPDHOST_HOST);
    add_option('p', "snmpdport", "Specify the port on which snmpd is listening.",
               0, false, true, OPT_ARG_SNMPDPORT_PORT,
               MD_DEFAULT_MASTER_AGENTX_PORT_STR);
}


void exa_clmonitorstart::init_see_alsos()
{
    add_see_also("exa_clmonitorstop");
    add_see_also("exa_clinfo");
}


void exa_clmonitorstart::run()
{
    string error_msg;
    exa_error_code error_code = EXA_SUCCESS;

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME,
                                 EXA_MAXSIZE_CLUSTERNAME,
                                 _cluster_name, false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", _cluster_name.c_str());

    exa_cli_info("Starting monitor for cluster '%s'\n",
                 exa.get_cluster().c_str());

    AdmindCommand command("clmonitorstart", exa.get_cluster_uuid());
    command.add_param("snmpd_host", _snmpd_host);
    command.add_param("snmpd_port", _snmpd_port);

    /* Send the command and receive the response */
    string error_message;
    send_command(command, "Start monitoring:", error_code, error_message);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_message, error_code);
}


void exa_clmonitorstart::parse_opt_args(const std::map<char,
                                                       std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('s') != opt_args.end())
        _snmpd_host = opt_args.find('s')->second;

    if (opt_args.find('p') != opt_args.end())
        if (to_int(opt_args.find('p')->second.c_str(),
                   &_snmpd_port) != EXA_SUCCESS)
            throw CommandException("Invalid port");
}


void exa_clmonitorstart::dump_short_description(std::ostream &out,
                                                bool show_hidden) const
{
    out << "Start the monitoring system for Exanodes.";
}


void exa_clmonitorstart::dump_full_description(std::ostream &out,
                                               bool show_hidden) const
{
    out << "Start the monitoring system for Exanodes."
        << std::endl;
}


void exa_clmonitorstart::dump_examples(std::ostream &out,
                                       bool show_hidden) const
{
    out <<
    "Start the monitoring system for Exanodes on cluster mycluster, assuming that snmpd is running on node1:"
   << std::endl;
    out << "  " << "exa_clmonitorstart --snmpdhost node1 cluster" << std::endl;
    out << std::endl;
}


