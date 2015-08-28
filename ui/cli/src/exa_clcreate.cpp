/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_clcreate.h"
#include "ui/cli/src/cli.h"
#include "ui/cli/src/license.h"

#include <sys/stat.h>
#include <iostream>
#include <fstream>

#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_mkstr.h"
#include "common/include/uuid.h"
#include "config/pkg_data_dir.h"
#include "os/include/os_file.h"
#include "os/include/os_dir.h"
#include "os/include/os_network.h"
#include "os/include/os_string.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/config_check.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/split_node_disk.h"

#include "boost/lexical_cast.hpp"
#include <boost/algorithm/string.hpp>

/** DTD to valide policy and tuning config files. */
#define EXA_TUNABLES_DTD                                \
    "<!ELEMENT Exanodes (tunables)>\n"                  \
    "<!ELEMENT tunables (tunable)*>\n"                  \
    "<!ELEMENT tunable EMPTY>\n"                        \
    "<!ATTLIST tunable name CDATA #REQUIRED>\n"         \
    "<!ATTLIST tunable value CDATA #IMPLIED>\n"         \
    "<!ATTLIST tunable default_value CDATA #IMPLIED>\n"

/** tag corresponding to all disks available on one node */
#define ALL_DISKS_OF_NODE "all"
#define ANY_DISKS_OF_NODE "any"

using boost::lexical_cast;
using boost::shared_ptr;
using std::vector;
using std::map;
using std::set;

const std::string exa_clcreate::OPT_ARG_CONFIG_FILE(Command::Boldify("FILE"));
const std::string exa_clcreate::OPT_ARG_HOSTNAME(Command::Boldify("HOSTNAME"));
const std::string exa_clcreate::OPT_ARG_SPOFGROUP(Command::Boldify("SPOFGROUP"));
const std::string exa_clcreate::OPT_ARG_DISK_PATH(Command::Boldify("PATH"));
const std::string exa_clcreate::OPT_ARG_LOAD_TUNING_FILE(Command::Boldify(
                                                             "FILE"));
const std::string exa_clcreate::OPT_ARG_LICENSE_FILE(Command::Boldify("FILE"));
const std::string exa_clcreate::OPT_ARG_DATANETWORK_HOSTNAME(Command::Boldify(
                                                                 "HOSTNAME"));

exa_clcreate::exa_clcreate(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , _datanetwork("")
{}


exa_clcreate::~exa_clcreate()
{}

void exa_clcreate::init_options()
{
    exa_clcommand::init_options();

    add_option('c', "config", "Specify the initialization file.",
               1, true, true, OPT_ARG_CONFIG_FILE, "1");

    add_option('i', "disk", "Specify the nodes and disks to use.", 1, false,
               true, "'" + OPT_ARG_HOSTNAME + EXA_CONF_SEPARATOR +
               OPT_ARG_DISK_PATH + "...'");

    add_option('l', "license", "Load the license file.", 0, true, true,
               OPT_ARG_LICENSE_FILE);

    add_option('s', "spof-group", "Specify SPOF groups: a SPOF group is a "
               "space-separated list of " + OPT_ARG_HOSTNAME + "s enclosed in "
               "square brackets.", 0, false, true, "'" + OPT_ARG_SPOFGROUP +
               "...'");

    add_option('t', "load-tuning", "Load parameters provided by your reseller "
               "or generated on a previous/other cluster with exa_cltune "
               "--save.", 0, false, true, OPT_ARG_LOAD_TUNING_FILE);

    add_option('D', "datanetwork", "Specify the hostnames or IP addresses to "
               "use for the data network: "
               " pairs of " + OPT_ARG_DATANETWORK_HOSTNAME + EXA_CONF_SEPARATOR
               + OPT_ARG_DATANETWORK_HOSTNAME + " enclosed with quotes.",
               0, false, true, "'" + OPT_ARG_DATANETWORK_HOSTNAME
               + EXA_CONF_SEPARATOR + OPT_ARG_DATANETWORK_HOSTNAME + "...'");
}


void exa_clcreate::init_see_alsos()
{
    add_see_also("exa_expand");
    add_see_also("exa_cldelete");
    add_see_also("exa_clstart");
    add_see_also("exa_clstop");
    add_see_also("exa_clinfo");
    add_see_also("exa_clstats");
    add_see_also("exa_cltune");
    add_see_also("exa_clreconnect");
}


/** Describes the disk set of a node */
typedef struct
{
    bool all;                   /**< all disks are available */
    std::set<string> set;       /**< set of disks */
}diskset_t;

typedef std::map< std::string, diskset_t > nodediskmap_t;

/** \brief Filter that aggregates the returns from get_node_disks clustered command */
struct getnodedisks_filter : private boost::noncopyable
{

    /** at least one node returned an error */
    bool got_error;
    /** map containing the disk vector by node */
    nodediskmap_t diskmap;

    getnodedisks_filter() :
        got_error(false)
    {}


    /** \brief filter error received by each nodes in a send_admind_by_node sequence
     *
     * \param node: the node
     * \param xml_cmd: the command ran on this node
     * \param error_code: the error received
     * \param value: the value received
     * \param admind_status: the current admind_status
     *
     * \return EXA_ERROR_DEFAULT to stop command processing
     *         EXA_SUCCESS to continue command processing to next nodes
     */
    void operator ()(const std::string &node, exa_error_code error_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_SUCCESS:
            /* [] operator insert an empty set into the map */
            if (message->get_payload() == ANY_DISKS_OF_NODE)
                diskmap[node].all = true;
            else if (!message->get_payload().empty())
            {
                boost::split(diskmap[node].set,
                             message->get_payload(),
                             boost::algorithm::is_any_of(" "));
                diskmap[node].all = false;
            }
            else
                diskmap[node].all = false;
            break;

        default:
            exa_cli_error("\n%sERROR%s, %s: %s", COLOR_ERROR, COLOR_NORM,
                          node.c_str(),
                          message ? message->get_error_msg().c_str()
                          : exa_error_msg(error_code));
            got_error = true;
        }
    }
};

