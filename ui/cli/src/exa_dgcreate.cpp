/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_dgcreate.h"

#include <sys/stat.h>

#include "common/include/exa_constants.h"
#include "common/include/exa_config.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/split_node_disk.h"

#include "os/include/os_dir.h"

#define INPUT_BUFFER_SIZE 100000

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using std::string;
using std::vector;
using std::map;
using std::set;
using boost::lexical_cast;

const std::string exa_dgcreate::OPT_ARG_DISK_HOSTNAMES(Command::Boldify(
                                                           "HOSTNAMES"));
const std::string exa_dgcreate::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));
const std::string exa_dgcreate::OPT_ARG_EXTRA_DGOPTION(Command::Boldify(
                                                           "DGOPTION"));
const std::string exa_dgcreate::OPT_ARG_LAYOUT_LAYOUT(Command::Boldify("LAYOUT"));
const std::string exa_dgcreate::OPT_ARG_GROUP_FILE(Command::Boldify("FILE"));
const std::string exa_dgcreate::OPT_ARG_NBSPARE_N(Command::Boldify("N"));

const std::string exa_dgcreate::OPT_ARG_LAYOUT_SSTRIPING(Command::Boldify(
                                                             SSTRIPING_NAME));
const std::string exa_dgcreate::OPT_ARG_LAYOUT_RAINX(Command::Boldify(
                                                         RAINX_NAME));

exa_dgcreate::exa_dgcreate()
        : startgroup(false)
          , alldisks(false)
          , nb_spare(-1)
{
    add_option('i', "disk", "Specify the nodes and disks to use.", 1, false,
               true, OPT_ARG_DISK_HOSTNAMES + EXA_CONF_SEPARATOR +
               OPT_ARG_DISK_PATH);
    add_option('a', "all-unassigned", "Create the group on all free disks of "
               "the cluster.", 1, false, false);
    add_option('y', "layout", "Specify the layout to use (" +
               OPT_ARG_LAYOUT_SSTRIPING + " or " + OPT_ARG_LAYOUT_RAINX + ").",
               2, false, true, OPT_ARG_LAYOUT_LAYOUT);
    add_option('s', "start", "Start the group after its creation.", 0, false,
               false);


    std::stringstream def;
    def << VRT_DEFAULT_NB_SPARES;
    add_option('n', "nb_spare", "Specify the number of spare SPOF groups.",
               0, false, true, OPT_ARG_NBSPARE_N, def.str());

    std::set<int> group;
    group.insert(1);
    group.insert(2);
    add_option('g', "group", "Specify the disk group config file.", group,
               true, true, OPT_ARG_GROUP_FILE);

    add_option('e', "extra",
               "Specify one or more extra options for your disk group.\n"
               "- chunk_size=262144 (in KiB)\n"
               "- slot_width=3\n"
               "- su_size=1024 (in KiB)\n"
               "- dirty_zone_size=32768 (in KiB)\n"
               "- blended_stripes=0\n"
               "Specify several values by quoting them:\n"
               "-e 'chunk_size=524288 su_size=64'",
               0, true, true, OPT_ARG_EXTRA_DGOPTION);
}


void exa_dgcreate::init_see_alsos()
{
    add_see_also("exa_dgdelete");
    add_see_also("exa_dgstart");
    add_see_also("exa_dgstop");
    add_see_also("exa_dgdiskrecover");
}


