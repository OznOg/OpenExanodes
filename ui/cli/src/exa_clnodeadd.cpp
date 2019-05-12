/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clnodeadd.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/config_check.h"

#include "os/include/os_network.h"

#include "boost/lexical_cast.hpp"
#include <boost/algorithm/string.hpp>
#include <fstream>

using boost::lexical_cast;
using std::set;

using std::shared_ptr;
using std::string;

const std::string exa_clnodeadd::OPT_ARG_CONFIG_FILE(Command::Boldify("FILE"));
const std::string exa_clnodeadd::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));
const std::string exa_clnodeadd::OPT_ARG_NODE_HOSTNAME(Command::Boldify(
                                                           "HOSTNAME"));
const std::string exa_clnodeadd::OPT_ARG_DATANETWORK_HOSTNAME(Command::Boldify("HOSTNAME"));

#define ALL_DISKS_OF_NODE "all"
#define ANY_DISKS_OF_NODE "any"

/** Describes the disk set of a node */
typedef struct
{
    bool all;                   /**< all disks are available */
    std::set<string> set;       /**< set of disks */
}diskset_t;

exa_clnodeadd::exa_clnodeadd()
    : _xml_cfg(NULL)
    , _node_name("")
    , _disks("")
    , _datanetwork("")
{}


void exa_clnodeadd::init_options()
{
    exa_clcommand::init_options();

    add_option('c', "config", "Specify the initialization file.", 1, true, true,
               OPT_ARG_CONFIG_FILE);
    add_option('n', "node", "Specify the hostname of the node to add.", 1,
               false, true, OPT_ARG_NODE_HOSTNAME);

    add_option('i', "disk", "Specify the disks to use.", 0, false, true,
               "'" + OPT_ARG_DISK_PATH + "...'");
    add_option('D', "datanetwork", "Specify the hostname or IP address to use "
               "for the data network.", 0,
               false, true, "'" + OPT_ARG_DATANETWORK_HOSTNAME + "'");
}


void exa_clnodeadd::init_see_alsos()
{
    add_see_also("exa_clnodedel");
    add_see_also("exa_clnodestart");
    add_see_also("exa_clnodestop");
    add_see_also("exa_clnoderecover");
}


exa_clnodeadd::~exa_clnodeadd()
{
    if (_xml_cfg)
        xmlFreeDoc(_xml_cfg);
}


