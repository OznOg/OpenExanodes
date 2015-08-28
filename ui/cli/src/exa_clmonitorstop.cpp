/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clmonitorstop.h"

#include <errno.h>
#include <boost/lexical_cast.hpp>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"

using std::string;

exa_clmonitorstop::exa_clmonitorstop(int argc, char *argv[])
    : exa_clcommand(argc, argv)
{}


void exa_clmonitorstop::init_options()
{
    exa_clcommand::init_options();
}


void exa_clmonitorstop::parse_opt_args(const std::map<char,
                                                      std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);
}


void exa_clmonitorstop::init_see_alsos()
{
    add_see_also("exa_clmonitorstart");
    add_see_also("exa_clinfo");
    add_see_also("exa_cltune");
}


void exa_clmonitorstop::run()
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

    exa_cli_info("Stopping monitor for cluster '%s'\n",
                 exa.get_cluster().c_str());

    AdmindCommand command("clmonitorstop", exa.get_cluster_uuid());

    /* Send the command and receive the response */
    string error_message;
    send_command(command, "Stop monitoring:", error_code, error_message);

    if (error_code)
        exa_cli_error("\n%sERROR%s: %s\n",
                      COLOR_ERROR, COLOR_NORM, error_message.c_str());

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_clmonitorstop::dump_short_description(std::ostream &out,
                                               bool show_hidden) const
{
    out << "Stop the monitoring system for Exanodes.";
}


void exa_clmonitorstop::dump_full_description(std::ostream &out,
                                              bool show_hidden) const
{
    out << "Stop the monitoring system for Exanodes."
        << std::endl;
}


void exa_clmonitorstop::dump_examples(std::ostream &out,
                                      bool show_hidden) const
{
    out << "Stop the monitoring system for Exanodes on cluster mycluster:" <<
    std::endl;
    out << "  " << "exa_clmonitorstop cluster" << std::endl;
    out << std::endl;
}