void exa_dgcreate::run()
{
    string error_msg;
    xmlDocPtr configDocPtr;
    exa_error_code error_code;
    string error_message;

    if (nb_spare >= 0)
        extra_option_list.push_back("nb_spare=" +
                                    lexical_cast<std::string>(nb_spare));

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME, EXA_MAXSIZE_GROUPNAME,
                                 _group_name, false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    exa_cli_info("Creating disk group '%s:%s':\n",
                 _cluster_name.c_str(), _group_name.c_str());

    /* Get the group config file */
    if (groupconfig.empty())
        configDocPtr = create_config_from_param(_cluster_name, _group_name,
                                                extra_option_list,
                                                layout_type);
    else
        configDocPtr = create_config_from_file(groupconfig);

    if (!configDocPtr)
        throw CommandException(EXA_ERR_DEFAULT);

    if (xml_conf_xpath_singleton(configDocPtr, "//cluster"))
        throw CommandException(
            "Your disk group initialization file '" +
            groupconfig +
            "' should not contain cluster level definition");

    if (!xml_conf_xpath_singleton(configDocPtr, "//diskgroup[@name='%s']",
                                  _group_name.c_str()))
        throw CommandException(
            "The disk group '" + _cluster_name + ":" +
            _group_name +
            "' is not in your initialization file '" +
            groupconfig +
            "', or there are multiple\ndefinitions for it");

    xmlNodePtr nd(xmlNewNode(NULL, BAD_CAST("Exanodes")));
    xmlAddChild(nd, xmlCopyNode(xml_conf_xpath_singleton(configDocPtr,
                                                         "//diskgroup[@name='%s']",
                                                         _group_name.c_str()),
                                1));

    /*
     * Create DGCREATE command
     */
    AdmindCommand command_create("dgcreate", exa.get_cluster_uuid());
    command_create.add_param("config", nd);
    command_create.add_param("alldisks", alldisks);

    /* Send the command and receive the response */
    if (!send_command(command_create, "Disk group create:", error_code,
                      error_message))
        throw CommandException(error_code);

    switch (error_code)
    {
    case ADMIND_ERR_METADATA_CORRUPTION:
        exa_cli_info(
            "Please run exa_dgdelete with the --metadata-recovery option.\n");
        goto done;
        break;

    case EXA_SUCCESS:
        break;

    default:
        goto done;
    }

    /*
     * Start command
     */
    if (startgroup)
    {
        AdmindCommand command_start("dgstart", exa.get_cluster_uuid());
        command_start.add_param("groupname", _group_name);

        exa_cli_info("\nStarting disk group '%s:%s'\n",
                     _cluster_name.c_str(), _group_name.c_str());

        /* Send the command and receive the response */
        send_command(command_start, "Disk group start:", error_code,
                     error_message);
    }

done:
    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


xmlDocPtr exa_dgcreate::create_config_from_param(
    const string &clustername,
    const string &groupname,
    std::vector<std::string>
    extra_option_list,
    std::string layout_type)
{
    typedef map< string, set<string> > nodediskmap_t;
    nodediskmap_t diskmap;
    vector<string> noderdev_list;

    xmlDocPtr configDocPtr = xmlNewDoc((xmlChar *) "1.0");
    configDocPtr->children = xmlNewDocNode(configDocPtr,
                                           NULL,
                                           (xmlChar *) "Exanodes",
                                           NULL);
    xmlNodePtr group_ptr = xmlNewChild(configDocPtr->children, NULL,
                                       BAD_CAST EXA_CONF_GROUP,
                                       NULL);

    /* The diskgroup properties */
    xmlSetProp(group_ptr, BAD_CAST EXA_CONF_CLUSTER, BAD_CAST clustername.c_str());
    xmlSetProp(group_ptr,
               BAD_CAST EXA_CONF_GROUP_NAME,
               BAD_CAST groupname.c_str());
    xmlSetProp(group_ptr, BAD_CAST "layout", BAD_CAST layout_type.c_str());

    /* Add the extra options if any */
    for (vector<string>::iterator it = extra_option_list.begin();
         it != extra_option_list.end(); ++it)
    {
        string key, value;

        if (string(*it).empty())
            continue;

        if (!column_split("=", string(*it), key, value))
            throw CommandException("Option '" + string(*it) +
                                   "' must be of the form 'name=value'");

        xml_set_prop(group_ptr, key.c_str(), value.c_str());
    }

    /* The list of disks */
    xmlNodePtr physical_ptr = xmlNewChild(group_ptr, NULL,
                                          BAD_CAST EXA_CONF_PHYSICAL,
                                          NULL);

    boost::split(noderdev_list, disks, boost::algorithm::is_any_of(" "));

    for (vector<string>::const_iterator it = noderdev_list.begin();
         it != noderdev_list.end(); ++it)
    {
        string noderdev = *it;
        string nodes, disk;

        split_node_disk(noderdev, nodes, disk);

        set<string> nodelist;
        /* Check expansion are valid */
        try
        {
            nodelist = exa_expand(nodes);
        }
        catch (string msg)
        {
            throw CommandException(msg);
        }

        for (set<string>::const_iterator itn = nodelist.begin();
             itn != nodelist.end(); ++itn)
            diskmap[*itn].insert(disk);
    }

    /* Create each node entry */
    for (nodediskmap_t::iterator it = diskmap.begin();
         it != diskmap.end();
         ++it)
    {
        string node_name = it->first.c_str();

        /* disks */
        for (set<string>::iterator it_rdev = it->second.begin();
             it_rdev != it->second.end(); ++it_rdev)
        {
            string rdev_name = *it_rdev;

            if (rdev_name == "")
                break;

            xmlNodePtr disk_ptr = xmlNewChild(physical_ptr, NULL,
                                              BAD_CAST EXA_CONF_DISK,
                                              NULL);
            xmlSetProp(disk_ptr,
                       BAD_CAST EXA_CONF_NODE,
                       BAD_CAST node_name.c_str());

            xmlSetProp(disk_ptr,
                       BAD_CAST EXA_CONF_DISK_PATH,
                       BAD_CAST rdev_name.c_str());
        }
    }

    return configDocPtr;
}


xmlDocPtr exa_dgcreate::create_config_from_file(std::string groupconfig)
{
    xmlDocPtr configDocPtr;

    if (groupconfig != "-")
    {
        struct stat statbuf;

        if (stat(groupconfig.c_str(), &statbuf) ||
            (!S_ISREG(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)))
            throw CommandException("Initialization file '" + groupconfig
                                   + "' is not found");
        configDocPtr = xml_conf_init_from_file(groupconfig.c_str());
    }
    else
    {
        char in_text[INPUT_BUFFER_SIZE + 1];
        size_t length;

        length = fread(&in_text, 1, INPUT_BUFFER_SIZE, stdin);

        if (length <= 0)
            throw CommandException(
                "Failed to read the initialization from stdin");
        else if (length >= INPUT_BUFFER_SIZE)
            throw CommandException(
                "Internal error, should increase the input buffer size INPUT_BUFFER_SIZE");

        in_text[length] = '\0';
        configDocPtr = xmlReadMemory(in_text,
                                     length,
                                     NULL,
                                     NULL,
                                     XML_PARSE_NOBLANKS);
    }

    if (!configDocPtr)
        exa_cli_error(
            "%sERROR%s: Failed to parse your disk group initialization file\n",
            COLOR_ERROR,
            COLOR_NORM);

    return configDocPtr;
}


void exa_dgcreate::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_dgcommand::parse_opt_args(opt_args);

    if (opt_args.find('e') != opt_args.end())
        boost::split(extra_option_list,
                     opt_args.find('e')->second,
                     boost::algorithm::is_any_of(" "),
                     boost::token_compress_on);

    /* Don't do the exe_expand here since disks may contains / */
    if (opt_args.find('i') != opt_args.end())
        disks = opt_args.find('i')->second;

    if (opt_args.find('a') != opt_args.end())
        alldisks = true;

    if (opt_args.find('g') != opt_args.end())
    {
        if (opt_args.find('e') != opt_args.end())
            throw CommandException(
                "You cannot provide a group initialization file and extra options.");
        if (opt_args.find('n') != opt_args.end())
            throw CommandException(
                "You cannot provide a group initialization file and nb_spare option.");
        groupconfig = opt_args.find('g')->second;
    }

    if (opt_args.find('s') != opt_args.end())
        startgroup = true;

    if (opt_args.find('n') != opt_args.end())
        nb_spare = exa::to_uint32(opt_args.find('n')->second);

    if (opt_args.find('y') != opt_args.end())
        layout_type = opt_args.find('y')->second;
}