void exa_clnodeadd::run()
{
    string error_msg;
    exa_error_code error_code;

    string msg_str;

    if (set_cluster_from_cache(_cluster_name.c_str(), error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (!_xml_cfg)
    {
        xmlNodePtr xmlcluster;
        xmlNodePtr xmlnode;
        char canonical_hostname[EXA_MAXSIZE_HOSTNAME + 1];

        /* Create the config file based on user input */
        _xml_cfg = xml_new_doc("1.0");
        _xml_cfg->children = xml_new_doc_node(_xml_cfg, NULL, "Exanodes", NULL);

        /* Add node entry */
        xmlcluster = xml_new_child(_xml_cfg->children,
                                   NULL,
                                   EXA_CONF_CLUSTER,
                                   NULL);
        xml_set_prop(xmlcluster, "name", _cluster_name.c_str());

        xmlnode = xml_new_child(xmlcluster, NULL, "node", NULL);

        xml_set_prop(xmlnode, "name", _node_name.c_str());

        if (os_host_canonical_name(_node_name.c_str(), canonical_hostname,
                                   sizeof(canonical_hostname)) != 0)
            throw ConfigException(
                "Couldn't get canonical name of '" + _node_name + "'");
        xml_set_prop(xmlnode, EXA_CONF_NODE_HOSTNAME, canonical_hostname);

        ConfigCheck::insert_node_network(xmlnode, _datanetwork);

        /* Transform the "disk" raw argument into a set of disks */
        diskset_t diskset_arg;

        if (!_disks.empty())
            boost::split(diskset_arg.set, _disks,
                         boost::algorithm::is_any_of(" "));

        if (diskset_arg.set.find(ALL_DISKS_OF_NODE) != diskset_arg.set.end())
        {
            diskset_arg.set.clear();
            diskset_arg.all = true;
        }
        else
            diskset_arg.all = false;

        /* Build the set of disks available retrieved from the node itself */
        AdmindCommand command_getdisks("get_nodedisks", exa_uuid_zero);
        shared_ptr<AdmindMessage> result_getdisks;
        diskset_t diskset_conf;         /* Set of disks available on the node */

        result_getdisks = send_admind_to_node(xml_get_prop(xmlnode,
                                                           "hostname"),
                                              command_getdisks, error_code);

        switch (error_code)
        {
        case EXA_SUCCESS:
            if (result_getdisks->get_payload() == ANY_DISKS_OF_NODE)
                diskset_conf.all = true;
            else if (!result_getdisks->get_payload().empty())
            {
                boost::split(diskset_conf.set,
                             result_getdisks->get_payload(),
                             boost::algorithm::is_any_of(" "));
                diskset_conf.all = false;
            }
            else
                diskset_conf.all = false;
            break;

        default:
            exa_cli_error("\n%sERROR%s, %s: %s",
                          COLOR_ERROR,
                          COLOR_NORM,
                          xml_get_prop(xmlnode, "hostname"),
                          result_getdisks ? result_getdisks->get_error_msg().
                          c_str() : exa_error_msg(error_code));
            throw CommandException("Can't retrieve available disks.");
        }

        /* Create the disk entries */
        if (diskset_arg.all)
        {
            if (diskset_conf.all)
                throw CommandException("Disk list not configured on the node.");

            /* add all disks from the configuration */
            for (set<string>::const_iterator it = diskset_conf.set.begin();
                 it != diskset_conf.set.end(); ++it)
            {
                const string &disk = *it;
                exa_uuid_t uuid;
                exa_uuid_str_t uuid_str;

                if (disk.empty())
                    continue;

                xmlNodePtr xmlrdev = xml_new_child(xmlnode, NULL, "disk", NULL);
                xml_set_prop(xmlrdev, "path", disk.c_str());

                /* Generate and assign an UUID */
                uuid_generate(&uuid);
                uuid2str(&uuid, uuid_str);
                xml_set_prop(xmlrdev, "uuid", uuid_str);
            }
        }
        else
            /* add matching disks from the arguments */
            for (set<string>::const_iterator it = diskset_arg.set.begin();
                 it != diskset_arg.set.end(); ++it)
            {
                const string &disk = *it;
                exa_uuid_t uuid;
                exa_uuid_str_t uuid_str;
                xmlNodePtr xmldisk;

                if (disk.empty())
                    continue;

                /* the disk from the arguments must match the configuration */
                if (!diskset_conf.all && diskset_conf.set.find(disk) ==
                    diskset_conf.set.end())
                    throw CommandException(
                        "Exanodes cannot use disk " + disk + " on the node.");

                /* create the XML node corresponding to the disk */
                xmldisk = xml_new_child(xmlnode, NULL, "disk", NULL);
                xml_set_prop(xmldisk, "path", disk.c_str());

                /* Generate and assign an UUID */
                uuid_generate(&uuid);
                uuid2str(&uuid, uuid_str);
                xml_set_prop(xmldisk, "uuid", uuid_str);
            }
    }
    else
    {
        /* Attribute UUIDs to each disk in the config file passed as argument.
         *
         * We need to set the UUIDs for the disks here, because the creation
         * of the new node itself is not clusterized and the XML .
         *
         * !!! In this case we don't compare the disk list from the XML to the
         * list returned by "get_nodedisks" command.
         */

        xmlNodeSetPtr xmldisk_set;
        xmlNodePtr xmliterator;
        int i;

        xmldisk_set = xml_conf_xpath_query(_xml_cfg,
                                           "//Exanodes/cluster/node/disk");

        /* iterate on the XML description of all disks */
        xml_conf_xpath_result_for_each(xmldisk_set, xmliterator, i)
        {
            exa_uuid_t uuid;
            exa_uuid_str_t uuid_str;

            /* Generate and assign an UUID */
            uuid_generate(&uuid);
            uuid2str(&uuid, uuid_str);
            xml_set_prop(xmliterator, "uuid", uuid_str);
        }

        xml_conf_xpath_free(xmldisk_set);
    }

    /* Extract the node description from the XmlNodeSet that contains it */
    xmlNodePtr xmlnode;
    xmlNodeSetPtr xmlnode_set;

    xmlnode_set = xml_conf_xpath_query(_xml_cfg, "//Exanodes/cluster/node");
    if (xml_conf_xpath_result_entity_count(xmlnode_set) != 1)
    {
        xml_conf_xpath_free(xmlnode_set);
        throw CommandException(
            "The configuration file should contain one and "
            "only one cluster/node information, check command usage with --help.");
    }
    xmlnode = xml_conf_xpath_result_entity_get(xmlnode_set, 0);

    /* The "hostname" property has been set by insert_node_network or we assume
     * that if a the command was called with -c, the hostname was properly set */
    string hostname(xml_get_prop(xmlnode, "hostname"));

    /* Checks the node to add is up and not inialized. */
    AdmindCommand command_name("get_cluster_name",  exa_uuid_zero);
    send_admind_to_node(hostname, command_name, error_code);
    if (error_code == EXA_ERR_CONNECT_SOCKET)
    {
        exa_cli_error(
            "%sERROR%s, %s: Failed to connect\n"
            " - First, check you can reach the node through the network,\n"
            " - then check exa_admind daemon is started on it.\n",
            COLOR_ERROR,
            COLOR_NORM,
            hostname.c_str());
        throw CommandException(EXA_ERR_DEFAULT);
    }
    if (error_code == EXA_SUCCESS)
    {
        exa_cli_error("%sERROR%s, %s: The node is already configured.\n",
                      COLOR_ERROR, COLOR_NORM, hostname.c_str());
        throw CommandException(EXA_ERR_DEFAULT);
    }

    /* Add the node to already existing nodes. */
    AdmindCommand command_add("clnodeadd", exa.get_cluster_uuid());
    command_add.add_param("tree", xmlnode);

    xml_conf_xpath_free(xmlnode_set);

    msg_str = "Adding a node to cluster '" + exa.get_cluster() + "'";

    send_command(command_add, msg_str, error_code, error_msg);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);

    shared_ptr<xmlDoc> cfg_file;
    error_code = get_configclustered(cfg_file);
    /* FIXME error_code seems to be ignored here */

    if (!cfg_file)
        throw CommandException("Could not get the cluster configuration");

    if (exa.set_config_node_add(_node_name,
                                hostname,
                                error_msg) != EXA_SUCCESS)
        exa_cli_error(
            "%sERROR%s: The addition of your node has been successful "
            "but we failed to update the local node cache:\n"
            "       %s\n",
            COLOR_ERROR,
            COLOR_NORM,
            error_msg.c_str());


    /* Add disks in the XML description of the CLI cache.
     * FIXME : confirm the comment
     */
    xmlNodePtr xmlnode_cache;
    xmlNodeSetPtr xmldisk_set;
    xmlNodePtr xmliterator;
    int i;

    xmlnode_cache = xml_conf_xpath_singleton(
        cfg_file.get(),
        "//Exanodes/cluster/node[@name='%s']",
        _node_name.c_str());
    EXA_ASSERT(xmlnode_cache);

    xmldisk_set = xml_conf_xpath_query(_xml_cfg, "//Exanodes/cluster/node/disk");

    xml_conf_xpath_result_for_each(xmldisk_set, xmliterator, i)
    xmlAddChild(xmlnode_cache, xmlCopyNodeList(xmliterator));
    xml_conf_xpath_free(xmldisk_set);

    /* Generate and call the "clcreate" admind command */
    AdmindCommand command_create("clcreate", exa_uuid_zero);
    command_create.add_param("config", cfg_file.get());
    command_create.add_param("join", ADMIND_PROP_TRUE);

    if (exa.get_license().empty())
        throw CommandException(
            "The license could not be read from the cluster.\n");

    xmlDocPtr license_doc = xml_new_doc("1.0");
    license_doc->children = xml_new_doc_node(license_doc,
                                             NULL,
                                             "Exanodes",
                                             NULL);
    xml_set_prop(license_doc->children, "license", exa.get_license().c_str());
    command_create.add_param("license", license_doc);

    command_create.replace_param("hostname", hostname);

    shared_ptr<AdmindMessage> message(send_admind_to_node(hostname,
                                                          command_create,
                                                          error_code));

    /* On success, we can finish here nothing more to print */
    if (error_code == EXA_SUCCESS)
        return;

    if (error_code == ADMIND_ERR_CLUSTER_ALREADY_CREATED)
        exa_cli_error("%sERROR%s: Could not integrate the node, it is already "
                      "part of a cluster\n", COLOR_ERROR, COLOR_NORM);
    else if (error_code == EXA_ERR_CONNECT_SOCKET)
        exa_cli_error(
            "%sERROR%s: We informed Exanodes that %s is now part of the cluster but\n"
            "       we failed to contact %s itself.\n"
            "       %s will not join the cluster once Exanodes is started on it.\n"
            "       Once %s is started, you can add it manually by using the command:\n"
            "       exa_clnoderecover %s --join -n %s\n",
            COLOR_ERROR,
            COLOR_NORM,
            _node_name.c_str(),
            _node_name.c_str(),
            _node_name.c_str(),
            _node_name.c_str(),
            exa.get_cluster().c_str(),
            hostname.c_str());
    else if (error_code == RDEV_ERR_CANT_OPEN_DEVICE)
        exa_cli_warning("\n%sWARNING%s: %s\n", COLOR_WARNING, COLOR_NORM,
                        message->get_error_msg().c_str());
    else
        exa_cli_error("%sERROR%s: Integration of the node failed: %s\n",
                      COLOR_ERROR, COLOR_NORM, message->get_error_msg().c_str());

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_clnodeadd::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    /* FIXME This is supposed to be done by command.cpp thanks to option
     *       groups, but it's "broken": groups should not implicitely be
     *       exclusive. It should be explicitely stated with something like
     *       exclude_option_groups(group1, group2, ...). */
    if (opt_args.find('c') != opt_args.end() &&
        (opt_args.find('i') != opt_args.end() || opt_args.find('n') !=
         opt_args.end()))
    {
        std::stringstream msg;
        msg << "Only one of (--config " << OPT_ARG_CONFIG_FILE <<
        " and --disk "
            << OPT_ARG_DISK_PATH << " --node " << OPT_ARG_NODE_HOSTNAME
            << ") should be provided.";
        throw CommandException(msg.str());
    }

    if (opt_args.find('i') != opt_args.end())
        _disks = opt_args.find('i')->second;
    if (opt_args.find('c') != opt_args.end())
        _xml_cfg = xml_conf_init_from_file(opt_args.find('c')->second.c_str());
    if (opt_args.find('D') != opt_args.end())
        _datanetwork = opt_args.find('D')->second.c_str();
    if (opt_args.find('n') != opt_args.end())
        _node_name = opt_args.find('n')->second.c_str();
}


void exa_clnodeadd::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Add a node to a cluster.";
}


