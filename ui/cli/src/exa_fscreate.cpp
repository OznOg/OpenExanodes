/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fscreate.h"

#include <errno.h>
#include <boost/lexical_cast.hpp>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"

using std::string;

const std::string exa_fscreate::OPT_ARG_MOUNTPOINT_PATH(Command::Boldify("PATH"));
const std::string exa_fscreate::OPT_ARG_SIZE_SIZE(Command::Boldify("SIZE"));
const std::string exa_fscreate::OPT_ARG_TYPE_FSTYPE(Command::Boldify("FSTYPE"));
const std::string exa_fscreate::OPT_ARG_NBJOURNALS_NB(Command::Boldify("NB"));
const std::string exa_fscreate::OPT_ARG_RGSIZE_SIZE(Command::Boldify("SIZE"));

exa_fscreate::exa_fscreate()
    : sizeKB_uu64(0)
    , rg_sizeM(0)
    , nb_logs(-1)
{
    add_option('m', "mountpoint", "Mountpoint directory to be used on your "
               "nodes.", 1, false, true, OPT_ARG_MOUNTPOINT_PATH);
    add_option('t', "type", "File system type. Supported file systems are:\n"
               " - Local file systems: ext3, XFS\n"
               " - Distributed file systems: sfs (Seanodes FS).\n"
               "(Seanodes FS is based on RedHat GFS).", 2, false, true,
               OPT_ARG_TYPE_FSTYPE);
    add_option('s', "size", "The size must be specified with a unit symbol "
               "like in 10G. The unit can be one char of K, M, G, T, P, E "
               "(For Kibi, Mebi, Gibi, Tebi, Pebi, Exbi). The decimal point is "
               "accepted like in 1.2T. The special value 'max' means all "
               "available space in the disk group.", 3, false, true,
               OPT_ARG_SIZE_SIZE);
    add_option('j', "nb-journals", "Number of journals in a Seanodes file "
               "system. This is the number of nodes that can mount the file "
               "system concurrently.", 0, false, true, OPT_ARG_NBJOURNALS_NB);
    add_option('r', "rg-size", "Size of resource groups in a Seanodes file "
               "system. Minimum would be 256M. Bigger means better performance "
               "with risks of lock contention.", 0, false, true,
               OPT_ARG_RGSIZE_SIZE);
}


void exa_fscreate::init_see_alsos()
{
    add_see_also("exa_fscheck");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fsresize");
    add_see_also("exa_fsstart");
    add_see_also("exa_fsstop");
    add_see_also("exa_fstune");
}


void exa_fscreate::run()
{
    string error_msg;
    exa_error_code error_code = EXA_SUCCESS;

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME, EXA_MAXSIZE_FSTYPE,
                                 fs_type, false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME,
                                 EXA_MAXSIZE_VOLUMENAME, _fs_name,
                                 false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);
    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME, EXA_MAXSIZE_GROUPNAME,
                                 _group_name, false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);
    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME,
                                 EXA_MAXSIZE_CLUSTERNAME, _cluster_name,
                                 false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", _cluster_name.c_str());
    exa_cli_trace("group=%s\n", _group_name.c_str());
    exa_cli_trace("fs=%s\n", _fs_name.c_str());

    exa_cli_info("Creating FS '%s' for cluster '%s' in main group '%s'\n",
                 _fs_name.c_str(),
                 exa.get_cluster().c_str(),
                 _group_name.c_str());

    std::string sizeKB_str(boost::lexical_cast<std::string>(sizeKB_uu64));
    std::string rg_sizeM_str(boost::lexical_cast<std::string>(rg_sizeM));

    AdmindCommand command("fscreate", exa.get_cluster_uuid());
    command.add_param("type", fs_type);
    command.add_param("sizeKB", sizeKB_str);
    command.add_param("rg_sizeM", rg_sizeM_str);
    command.add_param("mountpoint", mount_point);
    command.add_param("volume_name", _fs_name);
    command.add_param("group_name", _group_name);

    if (nb_logs != -1)
        command.add_param("sfs_nb_logs", nb_logs);

    exa_cli_notice("%sNOTICE%s: Formatting a file system may be very long\n",
                   COLOR_NOTICE,
                   COLOR_NORM);

    /* Send the command and receive the response */
    string error_message;
    send_command(command, "Creating file system:", error_code, error_message);

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


void exa_fscreate::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('t') != opt_args.end())
        fs_type = opt_args.find('t')->second;

    if (opt_args.find('m') != opt_args.end())
        mount_point = opt_args.find('m')->second;

    if (opt_args.find('j') != opt_args.end())
    {
        char *endptr;
        nb_logs = strtol((const char *) opt_args.find('j')->second.c_str(),
                         &endptr, 0);
        if ((*endptr) || (nb_logs < 1))
            throw CommandException("Invalid number of journals for SFS.");
        if ((fs_type != FS_NAME_GFS) && (nb_logs != -1))
            throw CommandException(
                "Specifying number of journals is only valid on a Seanodes filesystem.");
    }

    if (opt_args.find('s') != opt_args.end())
    {
        const std::string &value(opt_args.find('s')->second);
        if (value == "max")
            size_max = true;
        else
        {
            sizeKB_uu64 = exa::to_size_kb(value);

            if (sizeKB_uu64 == 0)
                throw CommandException(
                    "Cannot create a file system with a 0KB size.");
        }
    }

    if (opt_args.find('r') != opt_args.end())
    {
        const std::string &value(opt_args.find('r')->second);
        rg_sizeM = exa::to_size_kb(value) / 1024;
    }
}


void exa_fscreate::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Create a new file system managed by Exanodes.";
}


void exa_fscreate::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Create a new file system named " << ARG_FILESYSTEM_FSNAME <<
    " with type "
        << OPT_ARG_TYPE_FSTYPE << " and size " << OPT_ARG_SIZE_SIZE
        << " in the group " << ARG_FILESYSTEM_GROUPNAME << " of the cluster "
        << ARG_FILESYSTEM_CLUSTERNAME << "." << std::endl;
    out << "At start, this file system will be mounted on " <<
    OPT_ARG_MOUNTPOINT_PATH << "." << std::endl;
    out << OPT_ARG_SIZE_SIZE <<
    " includes space for Exanodes and file system metadata "
        << "so the available free space inside the filesystem will be a "
        << "little bit smaller." << std::endl;
}


void exa_fscreate::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Create a local 1GiB ext3 file system named " << Boldify("myfs")
        << " in the disk group " << Boldify("mygroup") << " of the cluster "
        << Boldify("mycluster") <<
    ". This file system will be mounted on /mnt/ext3 "
        << "when started with exa_fsstart:" << std::endl;
    out << "  " <<
    "exa_fscreate --mountpoint /mnt/ext3 --type ext3 --size 1G mycluster:mygroup:myfs"
   << std::endl;
    out << std::endl;

    out << "Create a 1.5GiB Seanodes distributed file system named "
        << Boldify("mysfs") << ". This file system will be mounted on"
        << " /mnt/sfs when started with exa_fsstart:" << std::endl;
    out << "  " <<
    "exa_fscreate --mountpoint /mnt/sfs --type sfs --size 1.5G mycluster:mygroup:mysfs"
   << std::endl;
    out << std::endl;
}