void exa_dgcreate::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Create a new Exanodes disk group.";
}


void exa_dgcreate::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Create a new disk group named " << ARG_DISKGROUP_GROUPNAME
        << " using the disks '" << OPT_ARG_DISK_HOSTNAMES
        << EXA_CONF_SEPARATOR << OPT_ARG_DISK_PATH << "...' in the cluster "
        << ARG_DISKGROUP_CLUSTERNAME << "." << std::endl;

    out << OPT_ARG_DISK_HOSTNAMES << " is a regular expansion (see exa_expand)."
        << std::endl;
    out << OPT_ARG_DISK_PATH << " is the path of the disk." << std::endl;
    out << "To specify multiple " << OPT_ARG_DISK_HOSTNAMES
        << EXA_CONF_SEPARATOR << OPT_ARG_DISK_PATH
        << ", separate them by a space and enclose the whole list with quotes."
        << std::endl;
    out << std::endl;
    out << "Supported layouts are " << OPT_ARG_LAYOUT_SSTRIPING << " and "
        << OPT_ARG_LAYOUT_RAINX
        << ". The layout defines the way a disk group organizes its data. "
        << "It has implications on performance and redundancy. " << std::endl;

    out << "The " << OPT_ARG_LAYOUT_SSTRIPING << " layout is very efficient in terms "
        << "of speed but has no data redundancy. There are no constraints and "
        << "no fault tolerance." << std::endl;

    out << "The " << OPT_ARG_LAYOUT_RAINX << " layout accepts the loss of part or "
        << "all of one SPOF group while providing full access to your storage. Moreover, "
        << "it has support for spare SPOF groups. Each spare SPOF group allows to "
        << "handle an additional SPOF group loss, provided there is sufficient time "
        << "between both failures to allow Exanodes to replicate the data."
        << std::endl;

    out << "The " << OPT_ARG_LAYOUT_RAINX << " layout requires at least 3 disks "
        << "in 3 different SPOF groups." << std::endl;

    out << "The maximum number of spare SPOF groups obeys the following rules:"
        << std::endl;
    out << " - Number of disks / max number of disks in a node >= 2 + number "
        << "of spare SPOF groups." << std::endl;
    out << " - Number of disks > 2 * (number of spare SPOF groups + 1) * max number "
        << "of disks in a node." << std::endl;
    out << " - No more than 16 spare SPOF groups." << std::endl;
}


