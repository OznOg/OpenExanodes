/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_makeconfig.h"

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/config_check.h"

#include "os/include/os_network.h"

#include "boost/lexical_cast.hpp"
#include <boost/algorithm/string.hpp>

using boost::lexical_cast;
using std::string;
using std::vector;

/* Some default values */
#define DEFAULT_NUMBER_OF_GROUP     1

const std::string exa_makeconfig::OPT_ARG_CLUSTER_CLUSTERNAME(Command::Boldify(
                                                                  "CLUSTERNAME"));
const std::string exa_makeconfig::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                             "HOSTNAMES"));
const std::string exa_makeconfig::OPT_ARG_DISKGROUP_DGNAME(Command::Boldify(
                                                               "DGNAME"));
const std::string exa_makeconfig::OPT_ARG_GROUP_N(Command::Boldify("N"));
const std::string exa_makeconfig::OPT_ARG_GROUP_M(Command::Boldify("M"));
const std::string exa_makeconfig::OPT_ARG_DISK_PATHS(Command::Boldify("PATHS"));
const std::string exa_makeconfig::OPT_ARG_LAYOUT_LAYOUT(Command::Boldify(
                                                            "LAYOUT"));
const std::string exa_makeconfig::OPT_ARG_LAYOUT_SSTRIPING(Command::Boldify(
                                                               SSTRIPING_NAME));
const std::string exa_makeconfig::OPT_ARG_LAYOUT_RAINX(Command::Boldify(
                                                           RAINX_NAME));

const std::string exa_makeconfig::OPT_ARG_EXTRA_DGOPTION(Command::Boldify(
                                                             "DGOPTION"));

exa_makeconfig::exa_makeconfig()
    : want_group(false)
    , group_id(DEFAULT_NUMBER_OF_GROUP)
    , number_of_group(DEFAULT_NUMBER_OF_GROUP)
{
    add_option('c', "cluster", "Name of the cluster to create.",
               1, false, true, OPT_ARG_CLUSTER_CLUSTERNAME);

    add_option('n', "node", "Specify the nodes to be part of the cluster. This "
               "option is a regular expansion (see exa_expand).", 2, false,
               true, OPT_ARG_NODE_HOSTNAMES);

    add_option('i', "disk", "Specify the disks to use for Exanodes. No default "
               "is provided. All disks will be assigned to each nodes. If it "
               "is not the case, you can run this command and edit the created "
               "configuration manually. You can provide a list of disks by "
               "quoting them.", 3, false, true, OPT_ARG_DISK_PATHS);

    add_option('g', "diskgroup", "Name of the disk group to create. The disk "
               "group name is indexed with the group index to create when "
               "option --group is used and there is more than one group.",
               0, false, true, OPT_ARG_DISKGROUP_DGNAME);

    add_option('G', "group", "The number of groups to create.\n"
               " N = Index of the group to create\n"
               " M = The total number of groups to create.",
               0, false, true, OPT_ARG_GROUP_N + "/" + OPT_ARG_GROUP_M);

    add_option('y', "layout", "Specify the layout to use (" +
               OPT_ARG_LAYOUT_SSTRIPING + " or " + OPT_ARG_LAYOUT_RAINX + ").",
               0, false, true, OPT_ARG_LAYOUT_LAYOUT);

    add_option('e', "extra", "Specify one or more extra options for your disk "
               "group. For the rainX layout, you can specify " +
               OPT_ARG_GROUP_N + ", the number of spare disk:\n"
               " - nb_spare=" + OPT_ARG_GROUP_N + " (default is 0)\n"
               " - chunk_size=262144 (in KiB)\n"
               " - slot_width=3\n"
               " - su_size=1024 (in KB)\n"
               " - dirty_zone_size=32768 (in KB)\n"
               " - blended_stripes=0", 0, true, true, OPT_ARG_EXTRA_DGOPTION);

    add_see_also("exa_expand");
    add_see_also("exa_clcreate");
    add_see_also("exa_dgcreate");
}


