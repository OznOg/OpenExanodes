/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_vlcreate.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>
#include <errno.h>

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"

#include "os/include/os_getopt.h"

using std::string;

const std::string exa_vlcreate::OPT_ARG_SIZE_SIZE(Command::Boldify("SIZE"));
const std::string exa_vlcreate::OPT_ARG_EXPORT_METHOD(Command::Boldify(
                                                          "EXPORT_METHOD"));
const std::string exa_vlcreate::OPT_ARG_ACCESS_MODE(Command::Boldify("MODE"));
const std::string exa_vlcreate::OPT_ARG_LUN(Command::Boldify("LUN"));
const std::string exa_vlcreate::OPT_ARG_READAHEAD_SIZE(Command::Boldify("SIZE"));

exa_vlcreate::exa_vlcreate(int argc, char *argv[])
    : exa_vlcommand(argc, argv)
    , is_private(false)
    , export_method("")
    , sizeKB_uu64(0)
    , size_max(false)
    , lun(-1)
    , readahead(-1)
{}


exa_vlcreate::~exa_vlcreate()
{}

void exa_vlcreate::init_options()
{
    exa_vlcommand::init_options();

#ifdef WITH_BDEV
    add_option('x', "export-method", "Specify the method (bdev or iSCSI) "
               "through which to export this volume.", 2, false, true,
                OPT_ARG_EXPORT_METHOD);

    add_option('r', "readahead", "Define the readahead size for the bdev "
               "volume. The readahead size must be ended with a unit symbol "
               "like the --size option. This setting is persistent. You can "
               "display it or change it later with the command exa_vltune.",
               0, false, true, OPT_ARG_READAHEAD_SIZE);
    add_option('a', "access", "If " +  OPT_ARG_ACCESS_MODE +
               "=shared, the bdev volume can be accessed from several nodes "
               "simultaneously. This is the default value. If " +
               OPT_ARG_ACCESS_MODE + "=private, the volume is private to a "
               "node: it cannot be started from several nodes simultaneously.",
               0, false, true, OPT_ARG_ACCESS_MODE);

    add_option('p', "forceprivate", "Equivalent to --access=private.", 0,
               false, false);
#endif

    add_option('s', "size", "The size must be specified with a unit symbol "
               "like in 10G. The unit can be one char of K, M, G, T, P, E (For "
               "Kibi, Mebi, Gibi, Tebi, Pebi, Exbi). The decimal point is "
               "accepted like in 1.2T. The special value 'max' means all "
               "available space in the disk group.", 1, false, true,
               OPT_ARG_SIZE_SIZE);

    add_option('L', "lun", "Specify the logical unit number (LUN) for the "
               "iSCSI volume.", 0, false, true, OPT_ARG_LUN);
}


void exa_vlcreate::init_see_alsos()
{
    add_see_also("exa_vldelete");
    add_see_also("exa_vlresize");
    add_see_also("exa_vlstart");
    add_see_also("exa_vlstop");
    add_see_also("exa_vltune");
}