/** \brief filter error received by each nodes in a send_admind_by_node sequence
 *
 * \param node: the node
 * \param xml_cmd: the command ran on this node
 * \param error_code: the error received
 * \param value: the value received
 * \param admind_status: the current admind_status
 *
 * \return EXA_ERROR_DEFAULT to stop command processing
 *         EXA_SUCCESS to continue command processing to next nodes
 */
struct getclustername_filter : private boost::noncopyable
{
    bool   filter_got_error;
    uint   ready_to_create;
    string cluster_already_created_on;
    uint   connect_socket_error;

    getclustername_filter() :
        filter_got_error(false),
        ready_to_create(0),
        connect_socket_error(0)
    {}


    void operator ()(const std::string &node, exa_error_code error_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_ERR_ADMIND_NOCONFIG:
            /* The Expected case */
            ready_to_create++;
            break;

        case EXA_SUCCESS:
            if (!message->get_payload().empty())
                exa_cli_error(
                    "\n%sERROR%s, %s: A cluster named '%s' is already "
                    "configured",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_payload().c_str());
            else
                exa_cli_error(
                    "\n%sERROR%s, %s: A cluster is already configured",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str());
            cluster_already_created_on = node;
            filter_got_error = true;
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_error("\n%sERROR%s, %s: Failed to connect",
                          COLOR_ERROR, COLOR_NORM, node.c_str());
            connect_socket_error++;
            filter_got_error = true;
            break;

        default:
            exa_cli_error("\n%sERROR%s, %s: %s", COLOR_ERROR, COLOR_NORM,
                          node.c_str(),
                          message ? message->get_error_msg().c_str()
                          : exa_error_msg(error_code));
            filter_got_error = true;
        }
    }
};

/** \brief filter error received by each nodes in a send_admind_by_node sequence
 *
 * \param node: the node
 * \param xml_cmd: the command ran on this node
 * \param error_code: the error received
 * \param value: the value received
 * \param admind_status: the current admind_status
 *
 * \return EXA_ERROR_DEFAULT to stop command processing
 *         EXA_SUCCESS to continue command processing to next nodes
 */
struct clcreate_filter : private boost::noncopyable
{
    std::set<std::string> nodelist_succeed;
    Line &line;

    clcreate_filter(Line &_line) :
        nodelist_succeed(),
        line(_line)
    {}


    void operator ()(const std::string &node, exa_error_code error_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_SUCCESS:
            nodelist_succeed.insert(node);
            break;

        default:
            line.process(*message);
            break;
        }
    }
};

/**
 * Set the hostname override of a (clcreate) command.
 *
 * \param[in]     hostname  Hostname to use
 * \param[in,out] command   Command to modify
 */
static void set_hostname_override(const string &hostname,
                                  AdmindCommand &command)
{
    command.replace_param("hostname", hostname);
}


static int xml_validate(xmlDocPtr doc)
{
    xmlParserInputBufferPtr dtd_buf;
    xmlValidCtxtPtr vctxt;
    xmlDtdPtr dtd;
    int ret;

    /* Parse the DTD */
    dtd_buf = xmlParserInputBufferCreateStatic(EXA_TUNABLES_DTD,
                                               sizeof(EXA_TUNABLES_DTD),
                                               XML_CHAR_ENCODING_UTF8);
    dtd = xmlIOParseDTD(NULL, dtd_buf, XML_CHAR_ENCODING_UTF8);
    EXA_ASSERT_VERBOSE(dtd != NULL, "failed to parse the DTD");

    /* Create the validation context */
    vctxt = xmlNewValidCtxt();
    if (vctxt == NULL)
    {
        exa::Exception ex("failed to allocate memory", EXA_ERR_DEFAULT);
        throw ex;
    }

    /* Print validation errors on stderr */
    vctxt->userData = (void *) stderr;
    vctxt->error    = (xmlValidityErrorFunc) fprintf;
    vctxt->warning  = (xmlValidityWarningFunc) fprintf;

    /* Validation */
    ret = xmlValidateDtd(vctxt, doc, dtd);

    /* Free memory */
    xmlFreeValidCtxt(vctxt);
    xmlFreeDtd(dtd);

    return ret ? EXA_SUCCESS : EXA_ERR_DEFAULT;
}