void exa_clnodeadd::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Add the node " << OPT_ARG_NODE_HOSTNAME <<
    " and possibly its disks "
        << OPT_ARG_DISK_PATH << " to the cluster " << ARG_CLUSTERNAME << ". ";
    out << OPT_ARG_DISK_PATH << " is the path of the disk." << std::endl;
    out << "You can specify multiple " << OPT_ARG_DISK_PATH <<
    " if you separate them by a space and enclose the whole list with quotes."
   << std::endl;

    out << OPT_ARG_DATANETWORK_HOSTNAME <<
    " is the hostname or IP address to use for the data network. "
        <<
    "If not specified, the default hostname or IP address of the node will be "
        << "used. All the nodes have to be up to use this command." << std::endl;

    if (show_hidden == false)
        return;

    out << "The configuration of this node can be provided in the " <<
    OPT_ARG_CONFIG_FILE
        << " initialization file." << std::endl;
}


void exa_clnodeadd::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Add the node " << Boldify("node3") << " to the cluster "
        << Boldify("mycluster") << " with the disk " << Boldify("/dev/sdb:") <<
    std::endl;
    out << "  " << "exa_clnodeadd mycluster --node node3 --disk /dev/sdb" <<
    std::endl;
    out << std::endl;

    out << "Add the node " << Boldify("node3") << " to the cluster "
        << Boldify("mycluster") << " with an alternate data network hostname:" <<
    std::endl;
    out << "  " <<
    "exa_clnodeadd --node node3 --datanetwork 'node3-data.mydomain' mycluster" <<
    std::endl;
    out << std::endl;

    if (show_hidden == false)
        return;

    out << "Add the node " << Boldify("node3") << " to the cluster "
        << Boldify("mycluster") << " based on the initialization file "
        << Boldify("node3.conf") << ":" << std::endl;
    out << "  " << "exa_clnodeadd mycluster -config node3.conf" << std::endl;
    out << std::endl;

    out <<
    "Below is a example of an initialization file to add node to a cluster. "
        << "This node provides 2 disks to Exanodes (/dev/sdb and /dev/sdc)."
        << std::endl;
    out << Boldify("Note") <<
    ": The command exa_makeconfig can help you create an initialization file."
        << std::endl;
    out << "<Exanodes>" << std::endl
        << "  <cluster name='mycluster'>" << std::endl
        << "    <node name='node3' hostname='node3.mydomain'>" << std::endl
        << "      <network hostname='node3.mydomain'/>" << std::endl
        << "      <disk path='/dev/sdb'/>" << std::endl
        << "      <disk path='/dev/sdc'/>" << std::endl
        << "    </node>" << std::endl
        << "  </cluster>" << std::endl
        << "</Exanodes>" << std::endl;
    out << std::endl;
}