void exa_vlcreate::run()
{
    string error_msg;
    string readahead_str;
    string sizeKB;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    sizeKB = boost::lexical_cast<std::string>(sizeKB_uu64);
    readahead_str = boost::lexical_cast<std::string>(readahead);

    exa_cli_trace("group=%s\n", _group_name.c_str());
    exa_cli_trace("volume=%s\n", _volume_name.c_str());
    exa_cli_trace("is_private=%s\n", is_private ? "private" : "shared");
    exa_cli_trace("sizeKB=%s\n", sizeKB.c_str());

    if (lun >= 0)
        exa_cli_trace("lun=%d\n", lun);

    if (readahead >= 0)
        exa_cli_trace("readahead=%s\n", readahead_str.c_str());

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME,
                                 EXA_MAXSIZE_VOLUMENAME,
                                 _volume_name, false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (export_method.empty())
        throw CommandException(EXA_ERR_DEFAULT);

    /*
     * Create command
     */
    AdmindCommand command("vlcreate", exa.get_cluster_uuid());
    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);
    command.add_param("export_type", export_method);
    command.add_param("private", is_private);
    command.add_param("size", sizeKB);

    if (lun >= 0)
        command.add_param("lun", lun);

    if (readahead >= 0)
        command.add_param("readahead", readahead_str);

    if (size_max)
        exa_cli_info(
            "Creating a volume '%s:%s' with all available space in group for cluster '%s'\n",
            _group_name.c_str(),
            _volume_name.c_str(),
            exa.get_cluster().c_str());
    else
    {
        char size_str[EXA_MAXSIZE_LINE + 1];
        exa::to_human_size(size_str, EXA_MAXSIZE_LINE + 1, sizeKB_uu64);
        exa_cli_info("Creating a %s volume '%s:%s' for cluster '%s'\n",
                     size_str,
                     _group_name.c_str(),
                     _volume_name.c_str(),
                     exa.get_cluster().c_str());
    }

    string msg_str = "Creating volume '" + _group_name + ":" + _volume_name +
                     "':";

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, msg_str, error_code, error_message);

    switch (error_code)
    {
    case VRT_ERR_GROUP_NOT_STARTED:
        throw CommandException(
            "The disk group is not started. Please use exa_dgstart first.",
            error_code);
        break;

    case ADMIND_ERR_METADATA_CORRUPTION:
        throw CommandException("Please run exa_vldelete --metadata-recovery.",
                               error_code);
        break;

    default:
        break;
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_vlcreate::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

#ifdef WITH_BDEV
    if (opt_args.find('x') != opt_args.end())
    {
        export_method = opt_args.find('x')->second;
        boost::algorithm::to_lower(export_method);

        if (export_method != "bdev" && export_method != "iscsi")
            throw CommandException(
                "Invalid export method, must be 'bdev' or 'iSCSI'");
    }

    if (opt_args.find('a') != opt_args.end())
    {
        if (export_method != "bdev")
            throw CommandException(
                "Option -a (accessmode) is reserved to export method 'bdev'");

        if (opt_args.find('p') != opt_args.end())
            throw CommandException("Options -a and -p are mutually exclusive");

        exa_cli_trace("Option -a [%s]\n", optarg);
        std::string accessmode = opt_args.find('a')->second;
        boost::algorithm::to_upper(accessmode);

        if (accessmode == ADMIND_PROP_SHARED)
            is_private = false;
        else if (accessmode == ADMIND_PROP_PRIVATE)
            is_private = true;
        else
            throw CommandException(
                "The access parameter must be 'shared' or 'private'");
    }

    if (opt_args.find('p') != opt_args.end())
    {
        if (export_method != "bdev")
            throw CommandException(
                "Option -p (forceprivate) is reserved to export method 'bdev'");

        if (opt_args.find('a') != opt_args.end())
            throw CommandException("Options -a and -p are mutually exclusive");

        is_private = true;
    }

    if (opt_args.find('r') != opt_args.end())
    {
        if (export_method != "bdev")
            throw CommandException(
                "Option -r (readahead) is reserved to export method 'bdev'");

        const std::string &value(opt_args.find('r')->second);
        readahead = exa::to_size_kb(value);
    }
#else
    export_method = "iscsi";
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
                throw CommandException(
                    "Cannot create a volume with a 0KB size.");
        }
    }

    if (opt_args.find('L') != opt_args.end())
    {
        if (export_method != "iscsi")
            throw CommandException(
                "Option -L (lun) is reserved to export method 'iSCSI'");

        const std::string &value(opt_args.find('L')->second);
        try
        {
            lun = exa::to_int32(value);
            if (lun < 0)
                throw CommandException("Lun value must be positive.");
        }
        catch (string msg)
        {
            throw CommandException(msg);
        }
    }
}


void exa_vlcreate::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Create a new Exanodes volume.";
}


void exa_vlcreate::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Create a new volume named " << ARG_VOLUME_VOLUMENAME
        << " with size " << OPT_ARG_SIZE_SIZE << " in the disk group " <<
    ARG_VOLUME_GROUPNAME
        << " of the cluster " << ARG_VOLUME_CLUSTERNAME << std::endl;
}


void exa_vlcreate::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Create a 50 GiB volume named " << Boldify("myvolume")
        << " in the group " << Boldify("mygroup") << " of the cluster "
        << Boldify("mycluster") << ", exported as an iSCSI target:" <<
    std::endl;
#ifdef WITH_BDEV
    out << "  " <<
    "exa_vlcreate --export-method iSCSI --size 50G mycluster:mygroup:myvolume"
   << std::endl;
#else
    out << "  " << "exa_vlcreate --size 50G mycluster:mygroup:myvolume" <<
    std::endl;
#endif
    out << std::endl;
}