/**
 * Load the given tunable file in the proper place of the xml_cfg tree
 *
 * @param policy: if true, load as a policy file (with "default_value"
 *                attributes). Else load as a tuning file (with "value"
 *                attributes).
 */
exa_error_code exa_clcreate::load_tune_file(string tuning_file,
                                            xmlDocPtr xml_cfg,
                                            bool policy)
{
    xmlDocPtr tuneDocPtr(xml_conf_init_from_file(tuning_file.c_str()));
    xmlNodeSetPtr nodeset;
    xmlNodePtr cfg_tunables;
    int i;

    if (!tuneDocPtr)
    {
        exa_cli_error("%sERROR%s: failed to parse '%s'\n",
                      COLOR_ERROR, COLOR_NORM, tuning_file.c_str());
        return EXA_ERR_DEFAULT;
    }

    if (xml_validate(tuneDocPtr) != EXA_SUCCESS)
    {
        exa_cli_error("%sERROR%s: failed to validate '%s'\n",
                      COLOR_ERROR, COLOR_NORM, tuning_file.c_str());
        return EXA_ERR_DEFAULT;
    }

    nodeset = xml_conf_xpath_query(tuneDocPtr, "//Exanodes/tunables");
    EXA_ASSERT(nodeset);

    nodeset = xml_conf_xpath_query(tuneDocPtr, "//Exanodes/tunables/tunable");
    if (!nodeset)
        return EXA_SUCCESS;

    cfg_tunables = xml_new_child(xml_cfg->children, NULL, "tunables", NULL);

    for (i = 0; i < xml_conf_xpath_result_entity_count(nodeset); i++)
    {
        xmlNodePtr tuning_tunable;
        xmlNodePtr cfg_tunable;
        const char *name;
        const char *default_value;
        const char *value;

        tuning_tunable = xml_conf_xpath_result_entity_get(nodeset, i);
        name          = xml_get_prop(tuning_tunable, "name");
        default_value = xml_get_prop(tuning_tunable, "default_value");
        value         = xml_get_prop(tuning_tunable, "value");

        /* Check default_value/value attribute depending on file type (policy ?) */
        if (policy)
        {
            if (!default_value)
            {
                fprintf(stderr,
                        "Missing attribute 'default_value' for parameter "
                        "'%s'.\n",
                        name);
                return EXA_ERR_DEFAULT;
            }
            if (value)
            {
                fprintf(stderr,
                        "The policy file should contain 'default_value' "
                        "attributes instead of 'value' ones.\n");
                return EXA_ERR_DEFAULT;
            }
        }
        else
        {
            if (!value)
            {
                fprintf(stderr,
                        "Missing attribute 'value' for parameter '%s'.\n",
                        name);
                return EXA_ERR_DEFAULT;
            }
            if (default_value)
            {
                fprintf(stderr, "The tuning file should contain 'value' "
                        "attributes instead of 'default_value' ones.\n");
                return EXA_ERR_DEFAULT;
            }
        }

        cfg_tunable = xml_conf_xpath_singleton(
            xml_cfg,
            "//Exanodes/tunables/tunable[@name='%s']",
            name);

        if (cfg_tunable == NULL)
        {
            cfg_tunable = xml_new_child(cfg_tunables, NULL, "tunable", NULL);
            xml_set_prop(cfg_tunable, "name", name);
        }

        if (default_value != NULL)
            xml_set_prop(cfg_tunable, "default_value", default_value);
        if (value != NULL)
            xml_set_prop(cfg_tunable, "value", value);
    }

    return EXA_SUCCESS;
}


static bool verify_bracket_syntax(const string str)
{
    const char *walk = str.c_str();
    int count = 0;

    if (*walk != '[')
        return false;

    while (*walk != '\0')
    {
        if (*walk == '[')
            count++;
        else if (*walk == ']')
            count--;
        else if (!isblank(*walk) && count != 1)
            return false;

        if (count < 0 || count > 1)
            return false;

        walk++;
    }

    return count == 0;
}


