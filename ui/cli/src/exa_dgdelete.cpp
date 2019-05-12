/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_dgdelete.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_stdio.h"

using std::string;

exa_dgdelete::exa_dgdelete()
    : _forcemode(false)
    , _recursive(false)
{}

void exa_dgdelete::init_options()
{
    exa_dgcommand::init_options();

    add_option('r', "recursive", "Recursively delete the volumes"
#ifdef WITH_FS
               " and file systems"
#endif
               " in the disk group.\n" +
               Boldify("CAUTION! All the data on this disk group will be "
               "erased"), 0, false, false);

    add_option('f', "force", "Force delete in case of error.", 0, true, false);
}


void exa_dgdelete::init_see_alsos()
{
    add_see_also("exa_dgcreate");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgstop");
    add_see_also("exa_dgdiskrecover");
}


void exa_dgdelete::run()
{
    string error_msg;
    char msg_str[80];

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    /*
     * Create command
     */
    AdmindCommand command("dgdelete", exa.get_cluster_uuid());
    command.add_param("groupname", _group_name);
    command.add_param("force", _forcemode);
    command.add_param("recursive", _recursive);

    exa_cli_info("Deleting disk group '%s' for cluster '%s'\n",
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    os_snprintf(msg_str,
                sizeof(msg_str),
                "systems of the disk group '%s'.",
                _group_name.c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Group delete:", error_code, error_message);

    if (error_code == VRT_ERR_GROUP_NOT_STOPPED)
        throw CommandException(
            "It is not allowed to delete a started group. Please use exa_dgstop first.");

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_dgdelete::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);

    if (opt_args.find('f') != opt_args.end())
        _forcemode = true;

    if (opt_args.find('r') != opt_args.end())
        _recursive = true;
}


void exa_dgdelete::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Delete an Exanodes disk group.";
}


void exa_dgdelete::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Delete the disk group " << ARG_DISKGROUP_GROUPNAME <<
    " from the cluster "
        << ARG_DISKGROUP_CLUSTERNAME << std::endl;
}


void exa_dgdelete::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Delete the disk group " << Boldify("mygroup")
        << " from the cluster " << Boldify("mycluster") << std::endl;
    out << std::endl;
    out << "  " << "exa_dgdelete mycluster:mygroup" << std::endl;
}


