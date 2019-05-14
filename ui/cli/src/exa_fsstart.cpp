/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fsstart.h"

#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_fsstart::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                          "HOSTNAMES"));
const std::string exa_fsstart::OPT_ARG_MOUNTPOINT_PATH(Command::Boldify("PATH"));

exa_fsstart::exa_fsstart()
    : allnodes(false)
    , mount_point("")
    , read_only(false)
{
    add_option('n', "node", "Specify the nodes on which to start this file "
               "system. This option is a regular expansion (see exa_expand).",
               1, false, true, OPT_ARG_NODE_HOSTNAMES);
    add_option('a', "all", "Start the file system on all the nodes.",
               1, false, false);
    add_option('m', "mountpoint", "Temporarily change mountpoint. Allowed only "
               "if the file system is not already mounted on some nodes.",
               0, false, true, OPT_ARG_MOUNTPOINT_PATH);
    add_option('r', "read-only", "Start the file system in read-only mode. "
               "This allows you to start a local file system on more than one "
               "node at a time. You cannot use this option if the file system "
               "is already started on one or more nodes. To go back in "
               "Read-Write mode, you must first stop the file system on all "
               "your nodes.", 0, false, false);

    add_see_also("exa_fscreate");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fsresize");
    add_see_also("exa_fscheck");
    add_see_also("exa_fsstop");
    add_see_also("exa_fstune");
    add_see_also("exa_expand");
}


void exa_fsstart::run()
{
    string error_msg;

    std::set<std::string> target_nodes;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (!allnodes)
    {
        try
        {
            target_nodes = exa_expand(nodes);
        }
        catch (string msg)
        {
            throw CommandException(msg);
        }
    }

    /*
     * Start command
     */
    AdmindCommand command("fsstart", exa.get_cluster_uuid());
    command.add_param("volume_name", _fs_name);
    command.add_param("group_name", _group_name);
    command.add_param("mountpoint", mount_point);
    command.add_param("host_list", strjoin(" ", target_nodes));
    command.add_param("read_only", read_only);

    exa_cli_info("Starting file system '%s' in group '%s' for cluster '%s'\n",
                 _fs_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Starting file system:", error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        exa_cli_error(
            "\n%sERROR%s: The disk group is not started. Please use exa_dgstart first.\n",
            COLOR_ERROR,
            COLOR_NORM);
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fsstart::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('a') != opt_args.end())
        allnodes = true;

    if (opt_args.find('m') != opt_args.end())
        mount_point = opt_args.find('m')->second;

    if (opt_args.find('n') != opt_args.end())
        nodes = opt_args.find('n')->second;

    if (opt_args.find('r') != opt_args.end())
        read_only = true;
}


void exa_fsstart::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Start (mount) a file system managed by Exanodes.";
}


void exa_fsstart::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out <<
    "Start (mount) a file system managed by Exanodes on the specified nodes."
        <<
    " A local file system (ext3, XFS) can be started on only one node at a time."
        <<
    " A distributed file system (Seanodes FS) can be started on several nodes"
        << " concurrently." << std::endl;
}


void exa_fsstart::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Mount the file system " << Boldify("mysfs") << " of the group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster")
        << " on all the nodes:" << std::endl;
    out << "  " << "exa_fsstart --all mycluster:mygroup:mysfs" << std::endl;
    out << std::endl;

    out << "Mount the file system " << Boldify("myfs") << " of the group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster") <<
    " on the nodes "
        << Boldify("node1") << " and " << Boldify("node2") << ":" << std::endl;
    out << "  " << "exa_fsstart --node node/1-2/ mycluster:mygroup:myfs" <<
    std::endl;
}