list<list<string> > exa_clcreate::parse_spof_groups(const string spof_groups)
{
    list<list<string> > spof_list;
    vector<string> rawspof_vect;
    string trimmed_spof_groups = spof_groups;

    boost::trim(trimmed_spof_groups);

    if (trimmed_spof_groups.empty())
        return spof_list;

    if (!verify_bracket_syntax(trimmed_spof_groups))
        throw CommandException("Parse error in SPOF groups '"
                               + trimmed_spof_groups + "'.");

    /* Split the trimmed_spof_groups by closing character, which is the easiest
     * way to parse it. Given [test/1-2/ test3]  [test5 test6][test/7-8/]
     * we'll get '[test/1-2/ test3' '  [test5 test6' and '[test/7-8/'.
     * The drawback is we have to trim and remove the starting '[' in
     * each substring.
     */

    boost::split(rawspof_vect, trimmed_spof_groups,
                 boost::algorithm::is_any_of("]"));

    for (vector<string>::const_iterator it = rawspof_vect.begin();
         it != rawspof_vect.end(); ++it)
    {
        string spof = *it;

        boost::replace_first(spof, "[", "");
        boost::trim(spof);

        if (spof == "")
        {
            /* We don't allow an empty SPOF with nothing else
             * but we're forced to accept that the split vector
             * will contain a last empty element, in which case
             * we just break the loop
             */
            if (!spof_list.empty() && (it + 1) == rawspof_vect.end())
                break;
            else
                throw CommandException("Parse error in SPOF groups '"
                                       + trimmed_spof_groups + "'.");
        }

        exa_cli_trace("Got spof group '%s'\n", spof.c_str());

        /* One spof, containing nodes as strings */
        list<string> spof_nodes;
        vector<string> nodes;

        /* Now split the spof definition into nodes regexps */
        boost::split(nodes, spof, boost::algorithm::is_any_of(" "));

        for (vector<string>::const_iterator it = nodes.begin();
             it != nodes.end(); ++it)
        {
            string node = *it;
            set<string> node_subset;

            boost::trim(node);
            try
            {
                node_subset = exa_expand(node);
            }
            catch (string msg)
            {
                throw CommandException(msg);
            }

            /* insert all the nodes expanded from the regexp (which
             * may be only one) into the current spof_nodes list.
             */
            for (set<string>::const_iterator it = node_subset.begin();
                 it != node_subset.end(); ++it)
                spof_nodes.push_back(*it);
        }

        /* insert our current spof list into all spofs list. */
        spof_list.push_back(spof_nodes);
    }

    return spof_list;
}