void exa_makeconfig::dump_synopsis(std::ostream &out, bool show_hidden) const
{
    out << "Create a cluster configuration file:" << std::endl << std::endl;
    out << "exa_makeconfig --cluster " << OPT_ARG_CLUSTER_CLUSTERNAME
        << " --node " << OPT_ARG_NODE_HOSTNAMES << " --disk " <<
    OPT_ARG_DISK_PATHS
        << std::endl;
    out << std::endl;
    out << "Create a disk group configuration file:" << std::endl << std::endl;
    out << "exa_makeconfig --cluster  " << OPT_ARG_CLUSTER_CLUSTERNAME
        << " --node " << OPT_ARG_NODE_HOSTNAMES << " --disk " <<
    OPT_ARG_DISK_PATHS
        << " --layout " << OPT_ARG_LAYOUT_LAYOUT << " --diskgroup " <<
    OPT_ARG_DISKGROUP_DGNAME
        << " --group " << OPT_ARG_GROUP_N << "/" << OPT_ARG_GROUP_M
        << " [--extra '" + Boldify("EXTRA_OPTIONS") + "']" << std::endl;
}


void exa_makeconfig::run()
{
    xmlDocPtr doc;

    vector<string> rdev_list;

    doc = xml_new_doc("1.0");
    doc->children = xml_new_doc_node(doc, NULL, "Exanodes", NULL);

    if (nodes.empty())
        throw CommandException(
            "You must provide the nodes for your cluster using '-n' or '--node'.\n"
            "       Check command usage with --help.");

    std::set<std::string> nodelist;
    boost::trim(disks);
    if (!disks.empty())
        boost::split(rdev_list, disks, boost::algorithm::is_any_of(
                         " "), boost::token_compress_on);

    /* Check expansion are valid */
    try
    {
        nodelist = exa_expand(nodes);
    }
    catch (string msg)
    {
        throw CommandException(msg);
    }

    /*
     * Now ready to start
     */

    if (!want_group)
    {
        xmlNodePtr xmlnode_cluster = xml_new_child(doc->children,
                                                   NULL,
                                                   EXA_CONF_CLUSTER,
                                                   NULL);
        xml_set_prop(xmlnode_cluster, "name", cluster_name.c_str());

        /* Create each node entry */
        for (std::set<std::string>::const_iterator it = nodelist.begin();
             it != nodelist.end(); ++it)
        {
            string node_name = *it;
            xmlNodePtr xmlnode = xml_new_child(xmlnode_cluster,
                                               NULL,
                                               "node",
                                               NULL);
            char canonical_hostname[EXA_MAXSIZE_HOSTNAME + 1];

            xml_set_prop(xmlnode, "name", node_name.c_str());

            if (os_host_canonical_name(node_name.c_str(), canonical_hostname,
                                       sizeof(canonical_hostname)) != 0)
                throw ConfigException(
                    "Couldn't get canonical name of '" + node_name + "'");
            xml_set_prop(xmlnode, EXA_CONF_NODE_HOSTNAME, canonical_hostname);

            ConfigCheck::insert_node_network(xmlnode, "");

            /* disks */
            for (vector<string>::iterator it_rdev = rdev_list.begin();
                 it_rdev != rdev_list.end(); ++it_rdev)
            {
                string rdev_path = *it_rdev;
                xmlNodePtr xmlrdev = xml_new_child(xmlnode, NULL, "disk", NULL);

                xml_set_prop(xmlrdev, "path",  rdev_path.c_str());
            }
        }
    }
    else                        /* DISK GROUP CREATION */
    {
        vector<string> disk_list;
        vector<string> layout_stl;

        if (layout_type.empty())
        {
            exa_cli_error(
                "%sERROR%s: Please provide a layout type using the '-y' option.\n"
                "       Supported layouts are: %s\n",
                COLOR_ERROR,
                COLOR_NORM,
                EXA_LAYOUT_TYPES);
            throw CommandException(EXA_ERR_DEFAULT);
        }

        /* Check layout is provided and is one we support */
        boost::split(layout_stl, EXA_LAYOUT_TYPES,
                     boost::algorithm::is_any_of(","));
        if (std::find(layout_stl.begin(), layout_stl.end(),
                      layout_type) == layout_stl.end())
        {
            exa_cli_error(
                "%sERROR%s: The layout '%s' is not supported. Supported layouts are:\n%s\n",
                COLOR_ERROR,
                COLOR_NORM,
                layout_type.c_str(),
                EXA_LAYOUT_TYPES);

            throw CommandException(EXA_ERR_DEFAULT);
        }

        /*
         * Create a big list with all node/disk combination
         * Form of the list is node, dev1, node, dev2, node2, dev1, ...
         */
        for (vector<string>::iterator it_rdev = rdev_list.begin();
             it_rdev != rdev_list.end(); ++it_rdev)
        {
            for (std::set<std::string>::const_iterator it = nodelist.begin();
                 it != nodelist.end(); ++it)
            {
                string rdev_path = *it_rdev;

                disk_list.push_back(*it);
                disk_list.push_back(rdev_path.c_str());
            }
        }

        if (disk_list.size() / 2 < number_of_group)
        {
            exa_cli_error(
                "%sERROR%s: You requested '%d' groups but there is only '%"
                PRIzu "' disks. Specify disks with the -i option\n",
                COLOR_ERROR,
                COLOR_NORM,
                number_of_group,
                disk_list.size() / 2);
            throw CommandException(EXA_ERR_DEFAULT);
        }

        /* Warning, the list contains 2 items by device */
        int start_index =
            (disk_list.size() / 2 / number_of_group) * (group_id - 1);
        int stop_index;

        stop_index = (disk_list.size() / 2 / number_of_group) * group_id;

        /* We loose device in the rounding, put all remaining devices in the last group */
        if (group_id == number_of_group)
            stop_index = disk_list.size() / 2;

        xmlNodePtr xmlnode_dg = xml_new_child(doc->children,
                                              NULL,
                                              "diskgroup",
                                              NULL);
        string this_group_name;

        /* Add the extra options if any */
        for (vector<string>::iterator it = extra_option_list.begin();
             it != extra_option_list.end(); ++it)
        {
            string key, value;

            if (!column_split("=", string(*it), key, value))
                throw CommandException("Extra option '" + key +
                                       "' must be of the form 'name=value'");

            xml_set_prop(xmlnode_dg, key.c_str(), value.c_str());
        }

        /* In case there is a single group to create, do not suffix it with a number */
        if (number_of_group == 1)
            this_group_name = group_name;
        else
            this_group_name = group_name + lexical_cast<std::string>(group_id);

        xml_set_prop(xmlnode_dg, "name",        this_group_name.c_str());
        xml_set_prop(xmlnode_dg, "cluster",     cluster_name.c_str());
        xml_set_prop(xmlnode_dg, "layout",      layout_type.c_str());

        xmlNodePtr xmlnode_physical = xml_new_child(xmlnode_dg,
                                                    NULL,
                                                    "physical",
                                                    NULL);

        for (int i = start_index; i < stop_index; i++)
        {
            xmlNodePtr xmldev = xml_new_child(xmlnode_physical,
                                              NULL,
                                              "disk",
                                              NULL);
            xml_set_prop(xmldev, "node",  disk_list[i * 2].c_str());
            xml_set_prop(xmldev, "path",  disk_list[i * 2 + 1].c_str());
        }
    }

    /*
     * Dump the resulting tree to stdout
     */
    xmlChar *cmd;
    int size;
    xmlDocDumpFormatMemory(doc, &cmd, &size, 1);
    printf("%s\n", cmd);
}


