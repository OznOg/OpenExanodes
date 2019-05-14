/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_dgreset.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

exa_dgreset::exa_dgreset()
{
    add_see_also("exa_dgcheck");
}


void exa_dgreset::run()
{
    std::string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    AdmindCommand command("dgreset", exa.get_cluster_uuid());
    command.add_param("groupname", _group_name);

    printf("Resetting group '%s' for cluster '%s'\n",
           _group_name.c_str(),
           exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    std::string error_message;
    send_command(command, "Group reset:", error_code, error_message);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_dgreset::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);
}


void exa_dgreset::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Reset an Exanodes disk group.";
}


void exa_dgreset::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Reset the disk group " << ARG_DISKGROUP_GROUPNAME
        << " of the cluster " << ARG_DISKGROUP_CLUSTERNAME << std::endl;
}


void exa_dgreset::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Reset the disk group " << Boldify("mygroup") << " in the cluster "
        << Boldify("mycluster") << ":" << std::endl;

    out << "  " << "exa_dgreset mycluster:mygroup" << std::endl;
    out << std::endl;
}