void exa_clcreate::run()
{
    int ret = 0;
    std::string error_msg;
    xmlNodePtr node;
    xmlDocPtr xml_cfg;

    shared_ptr<Line> line;
    std::string licensebuf;
    xmlDocPtr license_doc = NULL;

    if (!_config_file.empty())
    {
        /* We use the XML configuration file passed as argument to create the cluster
         *
         * !!! In this case we don't compare the disk list from the XML to the
         * list returned by "get_nodedisks" command.
         */
        xml_cfg = Exabase::parse_config_file(_config_file);
        if (!xml_cfg)
            throw CommandException(
                "Error parsing configuration file '" + _config_file + "'.");
    }
    else
    {
        /* Create the config file based on user input */
        xml_cfg = xml_new_doc("1.0");
        xml_cfg->children = xml_new_doc_node(xml_cfg, NULL, "Exanodes", NULL);

        if (_disks.empty())
            throw CommandException(
                "You must provide the nodes and disks for your cluster using '-i' or '--disk'.\n"
                "       Check command usage with --help.\n");

        /* Now ready to add node entry */
        xmlNodePtr xmlnode_cluster = xml_new_child(xml_cfg->children,
                                                   NULL,
                                                   EXA_CONF_CLUSTER,
                                                   NULL);
        xml_set_prop(xmlnode_cluster, "name", _cluster_name.c_str());

        /* Create and Set the UUID */
        exa_uuid_str_t cluster_uuid_str;
        exa_uuid_t cluster_uuid;
        uuid_generate(&cluster_uuid);
        uuid2str(&cluster_uuid, cluster_uuid_str);
        xml_set_prop(xmlnode_cluster, EXA_CONF_CLUSTER_UUID, cluster_uuid_str);

        /* Build the map associating each node with a set of local disks
         * based on the command line arguments */
        nodediskmap_t diskmap_arg;      /* Map of disks by node (from the CLI arguments) */
        set<string> node_set;           /* Set of disks */
        vector<string> rawdiskarg_vect;         /* Raw regexp vector */

        boost::split(rawdiskarg_vect, _disks, boost::algorithm::is_any_of(" "));

        for (vector<string>::const_iterator it = rawdiskarg_vect.begin();
             it != rawdiskarg_vect.end(); ++it)
        {
            string raw_arg = *it;               /* raw argument containing node regexp and disk name */
            string node_regexp;         /* regexp of nodes */
            string disk;                        /* disk name */
            set<string> node_subset;    /* Sub-set of nodes corresponding to the regexp */

            split_node_disk(raw_arg, node_regexp, disk);

            /* Expand the node regexp */
            try
            {
                node_subset = exa_expand(node_regexp);
            }
            catch (string msg)
            {
                throw CommandException(msg);
            }

            /* Fill the disk map for the subset of nodes */
            for (set<string>::const_iterator it = node_subset.begin();
                 it != node_subset.end(); ++it)
            {
                diskmap_arg[*it].all = false;

                if (disk == ALL_DISKS_OF_NODE)
                    diskmap_arg[*it].all = true;
                else if (disk != "")
                    diskmap_arg[*it].set.insert(disk);
            }

            /* Append the subset to the global set of nodes */
            for (set<string>::const_iterator it = node_subset.begin();
                 it != node_subset.end(); ++it)
                node_set.insert(*it);
        }

        list<list<string> > spof_list = parse_spof_groups(_spof_groups);

        for (list<list<string> >::const_iterator it = spof_list.begin();
             it != spof_list.end(); ++it)
        {
            exa_cli_trace("parsed SPOF group:\n");
            list<string> nodes = *it;
            for (list<string>::const_iterator n_it = nodes.begin();
                 n_it != nodes.end(); ++n_it)
            {
                string node = *n_it;
                exa_cli_trace("  %s\n", node.c_str());
            }
        }

        /* Build the map associating each node with a set of local disks
         * based on on the configuration retrieved from the nodes */
        AdmindCommand command_getdisks("get_nodedisks", exa_uuid_zero);
        struct getnodedisks_filter command_filter;

        send_admind_by_node(command_getdisks, node_set,
                            boost::ref(command_filter));
        if (command_filter.got_error)
            throw CommandException("Can't retrieve available disks.");

        nodediskmap_t &diskmap_conf = command_filter.diskmap;    /* Map of disks by node (from the configuration of the nodes) */

        /* Currently, a cluster with no node is not allowed to create */
        if (node_set.empty())
            throw CommandException("No node is provided");

        /* Create each node entry */
        exa_cli_info("Creating the cluster %s:\n", _cluster_name.c_str());

        int node_id = 0;
        for (set<string>::const_iterator it = node_set.begin();
             it != node_set.end(); ++it, ++node_id)
        {
            const string &node = *it;
            xmlNodePtr xmlnode = xml_new_child(xmlnode_cluster,
                                               NULL,
                                               "node",
                                               NULL);
            char canonical_hostname[EXA_MAXSIZE_HOSTNAME + 1];

            xml_set_prop(xmlnode, "name", node.c_str());

            /* Assign the node number in the tree */
            xml_set_prop(xmlnode, EXA_CONF_NODE_NUMBER,
                         lexical_cast<string>(node_id).c_str());

            if (os_host_canonical_name(node.c_str(), canonical_hostname,
                                       sizeof(canonical_hostname)) != 0)
                throw ConfigException(
                    "Couldn't get canonical name of '" + node + "'");
            xml_set_prop(xmlnode, EXA_CONF_NODE_HOSTNAME, canonical_hostname);

            ConfigCheck::insert_node_network(xmlnode, _datanetwork);

            if (diskmap_arg[node].all)
            {
                if (diskmap_conf[node].all)
                    throw CommandException(
                        "Disk list not configured on node " + node + ".");

                /* add all disks from the configuration */
                for (set<string>::const_iterator it =
                         diskmap_conf[node].set.begin();
                     it != diskmap_conf[node].set.end(); ++it)
                {
                    const string &disk = *it;

                    if (disk == "")
                        continue; /* FIXME : I think that an assert would be more correct */

                    xmlNodePtr xmlrdev = xml_new_child(xmlnode,
                                                       NULL,
                                                       "disk",
                                                       NULL);
                    xml_set_prop(xmlrdev, "path", disk.c_str());
                }
            }
            else
                /* add matching disks from the arguments */
                for (set<string>::const_iterator it =
                         diskmap_arg[node].set.begin();
                     it != diskmap_arg[node].set.end(); ++it)
                {
                    const string &disk = *it;
                    EXA_ASSERT(!disk.empty());

                    /* the disk from the arguments must match the configuration */
                    if (!diskmap_conf[node].all &&
                        diskmap_conf[node].set.find(disk) ==
                        diskmap_conf[node].set.end())
                        throw CommandException(
                            "Exanodes cannot use disk " + disk + " on node " +
                            node + ".");

                    xmlNodePtr xmlrdev = xml_new_child(xmlnode,
                                                       NULL,
                                                       "disk",
                                                       NULL);
                    xml_set_prop(xmlrdev, "path", disk.c_str());
                }
        }

        xmlNodePtr spofs_xml_node = xml_new_child(xmlnode_cluster,
                                                  NULL,
                                                  "spof-groups",
                                                  NULL);
        for (list<list<string> >::const_iterator it = spof_list.begin();
             it != spof_list.end(); ++it)
        {
            list<string> spof_nodes = *it;
            xmlNodePtr spof_xml_node = xml_new_child(spofs_xml_node,
                                                     NULL,
                                                     "spof-group",
                                                     NULL);

            for (list<string >::const_iterator it = spof_nodes.begin();
                 it != spof_nodes.end(); ++it)
            {
                string node = *it;
                xmlNodePtr spofnode_xml_node = xml_new_child(spof_xml_node,
                                                             NULL,
                                                             "node",
                                                             NULL);
                xml_set_prop(spofnode_xml_node, "name", node.c_str());
            }
        }
    }

    /* Load and append the license file */
    license_doc = xml_new_doc("1.0");
    license_doc->children = xml_new_doc_node(license_doc,
                                             NULL,
                                             "Exanodes",
                                             NULL);

    if (!_license_file.empty()) {
        string license_line;
        std::ifstream licensefd(_license_file.c_str(), std::ios::in);
    
        if (licensefd.fail())
    	    throw CommandException("Failed to read the license file from '" +
    			    _license_file + "'.");
        while (getline(licensefd, license_line))
    	    licensebuf += license_line + "\n";
    
        licensefd.close();
    } else {
        licensebuf = githubLicense;
    }
    xml_set_prop(license_doc->children, "license", licensebuf.c_str());

    /* Include default tuning values if any */
    string fullname(PKG_DATA_DIR OS_FILE_SEP "default_tunables.conf");
    struct stat statbuf;

    if (stat(fullname.c_str(), &statbuf) == 0 && S_ISREG(statbuf.st_mode))
        if (load_tune_file(fullname, xml_cfg, true) != EXA_SUCCESS)
            throw CommandException(EXA_ERR_DEFAULT);

    /* Include tuning values if any */
    if (!_tuning_file.empty())
        if (load_tune_file(_tuning_file, xml_cfg, false) != EXA_SUCCESS)
            throw CommandException(EXA_ERR_DEFAULT);

    if (ConfigCheck::check_param(ConfigCheck::CHECK_NAME,
                                 EXA_MAXSIZE_CLUSTERNAME, _cluster_name,
                                 false) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    ret = exa.set_cluster_from_config(_cluster_name,
                                      xml_cfg,
                                      licensebuf,
                                      error_msg);

    if (ret != EXA_SUCCESS)
        throw CommandException(error_msg);

    std::set<std::string> nodelist = exa.get_hostnames();

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    /* We need to set the UUIDs for the disks here, because the creation
     * of the cluster itself is not clusterized (obviously!). */
    int i;
    xmlNodeSetPtr node_set =
        xml_conf_xpath_query(xml_cfg, "//cluster/node/disk");

    xml_conf_xpath_result_for_each(node_set, node, i)
    {
        exa_uuid_t uuid;
        exa_uuid_str_t uuid_str;

        /* Assign an uuid */
        uuid_generate(&uuid);
        uuid2str(&uuid, uuid_str);
        xml_set_prop(node, "uuid", uuid_str);
    }

    xml_conf_xpath_free(node_set);

    /*
     * Send a command to each node for each cluster to check nodes are free
     */
    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Checking nodes are not already used");

    AdmindCommand command_getname("get_cluster_name", exa_uuid_zero);

    getclustername_filter mygetclusternamefilter;

    send_admind_by_node(command_getname, nodelist,
                        boost::ref(mygetclusternamefilter));

    /* Display the global status */
    if (!mygetclusternamefilter.filter_got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);

    if (mygetclusternamefilter.connect_socket_error)
        exa_cli_error(
            "\n%sERROR%s: Failed to contact one or more nodes of your cluster.\n"
            " - First, check you can reach your nodes on your network,\n"
            " - then check exa_admind daemon is started on them.\n",
            COLOR_ERROR,
            COLOR_NORM);

    if (mygetclusternamefilter.ready_to_create != exa.get_hostnames().size())
    {
        /* It does not worked, we need to remove the nodes cache file */
        exa.del_cluster();

        if (mygetclusternamefilter.cluster_already_created_on.empty())
            exa_cli_info(
                "\nCluster creation is not possible. Please fix previous error first.\n");

        throw CommandException(EXA_ERR_DEFAULT);
    }

    /*
     * Send command to each node for each cluster
     * WARNING: The "node" will be set by send_admind_by_node
     */
    AdmindCommand command_create("clcreate", exa_uuid_zero);
    command_create.add_param("config", (xmlDocPtr) xml_cfg);
    command_create.add_param("join", ADMIND_PROP_FALSE);

    /* Load and append the license file if any */
    if (license_doc)
        command_create.add_param("license", license_doc);

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Sending cluster configuration");

    line = shared_ptr<Line>(new Line(exa));

    clcreate_filter myclcreatefilter(*line);

    unsigned int nb_error(send_admind_by_node(command_create, nodelist,
                                              boost::ref(
                                                  myclcreatefilter),
                                              set_hostname_override));

    if (nb_error == 0)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    /* Display line by freing it */
    line = shared_ptr<Line>();

    if (nb_error > 0)
    {
        exa_cli_info(
            "The creation of the cluster failed. Please fix the error above and restart this command.\n");

        /* Rollback, deleting cluster on nodes that accepted it */
        AdmindCommand command_delete("cldelete", exa.get_cluster_uuid());
        command_delete.add_param("recursive", ADMIND_PROP_TRUE);

        exa_cli_info("%-" exa_mkstr(
                         FMT_TYPE_H1) "s ",
                     "Rollback, deleting the cluster on the nodes where the creation succeeded.\n");

        send_admind_by_node(command_delete,
                            myclcreatefilter.nodelist_succeed,
                            NULL);

        /* It does not worked, we need to remove the nodes cache file */
        exa.del_cluster();

        throw CommandException(EXA_ERR_DEFAULT);
    }

    if (nb_error)
    {
        exa_cli_warning(
            "%sWARNING%s: The cluster creation failed on one or more nodes.\n"
            "You have two options:\n"
            "- Start your cluster with exa_clstart, the nodes with errors will not start.\n"
            "  Once you have fixed the error above, you can join the missing nodes\n"
            "  with 'exa_clnoderecover --join'.\n"
            "- Delete your cluster now with exa_cldelete, fix the error above\n"
            "  and create your cluster again.\n",
            COLOR_WARNING,
            COLOR_NORM);
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    }
}


void exa_clcreate::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('c') != opt_args.end())
        _config_file = opt_args.find('c')->second;

    if (opt_args.find('i') != opt_args.end())
        _disks = opt_args.find('i')->second;

    if (opt_args.find('l') != opt_args.end())
        _license_file = opt_args.find('l')->second;

    if (opt_args.find('s') != opt_args.end())
        _spof_groups = opt_args.find('s')->second;

    if (opt_args.find('t') != opt_args.end())
        _tuning_file = opt_args.find('t')->second;

    if (opt_args.find('D') != opt_args.end())
        _datanetwork = opt_args.find('D')->second;
}


