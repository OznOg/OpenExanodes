/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_dgdiskrecover.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/split_node_disk.h"
#include "common/include/exa_config.h"

using std::string;

const std::string exa_dgdiskrecover::OPT_ARG_OLD_DISK_UUID(Command::Boldify(
                                                               "UUID1"));
const std::string exa_dgdiskrecover::OPT_ARG_NEW_DISK_UUID(Command::Boldify(
                                                               "UUID2"));

void exa_dgdiskrecover::init_options()
{
    exa_dgcommand::init_options();

    add_option('O', "old", "UUID of disk to be replaced.", 1, false, true,
               OPT_ARG_OLD_DISK_UUID);

    add_option('N', "new",
               "UUID of replacement disk. The disk must be unassigned"
               " (not part of any group) and on the same node as the disk"
               " to be replaced.", 2, false, true, OPT_ARG_NEW_DISK_UUID);
}


void exa_dgdiskrecover::init_see_alsos()
{
    add_see_also("exa_dgcreate");
    add_see_also("exa_dgdelete");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgstop");
}


void exa_dgdiskrecover::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    /*
     * Create command
     */
    AdmindCommand command("dgdiskrecover", exa.get_cluster_uuid());
    command.add_param("group_name",  _group_name);
    command.add_param("old_disk", _old_disk);
    command.add_param("new_disk", _new_disk);

    exa_cli_info(
        "Recovering group '%s:%s', replacing disk '%s' with disk '%s'\n",
        exa.get_cluster().c_str(),
        _group_name.c_str(),
        _old_disk.c_str(),
        _new_disk.c_str());

    string msg_str = "Replace disk '" + _old_disk + "' with disk '" +
                     _new_disk + "':";

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, msg_str, error_code, error_message);

    if (error_code == VRT_ERR_GROUP_NOT_STARTED)
        exa_cli_error(
            "\n%sERROR%s: The disk group is not started. Please use exa_dgstart first.\n",
            COLOR_ERROR,
            COLOR_NORM);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_dgdiskrecover::parse_opt_args(const std::map<char,
                                                      std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);

    if (opt_args.find('O') != opt_args.end())
    {
        _old_disk = opt_args.find('O')->second;
        if (_old_disk.empty())
            throw CommandException("Invalid old disk UUID");
    }

    if (opt_args.find('N') != opt_args.end())
    {
        _new_disk = opt_args.find('N')->second;
        if (_new_disk.empty())
            throw CommandException("Invalid new disk UUID");
    }
}


void exa_dgdiskrecover::dump_short_description(std::ostream &out,
                                               bool show_hidden) const
{
    out <<
    "Recover an Exanodes disk group by rebuilding data on one of its disks.";
}


void exa_dgdiskrecover::dump_full_description(std::ostream &out,
                                              bool show_hidden) const
{
    out << "Recover the disk group " << ARG_DISKGROUP_GROUPNAME
        << " of the cluster " << ARG_DISKGROUP_CLUSTERNAME <<
    " by replacing broken disk "
        << OPT_ARG_OLD_DISK_UUID << " with unassigned disk " <<
    OPT_ARG_NEW_DISK_UUID
        << " and rebuilding data on it."
        <<
    " It is not possible to replace a disk on a node by a disk on another node."
        << std::endl;

    out << "This command is useful to replace a failed disk by a new one." <<
    std::endl;
}


void exa_dgdiskrecover::dump_examples(std::ostream &out,
                                      bool show_hidden) const
{
    out << "In cluster " << Boldify("mycluster") << ", recover BROKEN disk "
        << Boldify("30E843E9:18362B65:38D2924C:5375A944")
        << " in group " << Boldify("mygroup")
        << " by replacing it with unassigned disk " << Boldify(
        "3C3C74AC:E1562795:14A5E441:B8C3CF5C") << ":" << std::endl;
    out << std::endl;
    out << "    exa_dgdiskrecover --old 30E843E9:18362B65:38D2924C:5375A944"
        << " --new 3C3C74AC:E1562795:14A5E441:B8C3CF5 mycluster:mygroup" <<
    std::endl;
}


