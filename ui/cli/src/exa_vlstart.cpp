/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vlstart.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_vlstart::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                          "HOSTNAMES"));

exa_vlstart::exa_vlstart() :
#ifdef WITH_FS
     nofscheck(false),
#endif
     allnodes(true)
    , readonly(false)
{}

void exa_vlstart::init_options()
{
    exa_vlcommand::init_options();

#ifdef WITH_BDEV
    add_option('r', "read-only",
               "Do not allow writes on this volume for specified nodes.",
               0, false, false);
#endif

#ifdef WITH_FS
    add_option('F', "nofscheck", "Force the export even if the volume is "
               "currently part of a file system. WARNING! you should always "
               "use exa_fs commands to manage your file systems.", 0, true,
               false);
#endif

#ifdef WITH_BDEV
    add_option('n', "node", "Specify the nodes on which to export this volume,"
                            " for bdev volumes. This option is a regular"
                            " expansion (see exa_expand).", 0,
               false, true, OPT_ARG_NODE_HOSTNAMES);
#endif
    add_option('a', "all", "Export the volume on all nodes of the cluster. "
                           "(Deprecated option: this is the default)",
               0, false, false);
}


void exa_vlstart::init_see_alsos()
{
    add_see_also("exa_vlcreate");
    add_see_also("exa_vldelete");
    add_see_also("exa_vlresize");
    add_see_also("exa_vlstop");
    add_see_also("exa_vltune");
}


void exa_vlstart::run()
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
    AdmindCommand command("vlstart", exa.get_cluster_uuid());
    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);
    command.add_param("host_list", strjoin(" ", target_nodes));
#ifdef WITH_FS
    command.add_param("no_fs_check", nofscheck);
#endif
    command.add_param("readonly", readonly);

    exa_cli_info("Starting volume '%s' in the group '%s' for cluster '%s'\n",
                 _volume_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Volume start:", error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        exa_cli_error(
            "\n%sERROR%s: The disk group is not started. Please use exa_dgstart first.\n",
            COLOR_ERROR,
            COLOR_NORM);
        break;

    case ADMIND_ERR_METADATA_CORRUPTION:
        exa_cli_info(
            "Please run exa_vldelete with the --metadata-recovery option.\n");
        break;

    case ADMIND_ERR_VOLUME_ACCESS_MODE:
        if (readonly)
            exa_cli_error(
                "%sERROR%s: You cannot change the volume's access mode from read/write to read-only.\n"
                "       You must stop the volume on the nodes where it is started read/write\n"
                "       before starting it read-only there.\n",
                COLOR_ERROR,
                COLOR_NORM);
        else
            exa_cli_error(
                "%sERROR%s: You cannot change the volume's access mode from read-only to read/write.\n"
                "       You must stop the volume on the nodes where it is started read-only\n"
                "       before starting it read/write there.\n",
                COLOR_ERROR,
                COLOR_NORM);
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_vlstart::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

#ifdef WITH_FS
    if (opt_args.find('F') != opt_args.end())
        nofscheck = true;
#endif

    if (opt_args.find('n') != opt_args.end())
    {
        allnodes = false;
        nodes = opt_args.find('n')->second;
    }

#ifdef WITH_BDEV
    if (opt_args.find('r') != opt_args.end())
        readonly = true;
#endif
}


void exa_vlstart::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Export an Exanodes volume.";
}


void exa_vlstart::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Export the volume " << ARG_VOLUME_VOLUMENAME <<
    " of the disk group "
        << ARG_VOLUME_GROUPNAME << " of the cluster " << ARG_VOLUME_CLUSTERNAME
        << "." << std::endl;
}


void exa_vlstart::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Assuming " << Boldify("mycluster") << " is comprised of nodes "
        << Boldify("node/1-5/") << ", export the volume " << Boldify("myvolume")
        << " of group " << Boldify("mygroup") << " on nodes "
        << Boldify("node1") << ", " << Boldify("node2") << ", "
        << Boldify("node3") << " and " << Boldify("node5") << ":" << std::endl;
    out << std::endl;
    out << "  " << "exa_vlstart mycluster:mygroup:myvolume" << std::endl;
}


