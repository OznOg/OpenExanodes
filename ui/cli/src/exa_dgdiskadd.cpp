/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_dgdiskadd.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/split_node_disk.h"
#include "common/include/exa_config.h"

using std::string;

const std::string exa_dgdiskadd::OPT_ARG_DISK_HOSTNAME(Command::Boldify(
                                                           "HOSTNAME"));
const std::string exa_dgdiskadd::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));

exa_dgdiskadd::exa_dgdiskadd()
{
    add_option('i', "disk", "Specify disk to add.", 1, false, true,
               OPT_ARG_DISK_HOSTNAME + EXA_CONF_SEPARATOR + OPT_ARG_DISK_PATH);

    add_see_also("exa_dgcreate");
    add_see_also("exa_dgdelete");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgstop");
    add_see_also("exa_cldiskadd");
}


void exa_dgdiskadd::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    /* Build the command */
    AdmindCommand command("dgdiskadd", exa.get_cluster_uuid());
    command.add_param("group_name",  _group_name);
    command.add_param("node_name", _node_name);
    command.add_param("disk_path", _disk_path);

    string msg_str = "Adding disk " + _node_name + EXA_CONF_SEPARATOR
                     + _disk_path + " in group '" + exa.get_cluster()
                     + EXA_CONF_SEPARATOR + _group_name + "'";

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, msg_str, error_code, error_message);

    if (error_code == VRT_ERR_GROUP_NOT_STOPPED)
        exa_cli_error("\n%sERROR%s: The disk group is not stopped. "
                      "Please use exa_dgstop first.\n",
                      COLOR_ERROR, COLOR_NORM);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_dgdiskadd::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);

    if (opt_args.find('i') != opt_args.end())
    {
        split_node_disk(opt_args.find('i')->second, _node_name, _disk_path);

        if (_disk_path.empty() || _node_name.empty())
            throw CommandException("Invalid disk identifier",
                                   EXA_ERR_INVALID_PARAM);
    }
}


void exa_dgdiskadd::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Add an unassigned disk to a group.";
}


void exa_dgdiskadd::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Add the disk " << OPT_ARG_DISK_PATH
        << " of the node " << OPT_ARG_DISK_HOSTNAME
        << " to the group " << ARG_DISKGROUP_GROUPNAME
        << " of cluster " << ARG_DISKGROUP_CLUSTERNAME
        << "." << std::endl;

    out << "Only disks already in the cluster, but not assigned to any group "
        << "can be added to a group. " << std::endl
        << "To add a disk to the cluster if needed, use 'exa_cldiskadd'."
        << std::endl;
}


void exa_dgdiskadd::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Add the unassigned disk " << Boldify("/dev/sdb") << " (on Linux)"
        << " or " << Boldify("E:") << " (on Windows)"
        << " to the group " << Boldify("mygroup") << " of " << Boldify(
        "mycluster")
        << ":" << std::endl;
    out << std::endl;
    out << "    Linux: exa_dgdiskadd --disk node1" << EXA_CONF_SEPARATOR
        << "/dev/sdb mycluster" << EXA_CONF_SEPARATOR << "mygroup" << std::endl;
    out << "  Windows: exa_dgdiskadd --disk node1" << EXA_CONF_SEPARATOR
        << "E: mycluster" << EXA_CONF_SEPARATOR << "mygroup" << std::endl;
    out << std::endl;
}


