/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_cldiskadd.h"

#include "common/include/exa_config.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/split_node_disk.h"

using std::string;

const std::string exa_cldiskadd::OPT_ARG_DISK_HOSTNAME(Command::Boldify(
                                                           "HOSTNAME"));
const std::string exa_cldiskadd::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));

void exa_cldiskadd::init_options()
{
    exa_clcommand::init_options();

    add_option('i', "disk", "Specify disk to add.", 1, false, true,
               OPT_ARG_DISK_HOSTNAME + EXA_CONF_SEPARATOR + OPT_ARG_DISK_PATH);
}


void exa_cldiskadd::init_see_alsos()
{
    add_see_also("exa_cldiskdel");
}


void exa_cldiskadd::run()
{
    string error_msg;
    exa_error_code error_code;

    string msg_str;
    string diskpath;
    string nodename;

    if (set_cluster_from_cache(_cluster_name.c_str(), error_msg) != EXA_SUCCESS)
        throw CommandException("Can't retrieve cluster informations",
                               EXA_ERR_DEFAULT);

    split_node_disk(_disk, nodename, diskpath);
    if (diskpath.empty())
        throw CommandException("This command requires a disk path, "
                               "check command usage with --help.",
                               EXA_ERR_INVALID_PARAM);

    msg_str = "Adding disk '" + nodename + EXA_CONF_SEPARATOR + diskpath +
              "' to cluster '" + exa.get_cluster() + "'";

    AdmindCommand command("cldiskadd", exa.get_cluster_uuid());
    command.add_param("node_name", nodename);
    command.add_param("disk_path", diskpath);

    if (!send_command(command, msg_str, error_code, error_msg))
        throw CommandException("Failed to receive the response from admind.");

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_cldiskadd::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('i') != opt_args.end())
        _disk = opt_args.find('i')->second;
}


void exa_cldiskadd::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Add a disk to an Exanodes cluster.";
}


void exa_cldiskadd::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Add the disk " << OPT_ARG_DISK_PATH << " of the node " <<
    OPT_ARG_DISK_HOSTNAME
        << " to the cluster " << ARG_CLUSTERNAME << std::endl << std::endl;
    out << OPT_ARG_DISK_PATH << " is the path of the disk." << std::endl <<
    std::endl;

    out << Boldify("CAUTION! The disks will be erased.") << std::endl;
}


void exa_cldiskadd::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Add disk " << Boldify("/dev/sdb") << " (on Linux) or " << Boldify(
        "E:") << " (on Windows)"
              << " on the node " << Boldify("node1") << ":" << std::endl;
    out << std::endl;
    out << "    Linux: exa_cldiskadd --disk node1" << EXA_CONF_SEPARATOR <<
    "/dev/sdb mycluster" << std::endl;
    out << "  Windows: exa_cldiskadd --disk node1" << EXA_CONF_SEPARATOR <<
    "E: mycluster" << std::endl;
    out << std::endl;
}