void exa_makeconfig::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    if (opt_args.find('c') != opt_args.end())
        cluster_name = opt_args.find('c')->second;

    if (opt_args.find('e') != opt_args.end())
        boost::split(extra_option_list, opt_args.find(
                         'e')->second, boost::algorithm::is_any_of(" "));

    if (opt_args.find('n') != opt_args.end())
        nodes = opt_args.find('n')->second;

    if (opt_args.find('i') != opt_args.end())
        disks = opt_args.find('i')->second;

    if (opt_args.find('G') != opt_args.end())
    {
        string sgroup_id, snumber_of_group;
        want_group = true;

        if (!column_split("/", opt_args.find('G')->second, sgroup_id,
                          snumber_of_group))
            throw CommandException(
                "The --group option requires a value of the form I/M (e.g. 2/4)");

        if (to_uint(sgroup_id.c_str(), &group_id) != EXA_SUCCESS)
            throw CommandException("Invalid group number N=" + sgroup_id);

        if (to_uint(snumber_of_group.c_str(), &number_of_group) != EXA_SUCCESS)
            throw CommandException("Invalid group number M=" + snumber_of_group);

        if (group_id > number_of_group)
            throw CommandException(
                "The group index to create '" + sgroup_id +
                "' is above the number of group '" +
                snumber_of_group + "'");
    }

    if (opt_args.find('y') != opt_args.end())
    {
        if (opt_args.find('G') == opt_args.end())
            throw CommandException(
                "Option --layout is only allowed in a diskgroup config creation context");
        layout_type = opt_args.find('y')->second;
    }

    if (opt_args.find('g') != opt_args.end())
    {
        if (opt_args.find('G') == opt_args.end())
            throw CommandException(
                "Option --group is only allowed in a diskgroup config creation context");
        group_name = opt_args.find('g')->second;
    }
}


