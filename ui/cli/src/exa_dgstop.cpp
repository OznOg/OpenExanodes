/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_dgstop.h"

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

using std::string;

exa_dgstop::exa_dgstop()
    : _recursive(false)
    , _force(false)
{}

void exa_dgstop::init_options()
{
    exa_dgcommand::init_options();

    add_option('r', "recursive", "Recursively stop the volumes"
#ifdef WITH_FS
               " and file systems"
#endif
               " in the disk group.", 0, false, false);
    add_option('f', "force", "Force the recursive stop when the disk group "
               "is OFFLINE.", 0, false, false);
}


void exa_dgstop::init_see_alsos()
{
    add_see_also("exa_dgdelete");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgdiskrecover");
}


void exa_dgstop::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (_recursive)
        exa_cli_info("Please wait, recursive disk group stop is in progress.\n");

    /*
     * Create command
     */
    AdmindCommand command("dgstop", exa.get_cluster_uuid());
    command.add_param("groupname", _group_name);
    command.add_param("recursive",
                      _recursive ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
    command.add_param("force", _force);

    exa_cli_info("Stopping disk group '%s' for cluster '%s'\n",
                 _group_name.c_str(),
                 exa.get_cluster().c_str());
    fflush(stdout); /* FIXME: is this really necessary? */

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Group stop:", error_code, error_message);

    switch (error_code)
    {
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


void exa_dgstop::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);

    if (opt_args.find('f') != opt_args.end())
        _force = true;
    if (opt_args.find('r') != opt_args.end())
        _recursive = true;
}


void exa_dgstop::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Stop an Exanodes disk group.";
}


void exa_dgstop::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Stop the disk group " << ARG_DISKGROUP_GROUPNAME
        << " in the cluster " << ARG_DISKGROUP_CLUSTERNAME << "." << std::endl;
    out <<
    "The --force option allow to recursively stop an OFFLINE disk group." <<
    std::endl;
    out << "" <<
    Boldify("CAUTION! All data in the Linux page cache will not") +
    "\n" + Boldify("be synced and will be lost.")
        << "" << std::endl;
}


void exa_dgstop::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Stop the disk group " << Boldify("mygroup") << " in the cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_dgstop mycluster:mygroup" << std::endl;
    out << std::endl;
}


