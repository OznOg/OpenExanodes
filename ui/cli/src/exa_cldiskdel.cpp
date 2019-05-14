/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_cldiskdel.h"

#include "common/include/exa_config.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/split_node_disk.h"

using std::string;

const std::string exa_cldiskdel::OPT_ARG_DISK_HOSTNAME(Command::Boldify(
                                                           "HOSTNAME"));
const std::string exa_cldiskdel::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));
const std::string exa_cldiskdel::OPT_ARG_DISK_UUID(Command::Boldify("UUID"));

exa_cldiskdel::exa_cldiskdel()
{
    add_option('i', "disk", "The node and the path of the disk to remove.",
               1, false, true, OPT_ARG_DISK_HOSTNAME + EXA_CONF_SEPARATOR +
               OPT_ARG_DISK_PATH);
    add_option('u', "uuid", "The uuid of the disk to remove.", 1, false, true,
               OPT_ARG_DISK_UUID);

    add_see_also("exa_cldiskadd");
}


void exa_cldiskdel::run()
{
    string error_msg;
    exa_error_code error_code;
    string msg_str;
    string diskpath;
    string nodename;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (!_disk.empty())
    {
        split_node_disk(_disk, nodename, diskpath);
        if (diskpath.empty())
            throw CommandException("This command requires a disk path, "
                                   "check command usage with --help.");
    }

    AdmindCommand command("cldiskdel", exa.get_cluster_uuid());
    if (!_disk.empty())
    {
        msg_str = "Removing disk '" + nodename + EXA_CONF_SEPARATOR +
                  diskpath + "' from cluster '" + exa.get_cluster() + "'";
        command.add_param("node_name",  nodename);
        command.add_param("disk_path",  diskpath);
    }
    if (!_uuid.empty())
    {
        msg_str = "Removing disk '" + _uuid + "' from cluster '" +
                  exa.get_cluster() + "'";
        command.add_param("uuid",  _uuid);
    }

    if (!send_command(command, msg_str, error_code, error_msg))
        throw CommandException("Failed to receive the response from admind.");

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_cldiskdel::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('i') != opt_args.end())
        _disk = opt_args.find('i')->second;
    if (opt_args.find('u') != opt_args.end())
        _uuid = opt_args.find('u')->second;
}


void exa_cldiskdel::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Remove a disk from a cluster.";
}


void exa_cldiskdel::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Remove the disk " << OPT_ARG_DISK_PATH << " or " <<
    OPT_ARG_DISK_UUID
        << " from the cluster " << ARG_CLUSTERNAME << "." << std::endl <<
    std::endl;
    out << OPT_ARG_DISK_PATH << " is the path of the disk." << std::endl <<
    std::endl;
    out << "The " << OPT_ARG_DISK_UUID
        << " of the disk is printed by exa_clinfo when the path is unknown." <<
    std::endl << std::endl;
}


void exa_cldiskdel::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Remove disk " << Boldify("/dev/sdb") << " (on Linux) or " <<
    Boldify("E:") << " (on Windows)"
                  << " on node " << Boldify("node1") << " from cluster " <<
    Boldify(
        "mycluster") << ":" << std::endl;
    out << std::endl;
    out << "    Linux: exa_cldiskdel --disk node1" << EXA_CONF_SEPARATOR <<
    "/dev/sdb mycluster" << std::endl;
    out << "  Windows: exa_cldiskdel --disk node1" << EXA_CONF_SEPARATOR <<
    "E: mycluster" << std::endl;
    out << std::endl;

    out << "Remove the disk " << Boldify("0EF5A7D2:1312AF25:0963C4AD:3E0D5669")
        << " from the cluster " << Boldify("mycluster")
        << " (works the same for a Linux or a Windows cluster):" << std::endl;
    out << std::endl;
    out <<
    "  exa_cldiskdel --uuid 0EF5A7D2:1312AF25:0963C4AD:3E0D5669 mycluster" <<
    std::endl;
    out << std::endl;
}


