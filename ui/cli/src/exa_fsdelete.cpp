/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fsdelete.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_stdio.h"

using std::string;

exa_fsdelete::exa_fsdelete()
    : metadata_recovery(false)
{}

void exa_fsdelete::init_options()
{
    exa_fscommand::init_options();

    add_option('M', "metadata-recovery",
               "Force deletion after a failure during an exa_fscreate.",
               0, false, false);
}


void exa_fsdelete::init_see_alsos()
{
    add_see_also("exa_fscreate");
    add_see_also("exa_fscheck");
    add_see_also("exa_fsresize");
    add_see_also("exa_fsstart");
    add_see_also("exa_fsstop");
    add_see_also("exa_fstune");
}


void exa_fsdelete::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    /*
     * Create command
     */
    AdmindCommand command("fsdelete", exa.get_cluster_uuid());
    command.add_param("volume_name", _fs_name);
    command.add_param("group_name", _group_name);
    command.add_param("metadata_recovery", metadata_recovery);

    exa_cli_info("Deleting file system '%s' in group '%s' for cluster '%s'\n",
                 _fs_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Deleting file system:", error_code, error_message);

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
            "Please run exa_fsdelete with the --metadata-recovery option.\n");
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fsdelete::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('M') != opt_args.end())
        metadata_recovery = true;
}


void exa_fsdelete::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Delete a file system managed by Exanodes.";
}


void exa_fsdelete::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Delete the file system " << ARG_FILESYSTEM_FSNAME
        << " from the disk group " << ARG_FILESYSTEM_GROUPNAME <<
    " of the cluster "
        << ARG_FILESYSTEM_CLUSTERNAME << "." << std::endl;
}


void exa_fsdelete::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Delete the file system " << Boldify("myfs") << " from the group "
        << Boldify("mygroup") << " from the cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_fsdelete mycluster:mygroup:myfs" << std::endl;
    out << std::endl;
}


