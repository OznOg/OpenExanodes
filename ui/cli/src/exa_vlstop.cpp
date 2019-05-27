/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vlstop.h"

#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_vlstop::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                         "HOSTNAMES"));

exa_vlstop::exa_vlstop()
    : nofscheck(false)
    , force(false)
    , allnodes(true)
{
#ifdef WITH_BDEV
    add_option('n', "node", "Specify the nodes on which to stop this volume, "
               "for bdev volumes. This option is a regular expansion (see "
               "exa_expand).",
               0, false, true, OPT_ARG_NODE_HOSTNAMES);
#endif
    add_option('a', "all", "Stop the volume on the whole cluster. "
                           "(Deprecated option: this is the default)",
               0, false, false);
    add_option('F', "nofscheck", "Force the stop even if a volume is "
               "currently part of a file system. WARNING! Next file system "
               "start will fail.", 0, true, false);
    add_option('f', "force", "Continue the stop even if something goes wrong. "
               "CAUTION! This option is very dangerous.", 0, true, false);

    add_see_also("exa_vlcreate");
    add_see_also("exa_vldelete");
    add_see_also("exa_vlresize");
    add_see_also("exa_vlstart");
    add_see_also("exa_vltune");
}


void exa_vlstop::run()
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
    AdmindCommand command("vlstop", exa.get_cluster_uuid());
    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);
    command.add_param("host_list", strjoin(" ", target_nodes));
    command.add_param("no_fs_check", nofscheck);
    command.add_param("force", force);

    exa_cli_info("Stopping volume '%s' in the group '%s' for cluster '%s'\n",
                 _volume_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    send_command(command, "Volume stop:", error_code, error_msg);

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


void exa_vlstop::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

    if (opt_args.find('F') != opt_args.end())
        nofscheck = true;

    if (opt_args.find('n') != opt_args.end())
    {
        allnodes = false;
        nodes = opt_args.find('n')->second;
    }

    if (opt_args.find('f') != opt_args.end())
        force = true;
}


void exa_vlstop::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Stop (unexport) an Exanodes volume.";
}


void exa_vlstop::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Stop (unexport) the volume " << ARG_VOLUME_VOLUMENAME <<
    " of the disk group "
        << ARG_VOLUME_GROUPNAME << " of the cluster " << ARG_VOLUME_CLUSTERNAME
        << "." << std::endl;
}


void exa_vlstop::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Stop the volume " << Boldify("myvolume") << " of the group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster")
        << " on the nodes " << Boldify("node1") << ", " << Boldify("node2") <<
    ", "
        << Boldify("node3") << " and " << Boldify("node5") << ":" << std::endl;
    out << "  " <<
    "exa_vlstop --node 'node/1-3/ node5' mycluster:mygroup:myvolume" <<
    std::endl;
    out << std::endl;

    out << "Stop the volume " << Boldify("myvolume") << " of the group "
        << Boldify("mygroup") << " on all the nodes of the cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_vlstop --all mycluster:mygroup:myvolume" << std::endl;
    out << std::endl;
}