void exa_clcreate::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Create an Exanodes cluster.";
}


void exa_clcreate::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Create a cluster named " << ARG_CLUSTERNAME << " with nodes " <<
    OPT_ARG_HOSTNAME << " and disks "
                     << "'" + OPT_ARG_HOSTNAME + EXA_CONF_SEPARATOR +
    OPT_ARG_DISK_PATH +
    "...', "
        << "using the license in the provided " + OPT_ARG_LICENSE_FILE + "." <<
    std::endl << std::endl;
    out << OPT_ARG_HOSTNAME <<
    " is a regular expansion (see exa_expand). Note: these hostnames "
        <<
    "have to be used 'as is' in other Exanodes commands (aliases will not work)."
   << std::endl;
    out << OPT_ARG_DISK_PATH << " is the path of the disk." << std::endl;
    out << "To specify multiple " << OPT_ARG_HOSTNAME + EXA_CONF_SEPARATOR +
    OPT_ARG_DISK_PATH
        << ", separate them by a space and enclose the whole list with quotes."
   << std::endl;
    out << std::endl;

    out << Boldify("CAUTION! The disks will be erased.") << std::endl;
}


void exa_clcreate::dump_examples(std::ostream &out, bool show_hidden) const
{
    Subtitle(out, "Cluster with same disks:");

    out << "Create a Linux cluster named " << Boldify("mycluster") <<
    " with nodes "
        << Boldify("node1") << ", " << Boldify("node2") << " and " << Boldify(
        "node3") << ","
    " using disk " << Boldify("/dev/sdb") << " on each of them:" << std::endl;
    out << "       exa_clcreate --license cluster.lic --disk node/1-3/" <<
    EXA_CONF_SEPARATOR << "/dev/sdb mycluster" << std::endl;
    out << std::endl;
    out << "Create a Windows cluster named " << Boldify("mycluster") <<
    " with nodes "
        << Boldify("node1") << ", " << Boldify("node2") << " and " << Boldify(
        "node3") << ","
    " using disk " << Boldify("E:") << " on each of them:" << std::endl;
    out << "      exa_clcreate --license cluster.lic --disk node/1-3/" <<
    EXA_CONF_SEPARATOR << "E: mycluster" << std::endl;
    out << std::endl;

    Subtitle(out, "Cluster with all disks:");

    out << "Create a Linux or Windows cluster named " <<
    Boldify("mycluster") << " with nodes "
                         << Boldify("node1") << ", " << Boldify("node2") <<
    " and " << Boldify(
        "node3") << ","
    " using all the disks available to Exanodes " << ":" << std::endl;
    out << "      exa_clcreate --license cluster.lic --disk node/1-3/" <<
    EXA_CONF_SEPARATOR << ALL_DISKS_OF_NODE << std::endl;
    out << std::endl;
    out << "By default, all disks are considered available." << std::endl;
    out <<
    "See the user manual on how to restrict, on a given node, which disks"
    " are made available to Exanodes." << std::endl;
    out << std::endl;

    Subtitle(out, "Cluster with different disks:");

    out << "Create a Linux cluster named " << Boldify("mycluster")
        << " with nodes " << Boldify("node1") << ", " << Boldify("node2") <<
    " and " << Boldify("node3")
            << " using disk " << Boldify("/dev/sda2") <<
    ", no disk, and disk " <<
    Boldify("/dev/sdb") << ", respectively:" << std::endl;
    out << "      exa_clcreate --license cluster.lic --disk 'node1" <<
    EXA_CONF_SEPARATOR << "/dev/sda2"
                       << " node2 node3" << EXA_CONF_SEPARATOR <<
    "/dev/sdb' mycluster" <<
    std::endl;
    out << std::endl;

    out << "Create a Windows cluster named " << Boldify("mycluster")
        << " with nodes " << Boldify("node1") << ", " << Boldify("node2") <<
    " and " << Boldify("node3")
            << " using disk " << Boldify(
        "\\\\?\\Volume{c0aa39e7-e9ca-4a2c-b423-2071a4af7f2b}")
        << ", no disk, and disk " << Boldify("E:") << ", respectively:" <<
    std::endl;
    out << "      exa_clcreate --license cluster.lic --disk 'node1" <<
    EXA_CONF_SEPARATOR
        << "\\\\?\\Volume{c0aa39e7-e9ca-4a2c-b423-2071a4af7f2b} node2 node3"
        << EXA_CONF_SEPARATOR << "E:' mycluster" << std::endl;
    out << std::endl;

    Subtitle(out, "Cluster with specified data network:");

    out << "Create a Linux cluster named " << Boldify("mycluster")
        << " with " << Boldify("node1") << ", " << Boldify("node2") <<
    " and " << Boldify("node3")
            << " using on each of them disk " << Boldify("/dev/sdb")
            << ", and using the specified hostnames for the data network:"
   <<
    std::endl;
    out << "      exa_clcreate --license cluster.lic --disk node/1-3/" <<
    EXA_CONF_SEPARATOR << "/dev/sdb --datanetwork 'node1:node1-data.mydomain"
                       << " node2:node2-data.mydomain node3:node3-data.mydomain' mycluster" <<
    std::endl;
    out << std::endl;

    out << "Create a Windows cluster named " << Boldify("mycluster")
        << " with " << Boldify("node1") << ", " << Boldify("node2") <<
    " and " << Boldify("node3")
            << " using on each of them disk " << Boldify("E:")
            << ", and using the specified IP addresses for the data network:"
   <<
    std::endl;
    out << "      exa_clcreate --license cluster.lic --disk node/1-3/" <<
    EXA_CONF_SEPARATOR << "E:"
                       <<
    " --datanetwork 'node1:192.168.0.1 node2:192.168.0.2 node3:192.168.0.3' mycluster"
   << std::endl;
    out << std::endl;

    Subtitle(out, "Cluster with specified SPOFs groups:");

    out << "Create a Linux cluster named " << Boldify("mycluster")
        << " with " << Boldify("node1") << ", " << Boldify("node2") << ", "
        << Boldify("node3") << ", " << Boldify("node4") << " and " << Boldify(
        "node5")
        << " using on each of them disk " << Boldify("/dev/sdb")
        << ", and declaring that the simultaneous failure of "
        << Boldify("node1") << " and " << Boldify("node2")
        << " must be supported, as well as the simultaneous failure of "
        << Boldify("node3") << " and " << Boldify("node4")
        << ":" << std::endl;
    out << "      exa_clcreate --license cluster.lic --disk node/1-5/"
        << EXA_CONF_SEPARATOR <<
    "/dev/sdb --spof-group '[node1 node2][node/3-4/][node5]'"
        << " mycluster" << std::endl;
    out << std::endl;
    out << "This is an advanced option. Please refer to the documentation for"
        << " further information." << std::endl;
    out << std::endl;

    /* FIXME Usage help of exa_cldiskadd, exa_cldiskdel, exa_dgcreate,
     * exa_dgdiskrecover should show this note too. (Or, at least, point to
     * this usage help.) */
    Subtitle(out, "Note:");

    out <<
    "On Windows clusters, Exanodes accepts the following notations for disks, where X is a drive letter:"
   << std::endl;
    out << std::endl;
    out << "  X:" << std::endl;
    out << "  \\\\?\\X:" << std::endl;
    out << "  \\\\?\\Volume{...}" << std::endl;
    out << std::endl;

    if (!show_hidden)
        return;

    out << Boldify("File-based cluster creation") << std::endl;
    out << std::endl;
    out << "Create the cluster named " << Boldify("mycluster") <<
    " based on the initialization file " << Boldify("mycluster.init") << ":" <<
    std::endl;
    out << std::endl;
    out << "  " <<
    "exa_clcreate --license cluster.lic --config mycluster.init mycluster" <<
    std::endl;
    out << std::endl;
    out <<
    "Below is a mycluster.init example that creates a 2 nodes Linux cluster."
        << " Each node provides 2 disks to Exanodes (" << Boldify("/dev/sdb")
        << " and " << Boldify("/dev/sdc") << ")." << std::endl;
    out << "A Windows cluster can be created in the same way by using the"
        << " Windows disk notations described above." << std::endl;
    out << std::endl;
    out << "<Exanodes>" << std::endl
        << "  <cluster name='mycluster'>" << std::endl
        << "    <node name='node1' hostname='node1.yourdomain'>" << std::endl
        << "      <network hostname='192.168.8.171'/>" << std::endl
        << "      <disk path='/dev/sdb'/>" << std::endl
        << "      <disk path='/dev/sdc'/>" << std::endl
        << "    </node>" << std::endl
        << "    <node name='node2' hostname='node2.yourdomain'>" << std::endl
        << "      <network hostname='host2.yourdomain'/>" << std::endl
        << "      <disk path='/dev/sdb'/>" << std::endl
        << "      <disk path='/dev/sdc'/>" << std::endl
        << "    </node>" << std::endl
        << "  </cluster>" << std::endl
        << "</Exanodes>" << std::endl;
    out << std::endl;
    out << "Note: The command exa_makeconfig can help you to "
        << "create an initialization file." << std::endl;
}