void exa_dgcreate::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Create a new disk group named " << Boldify("mygroup")
        << " with the layout " << OPT_ARG_LAYOUT_SSTRIPING
        << " that uses disk " << Boldify("/dev/sdb") << " (on Linux) or "
        << Boldify("E:") << " (on Windows)" << " on nodes "
        << Boldify("node1") << ", " << Boldify( "node2") << " and " << Boldify("node3")
        << " in the cluster " << Boldify("mycluster") << ":" << std::endl;
    out << std::endl;
    out << "    Linux: exa_dgcreate --disk node/1-3/" << EXA_CONF_SEPARATOR
        << "/dev/sdb --layout sstriping mycluster:mygroup" << std::endl;
    out << "  Windows: exa_dgcreate --disk node/1-3/" << EXA_CONF_SEPARATOR
        << "E: --layout sstriping mycluster:mygroup" << std::endl;
    out << std::endl;

    out << "Create a new disk group named " << Boldify("mygroup")
        << " with the layout " << OPT_ARG_LAYOUT_RAINX
        << " that uses disk " << Boldify("/dev/sdb") << " (on Linux) or "
        << Boldify("E:") << " (on Windows)" << " on nodes "
        << Boldify("node1") << " to " << Boldify("node10")
        << " in the cluster " << Boldify("mycluster")
        << ", using 3 spare SPOF groups (the maximum allowed in this configuration):"
        << std::endl;
    out << std::endl;
    out << "    Linux: exa_dgcreate --disk node/1-10/" << EXA_CONF_SEPARATOR
        << "/dev/sdb --layout rainX --nb_spare 3 mycluster:mygroup" << std::endl;
    out << "  Windows: exa_dgcreate --disk node/1-10/" << EXA_CONF_SEPARATOR
        << "E: --layout rainX --nb_spare 3 mycluster:mygroup" << std::endl;
    out << std::endl;

    if (show_hidden == false)
        return;

    out << "Create the new disk group " << Boldify("mygroup")
        << " in the cluster " << Boldify("mycluster")
        << " using the initialization file " << Boldify("mygroup.conf") <<
    ":" << std::endl;
    out << std::endl;
    out << "  " << "exa_dgcreate --group mygroup.conf mycluster:mygroup" <<
    std::endl;
    out << std::endl;
}


