/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vlresize.h"

#include <errno.h>
#include <boost/lexical_cast.hpp>

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_stdio.h"

using std::string;

const std::string exa_vlresize::OPT_ARG_SIZE_SIZE(Command::Boldify("SIZE"));

exa_vlresize::exa_vlresize()
    : nofscheck(false)
    , sizeKB_uu64(0)
    , size_max(false)
{
    add_option('s', "size", "The new size, with a unit symbol like in 10G. The "
               "unit can be one char of K, M, G, T, P, E (For Kibi, Mebi, "
               "Gibi, Tebi, Pebi, Exbi). The decimal point is accepted like in "
               "1.2T. The special value 'max' means all available space in the "
               "disk group.", 1, false, true, OPT_ARG_SIZE_SIZE);
#ifdef WITH_FS
    add_option('F', "nofscheck", "Force the resizing even if the volume is "
               "currently part of a file system. WARNING! you won't be able to "
               "use your file system anymore", 0, true, false);
#endif
}


void exa_vlresize::init_see_alsos()
{
    add_see_also("exa_vlcreate");
    add_see_also("exa_vldelete");
    add_see_also("exa_vlstart");
    add_see_also("exa_vlstop");
    add_see_also("exa_vltune");
}


void exa_vlresize::run()
{
    string error_msg;
    string sizeKB_str;
    char human_size[EXA_MAXSIZE_LINE + 1];

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    sizeKB_str = boost::lexical_cast<std::string>(sizeKB_uu64);
    /*
     * Resize command
     */
    AdmindCommand command("vlresize", exa.get_cluster_uuid());
    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);
    command.add_param("size", sizeKB_str);
    command.add_param("no_fs_check", nofscheck);

    if (size_max)
        exa_cli_info(
            "Resizing to maximum size the logical volume '%s' in the group '%s' for cluster '%s'\n",
            _volume_name.c_str(),
            _group_name.c_str(),
            exa.get_cluster().c_str());
    else
    {
        exa::to_human_size(human_size, EXA_MAXSIZE_LINE + 1, sizeKB_uu64);
        exa_cli_info(
            "Resizing to %s the logical volume '%s' in the group '%s' for cluster '%s'\n",
            human_size,
            _volume_name.c_str(),
            _group_name.c_str(),
            exa.get_cluster().c_str());
    }

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Volume resize:", error_code, error_message);

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


void exa_vlresize::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

#ifdef WITH_FS
    if (opt_args.find('F') != opt_args.end())
        nofscheck = true;
#endif

    if (opt_args.find('s') != opt_args.end())
    {
        const std::string &value(opt_args.find('s')->second);
        if (value == "max")
            size_max = true;
        else
        {
            sizeKB_uu64 = exa::to_size_kb(value);

            if (sizeKB_uu64 == 0)
                throw CommandException("Cannot resize a volume to 0KB.");
        }
    }
}


void exa_vlresize::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Resize an Exanodes volume.";
}


void exa_vlresize::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Resize the volume " << ARG_VOLUME_VOLUMENAME << " of the group "
        << ARG_VOLUME_GROUPNAME << " of the cluster " << ARG_VOLUME_CLUSTERNAME
        << " to the new size " << OPT_ARG_SIZE_SIZE <<
    ". This command works online "
        <<
    "(ie. when the volume is started and in use). If you reduce the size,"
        <<
    " you have FIRST to resize the file system currently running on the volume"
        <<
    " using its own resizing tool. If this is not done, the file system will "
        << "stop working and you will lose your data." << std::endl;
    out << std::endl;
}


void exa_vlresize::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Resize the volume " << Boldify("myvolume") << " of the group "
        << Boldify("mygroup") << " to 20 GiB:" << std::endl;
    out << "  " << "exa_vlresize --size 20G mycluster:mygroup:myvolume" <<
    std::endl;
    out << std::endl;
}


