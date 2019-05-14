/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fsstop.h"

#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_fsstop::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                         "HOSTNAMES"));

exa_fsstop::exa_fsstop()
    : allnodes(false)
    , forcemode(false)
{
    add_option('n', "node", "Specify the nodes on which to stop this file "
               "system. This option is a regular expansion (see exa_expand).",
               1, false, true, OPT_ARG_NODE_HOSTNAMES);
    add_option('a', "all", "Stop the file system on all the nodes.",
               1, false, false);
    add_option('f', "force", "Continue the stop even if something goes wrong, "
               "eg. if the associated data volume is DOWN. CAUTION! This "
               "option is very dangerous.", 0, true, false);

    add_see_also("exa_fscreate");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fsresize");
    add_see_also("exa_fsstart");
    add_see_also("exa_fscheck");
    add_see_also("exa_fstune");
    add_see_also("exa_expand");
}


void exa_fsstop::run()
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
     * Create command
     */
    AdmindCommand command("fsstop", exa.get_cluster_uuid());
    command.add_param("volume_name", _fs_name);
    command.add_param("group_name", _group_name);
    command.add_param("host_list", strjoin(" ", target_nodes));
    command.add_param("force", forcemode);

    exa_cli_info("Stopping file system '%s' in group '%s' for cluster '%s'\n",
                 _fs_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Stopping file system:", error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        exa_cli_error(
            "\n%sERROR%s: The disk group is not started. Please use exa_dgstart first.\n",
            COLOR_ERROR,
            COLOR_NORM);
        break;

    case FS_ERR_STOP_WITH_VOLUME_DOWN:
        exa_cli_info(
            "You can use option --force if you really want to stop the volume.\n");
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fsstop::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('a') != opt_args.end())
        allnodes = true;

    if (opt_args.find('n') != opt_args.end())
        nodes = opt_args.find('n')->second;

    if (opt_args.find('f') != opt_args.end())
        forcemode = true;
}


void exa_fsstop::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Stop (unmount) a file system managed by Exanodes.";
}


void exa_fsstop::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out <<
    "Stop (unmount) a file system managed by Exanodes on the specified nodes."
        << std::endl;
}


void exa_fsstop::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Stop the file system " << Boldify("mysfs") << " of the group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster")
        << " on all the nodes:" << std::endl;
    out << "  " << "exa_fsstop --all mycluster:mygroup:mysfs" << std::endl;
    out << std::endl;

    out << "Stop the file system " << Boldify("myfs") << " of the group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster") <<
    " on the nodes "
        << Boldify("node1") << " and " << Boldify("node2") << ":" << std::endl;
    out << "  " << "exa_fsstop --node node/1-2/ mycluster:mygroup:myfs" <<
    std::endl;
    out << std::endl;
}