void exa_makeconfig::dump_short_description(std::ostream &out,
                                            bool show_hidden) const
{
    out <<
    "Create a cluster or a disk group initialization file for Exanodes." <<
    std::endl;
    out << std::endl;
    out << Boldify(
        "CAUTION! This command is deprecated. It is recommended to use exa_clcreate and exa_dgcreate directly.")
        << std::endl;
}


void exa_makeconfig::dump_full_description(std::ostream &out,
                                           bool show_hidden) const
{
    out <<
    "The first synopsis creates a configuration file for a cluster named "
        << OPT_ARG_CLUSTER_CLUSTERNAME << " containing the nodes " <<
    OPT_ARG_NODE_HOSTNAMES
        << " and the disks " << OPT_ARG_DISK_PATHS <<
    " on each of these nodes." << std::endl;
    out << std::endl;
    out <<
    "The second synopsis creates a configuration file for a disk group named "
        << OPT_ARG_DISKGROUP_DGNAME << " in the cluster " <<
    OPT_ARG_CLUSTER_CLUSTERNAME << "."
                                << OPT_ARG_NODE_HOSTNAMES << " and " <<
    OPT_ARG_DISK_PATHS
                                <<
    " parameters should be exactly the same that the ones used to create the cluster "
        << OPT_ARG_CLUSTER_CLUSTERNAME <<
    ". The command will select the disks to create the "
        << OPT_ARG_GROUP_N << "th disk group of " << OPT_ARG_GROUP_M <<
    " groups. The different "
        << "groups have to be created with the same value of " <<
    OPT_ARG_GROUP_M << std::endl;
}


void exa_makeconfig::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Create a cluster named " << Boldify("mycluster") <<
    " with 10 nodes " << Boldify("node01")
                      << " to " << Boldify("node10") << " and 2 disks " <<
    Boldify("/dev/hdb")
                      << " and " << Boldify("/dev/hdc on each node:") <<
    std::endl <<
    std::endl;
    out << "  " <<
    "exa_makeconfig --cluster mycluster --node node/01-10/ --disk '/dev/hdb /dev/hdc'"
        << std::endl;
    out << std::endl;
    out << "Create the 2nd disk group of 4 for this cluster:" << std::endl;
    out << "  " <<
    "exa_makeconfig --cluster mycluster --node node/01-10/ --disk"
        << " '/dev/hdb /dev/hdc' --layout rainX --group 2/4" << std::endl;
    out << std::endl;
}


