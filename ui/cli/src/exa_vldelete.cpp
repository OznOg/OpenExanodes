/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vldelete.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_stdio.h"

using std::string;

exa_vldelete::exa_vldelete()
    : nofscheck(false)
    , metadata_recovery(false)
{
    add_option('M', "metadata-recovery", "Force deletion after a failure "
               "during an exa_vlcreate.", 0, false, false);

    add_option('F', "nofscheck", "Force the deletion even if the volume is "
               "currently part of a file system. WARNING! you won't be able to "
               "use your file system anymore", 0, true, false);

    add_see_also("exa_vlcreate");
    add_see_also("exa_vlresize");
    add_see_also("exa_vlstart");
    add_see_also("exa_vlstop");
    add_see_also("exa_vltune");
}


void exa_vldelete::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    /*
     * Create command
     */
    AdmindCommand command("vldelete", exa.get_cluster_uuid());
    command.add_param("group_name",  _group_name);
    command.add_param("volume_name", _volume_name);
    command.add_param("no_fs_check",  nofscheck);
    command.add_param("metadata_recovery", metadata_recovery);

    exa_cli_info(
        "Deleting logical volume '%s' in the group '%s' for cluster '%s'\n",
        _volume_name.c_str(),
        _group_name.c_str(),
        exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Volume delete:", error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        throw CommandException("The disk group is not started."
                               " Please use exa_dgstart first.", error_code);
        break;

    case VRT_ERR_VOLUME_NOT_STOPPED:
        throw CommandException(
            "It is not allowed to delete a started logical volume."
            " Please use exa_vlstop first.");
        break;

    case ADMIND_ERR_METADATA_CORRUPTION:
        exa_cli_info("Please run exa_vldelete --metadata-recovery.\n");

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_vldelete::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

    if (opt_args.find('F') != opt_args.end())
        nofscheck = true;

    if (opt_args.find('M') != opt_args.end())
        metadata_recovery = true;
}


void exa_vldelete::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Delete an Exanodes volume.";
}


void exa_vldelete::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Delete the volume " << ARG_VOLUME_VOLUMENAME <<
    " from the disk group "
        << ARG_VOLUME_GROUPNAME << " from the cluster " <<
    ARG_VOLUME_CLUSTERNAME << "."
                           << std::endl;
}


void exa_vldelete::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Delete the volume " << Boldify("myvolume") << " from the group "
        << Boldify("mygroup") << " from the cluster " << Boldify("mycluster")
        << ":" << std::endl;
    out << "  " << "exa_vldelete mycluster:mygroup:myvolume" << std::endl;
    out << std::endl;
}


