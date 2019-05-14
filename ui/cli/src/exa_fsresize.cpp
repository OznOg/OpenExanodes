/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fsresize.h"

#include <errno.h>
#include <boost/lexical_cast.hpp>
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

using std::string;

const std::string exa_fsresize::OPT_ARG_SIZE_SIZE(Command::Boldify("SIZE"));

exa_fsresize::exa_fsresize()
    : sizeKB_uu64(0)
    , size_max(false)
{
    add_option('s', "size", "The new size, with a unit symbol like in 10G. "
               "The unit can be one char of K, M, G, T, P, E (For Kibi, Mebi, "
               "Gibi, Tebi, Pebi, Exbi). The decimal point is accepted like in "
               "1.2T. The special value 'max' means all available space in the "
               "disk group.", 1, false, true, OPT_ARG_SIZE_SIZE);

    add_see_also("exa_fscreate");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fscheck");
    add_see_also("exa_fsstart");
    add_see_also("exa_fsstop");
    add_see_also("exa_fstune");
}


void exa_fsresize::run()
{
    string error_msg;
    string sizeKB_str;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    sizeKB_str = boost::lexical_cast<std::string>(sizeKB_uu64);

    /*
     * Create command
     */
    AdmindCommand command("fsresize", exa.get_cluster_uuid());
    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _fs_name);
    command.add_param("sizeKB", sizeKB_str);

    exa_cli_info("Resizing file system '%s' in group '%s' for cluster '%s'\n",
                 _fs_name.c_str(),
                 _group_name.c_str(),
                 exa.get_cluster().c_str());

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "File System resize:", error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        throw CommandException("The disk group is not started. "
                               "Please use exa_dgstart first.", error_code);
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fsresize::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('s') != opt_args.end())
    {
        const std::string &value(opt_args.find('s')->second);
        if (value == "max")
        {
            /* TODO */
            throw CommandException(
                "TODO : size = max is not supported in admind resize command");
            size_max = true;
        }
        else
        {
            sizeKB_uu64 = exa::to_size_kb(value);

            if (sizeKB_uu64 == 0)
                throw CommandException(
                    "Cannot resize a file system with a 0KB size.");
        }
    }
}


void exa_fsresize::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Resize a file system managed by Exanodes.";
}


void exa_fsresize::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Resize the file system " << ARG_FILESYSTEM_FSNAME <<
    " from the disk group "
        << ARG_FILESYSTEM_GROUPNAME << " of the cluster " <<
    ARG_FILESYSTEM_CLUSTERNAME
        << " to size " << OPT_ARG_SIZE_SIZE << "." << std::endl;
}


void exa_fsresize::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Resize the file system " << Boldify("mysfs") <<
    " of the disk group "
        << Boldify("mygroup") << " of the cluster " << Boldify("mycluster")
        << " to 100GiB:" << std::endl;
    out << "  " << "exa_fsresize --size 100G mycluster:mygroup:mysfs" <<
    std::endl;
    out << std::endl;
}


