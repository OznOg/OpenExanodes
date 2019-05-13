/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_clinfo.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_math.h"
#include "common/include/exa_names.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"
#include "token_manager/tm_client/include/tm_client.h"

#include <iostream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

using std::string;
using std::set;
using std::multiset;

using std::shared_ptr;
using boost::format;
using boost::lexical_cast;

#define NOT_AVAILABLE "N/A"
#define MIN_NODE_COLUMN_WIDTH 11
#define CLINFO_STATUS_WIDTH 12
#define CLINFO_RDEV_STATUS_WIDTH (CLINFO_STATUS_WIDTH + 3)
#define ROOT_DEV  "/dev/exa"

const std::string exa_clinfo::OPT_ARG_ONLY_ITEM(Command::Boldify("ITEM"));
const std::string exa_clinfo::OPT_ARG_WRAPPING_N(Command::Boldify("N"));

/* Comparison operators */

struct exa_cmp_xmlnode_name_lt
{
    bool operator ()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
    {
        const char *c1node(xml_get_prop((xmlNodePtr) item1, "node"));
        const char *c2node(xml_get_prop((xmlNodePtr) item2, "node"));
        const char *c1name(xml_get_prop((xmlNodePtr) item1, "path"));
        const char *c2name(xml_get_prop((xmlNodePtr) item2, "path"));

        std::string qc1 = std::string(c1node);

        if (c1name != NULL)
            qc1 += c1name;
        std::string qc2 = std::string(c2node);
        if (c2name != NULL)
            qc2 += c2name;

        const char *c1 = qc1.c_str();
        const char *c2 = qc2.c_str();

        EXA_ASSERT(c1);
        EXA_ASSERT(c2);

        return os_strverscmp(c1, c2) < 0;
    }
};

struct exa_cmp_xmlparentnode_name_lt
{
    bool operator ()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
    {
        const char *c1node(xml_get_prop(((xmlNodePtr) item1)->parent, "name"));
        const char *c2node(xml_get_prop(((xmlNodePtr) item2)->parent, "name"));
        const char *c1name(xml_get_prop((xmlNodePtr) item1, "path"));
        const char *c2name(xml_get_prop((xmlNodePtr) item2, "path"));

        std::string qc1 = std::string(c1node);

        if (c1name != NULL)
            qc1 += c1name;
        std::string qc2 = std::string(c2node);
        if (c2name != NULL)
            qc2 += c2name;

        const char *c1 = qc1.c_str();
        const char *c2 = qc2.c_str();

        EXA_ASSERT(c1);
        EXA_ASSERT(c2);

        return os_strverscmp(c1, c2) < 0;
    }
};

/* Sort a volume on its full name including its group like group:vol1 */
struct exa_cmp_xmlnode_volume_lt
{
    bool operator ()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
    {
        const char *vol1(xml_get_prop((xmlNodePtr) item1, "name"));
        const char *grp1(xml_get_prop((xmlNodePtr) item1->parent->parent,
                                      "name"));
        const char *vol2(xml_get_prop((xmlNodePtr) item2, "name"));
        const char *grp2(xml_get_prop((xmlNodePtr) item2->parent->parent,
                                      "name"));

        if (os_strverscmp(grp1, grp2))
            return os_strverscmp(grp1, grp2) < 0;
        else
            return os_strverscmp(vol1, vol2) < 0;
    }
};

/* Sort a filesystem on its full name including its group like group:fs1 */
struct exa_cmp_xmlnode_fs_lt
{
    bool operator ()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
    {
        const char *fs1(xml_get_prop((xmlNodePtr) item1->parent, "name"));
        const char *grp1(xml_get_prop(
                             (xmlNodePtr) item1->parent->parent->parent,
                             "name"));
        const char *fs2(xml_get_prop((xmlNodePtr) item2->parent, "name"));
        const char *grp2(xml_get_prop(
                             (xmlNodePtr) item2->parent->parent->parent,
                             "name"));

        if (os_strverscmp(grp1, grp2))
            return os_strverscmp(grp1, grp2) < 0;
        else
            return os_strverscmp(fs1, fs2) < 0;
    }
};

exa_clinfo::exa_clinfo()
    : volumes_info(true)
    , groups_info(true)
    , disks_info(true)
#ifdef WITH_FS
    , filesystems_info(true)
    , display_filesystem_size(false)
#endif
    , softwares_info(true)
    , wrapping_in_nodes(EXA_CLI_MAX_NODE_PER_LINE)
    , xml_dump(false)
    , show_unassigned_rdev(false)
    , force_kilo(false)
    , display_group_config(false)
    , iscsi_details(false)
{
    add_option('g', "group", "Display only groups information.",
               0, false, false);
    add_option('G', "group-config", "Display internal configuration of groups",
               0, true, false);
    add_option('l', "volume", "Display only volumes information.",
               0, false, false);
    add_option('i', "disk", "Display only disks information.",
               0, false, false);
    add_option('D', "iscsi-details", "Display details about iSCSI connections.",
               0, false, false);
    add_option('S', "software", "Display only software status.",
               0, true, false);
#ifdef WITH_FS
    add_option('f', "filesystem", "Display only file systems information.",
               0, false, false);
    add_option('F', "filesystemsize", "Display the size of the file systems.\n"
               "WARNING: this may be very long.", 0, false, false);
#endif
    add_option('k', "kilo", "Display all capacities in kibibyte (1024 bytes).",
               0, false, false);
    add_option('w', "wrapping", "Wrap to display N nodes per line.\n"
               "N=0 disables line wrapping.", 0, false, true,
               OPT_ARG_WRAPPING_N);
    add_option('o', "only", "Display only the specified " + OPT_ARG_ONLY_ITEM +
               ". This needs one of the following options: -f, -g, -l, -i.",
               0, true, true, OPT_ARG_ONLY_ITEM);
    add_option('u', "unassigned", "Show disks which are not assigned to any "
               "disk group.", 0, false, false);
    add_option('x', "xml", "Display the raw xml tree on stdout instead of the "
               "regular display. This option also returns the error code "
               "returned by the administrative daemon as the exit code.",
               0, true, false);
}


void exa_clinfo::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
    add_see_also("exa_clstart");
    add_see_also("exa_clstop");
    add_see_also("exa_clstats");
    add_see_also("exa_cltune");
    add_see_also("exa_clreconnect");
}


/**
 * Display a warning that a recovery is in progress.
 */
static void warn_recovering(void)
{
    exa_cli_warning(
        "%sWARNING%s: A recovery is in progress on your cluster.\n"
        "         You should run again exa_clinfo to get an updated view.\n\n",
        COLOR_WARNING,
        COLOR_NORM);
}


void exa_clinfo::run()
{
    string error_msg;
    bool in_recovery;
    exa_error_code error_code;

    if (exa.set_cluster_from_cache(_cluster_name.c_str(),
                                   error_msg) != EXA_SUCCESS)
        throw CommandException(error_msg);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());
    exa_cli_trace("filter [%s%s%s%s%s%s]\n",
                  softwares_info ? "s" : "",
                  volumes_info ? "v" : "",
                  groups_info ? "g" : "",
                  disks_info ? "d" : "",
#ifdef WITH_FS
                  filesystems_info ? "f" : "",
                  display_filesystem_size ? "z" : "");
#else
                  "", "");
#endif
    exa_cli_trace("extra [%d]\n", force_kilo);

    /* If a testfile is provided, don't get it from the cluster */
    if (!config_ptr)
        config_ptr = get_config(error_code, in_recovery);
    else
        in_recovery = false;

    if (xml_dump)
    {
        xmlChar *buf;

        if (config_ptr)
        {
            xmlDocDumpFormatMemory(config_ptr.get(), &buf, NULL, 1);
            /* Do not use exa_cli_*, user really expect output whatever the
             * trace level */
            printf("%s\n", buf);
            xmlFree(buf);
        }

        throw CommandException(error_code);
    }
    else
    {
        if (!config_ptr)
            throw CommandException(EXA_ERR_DEFAULT);

        if (in_recovery)
            warn_recovering();

        exa_display_clinfo(config_ptr);

        if (in_recovery)
            warn_recovering();
    }

    if (config_ptr == NULL)
        throw CommandException(EXA_ERR_DEFAULT);
}


/** \brief this function contact an admind and returns the clinfo tree.
 *
 * \param[in] filter: an exa_admind_getconfigfilter_t filter used to
 *            ask admind to update only a subset to the tree. By
 *            default, CLINFO_FILTER_ALL is used
 * \param[out] in_recovery: returns the in_recovery state of the command.
 *
 * \note In case of failure, an exit is done with the proper exit
 *       code and error message.
 *
 * \return The remote config tree or NULL (an error message is displayed).
 */
shared_ptr<xmlDoc> exa_clinfo::get_config(exa_error_code &error_code,
                                          bool &in_recovery)
{
    string error_message;

    in_recovery = false;

    AdmindCommand command("clinfo", exa.get_cluster_uuid());

    /* We always need software information to display NODEUP/DOWN */
    command.add_param("softwares_info", true);
    command.add_param("disks_info", disks_info);
#ifdef WITH_FS
    command.add_param("filesystems_info", filesystems_info);
    command.add_param("filesystems_size_info", display_filesystem_size);
#endif

    if (groups_info ||
#ifdef WITH_FS
        filesystems_info ||
#endif
        disks_info)
        command.add_param("groups_info", true);

    /* Since group also display the number of volumes,
     * set the volume flags in this case.  */
    command.add_param("volumes_info", volumes_info);

    shared_ptr<AdmindMessage> message(send_command(command, "", error_code,
                                                   error_message));

    if (!message)
    {
        exa_cli_error("%sERROR%s: You can run exa_clinfo only on a started "
                      "cluster.\n       Please fix previous error first if "
                      "any and run exa_clstart.\n", COLOR_ERROR, COLOR_NORM);

        return shared_ptr<xmlDoc>();
    }
    else if (get_error_type(error_code) != ERR_TYPE_SUCCESS)
        throw CommandException("Failed to receive a valid response from admind",
                               error_code);

    shared_ptr<xmlDoc> cfg(
        xmlReadMemory(
            (*message).get_payload().c_str(),
            (*message).get_payload().size(),
            NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOERROR |
            XML_PARSE_NOWARNING),
        xmlFreeDoc);

    if (!cfg || !cfg->children || !cfg->children->name
        || !xmlStrEqual(cfg->children->name, BAD_CAST("Exanodes")))
        throw CommandException(
            "Failed to parse admind returned initialization file");

    EXA_ASSERT_VERBOSE(cfg->children->children,
                       "No cluster element found in the tree");

    if (strcmp(xml_get_prop(cfg->children->children,
                            "in_recovery"), ADMIND_PROP_TRUE) == 0)
        in_recovery = true;

    if (exa.update_cache_from_config(cfg.get(), error_message) != EXA_SUCCESS)
        exa_cli_warning("%sWARNING%s: %s\n", COLOR_WARNING, COLOR_NORM,
                        error_message.c_str());

    return cfg;
}


/** \brief Display status (for disk group or disk).
 *
 * \param[in] stat: status value to display.
 * \param[in] width: the minimum width to write the status in
 */
static void displayStatus(string stat, uint width)
{
    if (stat.empty())
        exa_cli_info("%-*s", width, "UNKNOWN");
    else if (stat == ADMIND_PROP_STOPPED)
        exa_cli_info("%-*s", width,
                     stat.c_str());
    else if (stat == ADMIND_PROP_OK)
        exa_cli_info("%s%-*s%s", COLOR_INFO, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_OFFLINE)
        exa_cli_info("%s%-*s%s", COLOR_ERROR, width,
                     stat.c_str(), COLOR_NORM);
    else if (boost::algorithm::find_first(stat, ADMIND_PROP_DEGRADED))
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (boost::algorithm::find_first(stat, ADMIND_PROP_REBUILDING))
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (boost::algorithm::find_first(stat, ADMIND_PROP_UPDATING))
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (boost::algorithm::find_first(stat, ADMIND_PROP_REPLICATING))
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_ALIEN)
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_MISSING)
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_DOWN)
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_OUTDATED)
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_BLANK)
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == ADMIND_PROP_UP)
        exa_cli_info("%s%-*s%s", COLOR_INFO, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == "BROKEN")
        exa_cli_info("%s%-*s%s", COLOR_ERROR, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == "INVALID")
        exa_cli_info("%s%-*s%s", COLOR_ERROR, width,
                     stat.c_str(), COLOR_NORM);
    else if (stat == "USING SPARE")
        exa_cli_info("%s%-*s%s", COLOR_WARNING, width,
                     stat.c_str(), COLOR_NORM);
    else
        exa_cli_info("%-*s", width, stat.c_str());

}


/** \brief Test if the node is Up.
 *
 * \param[in] config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in] node: node name tested.
 * \return 0/1 DOWN/UP.
 */
static int isNodeUp(shared_ptr<xmlDoc> config_doc_ptr, const char *node)
{
    xmlNodePtr node_ptr;

    node_ptr = xml_conf_xpath_singleton(
        config_doc_ptr.get(),
        "//software/node[@name='%s' and @status='%s']",
        node, ADMIND_PROP_UP);
    return node_ptr != NULL;
}


/** \brief Test if a transaction is "INPROGRESS"
 *
 * \param[in] transaction: the transaction parameter of the volume or
 *                         file system in Admind's response.
 * \return true if the volume or file system transaction is in-progress.
 */
static bool isInProgress(const char *transaction)
{
    if (strcmp(transaction, ADMIND_PROP_INPROGRESS) == 0)
        return true;
    else
        return false;
}

/** \brief Display token manager info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodeslist: The list of the nodes
 *
 */
void exa_clinfo::exa_display_token_manager(shared_ptr<xmlDoc> config_doc_ptr,
                                           set<xmlNodePtr,
                                               exa_cmp_xmlnode_lt> &nodelist)
{
    bool configured_nodes = false;
    bool unconfigured_nodes = false;
    bool disconnected_nodes = false;
    std::set<std::string> tm_unconfigured_list;
    std::set<std::string> tm_disconnected_list;

    for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = nodelist.begin();
         it != nodelist.end(); ++it)
    {
        const char *node = xml_get_prop(*it, "name");

        if (isNodeUp(config_doc_ptr, node))
        {
            xmlNodePtr tm_ptr =
                xml_conf_xpath_singleton(
                    config_doc_ptr.get(),
                    "//software/node[@name='%s']/component[@name='%s']",
                    node, "token_manager");

            if (tm_ptr == NULL)
                continue;

            bool configured = strcmp(xml_get_prop(tm_ptr, "configured"),
                                      ADMIND_PROP_TRUE) == 0;
            const char *status_str = xml_get_prop(tm_ptr, "status");

            if (!configured)
            {
                unconfigured_nodes = true;
                tm_unconfigured_list.insert(node);
            }
            else
                configured_nodes = true;

            if (strcmp(status_str, "NOT CONNECTED") == 0)
            {
                disconnected_nodes = true;
                tm_disconnected_list.insert(node);
            }
        }
    }

    /* If no node has a TM configured, it's probably on purpose. */
    if (!configured_nodes)
        return;

    exa_cli_info("%-16s", "TOKEN MANAGER: ");

    if (disconnected_nodes || unconfigured_nodes)
        exa_cli_warning("%sWARNING%s", COLOR_WARNING, COLOR_NORM);
    else
        exa_cli_info("%sOK%s", COLOR_INFO, COLOR_NORM);

    if (disconnected_nodes)
    {
        string resultstring = str(
            format("Not connected on %s")
            % display_nodes(tm_disconnected_list, 0));

        exa_cli_info("\n\t\t%s", resultstring.c_str());
    }

    if (unconfigured_nodes)
    {
        string resultstring = str(
            format("Unconfigured token manager on %s")
            % display_nodes(tm_unconfigured_list, 0));

        exa_cli_info("\n\t\t%s", resultstring.c_str());
    }

    exa_cli_info("\n");
}


/** \brief Display clinfo.
 *
 * \param[in] config_doc_ptr: The doc xml tree of the response from admind.
 *
 */
void exa_clinfo::exa_display_clinfo(shared_ptr<xmlDoc> config_doc_ptr)
{
    xmlNodePtr nodeptr;

    set<xmlNodePtr, exa_cmp_xmlnode_lt> nodelist;
    set<xmlNodePtr, exa_cmp_xmlnode_lt> grouplist;
    int i;

    xmlNodeSetPtr nodesSet = xml_conf_xpath_query(config_doc_ptr.get(),
                                                  "//cluster/node");

    xml_conf_xpath_result_for_each(nodesSet, nodeptr, i)
    nodelist.insert(nodeptr);

    xmlNodeSetPtr groupsSet = xml_conf_xpath_query(config_doc_ptr.get(),
                                                   "//diskgroup");

    xml_conf_xpath_result_for_each(groupsSet, nodeptr, i)
    grouplist.insert(nodeptr);

    int nbNodes = nodelist.size();
    int nbGroups = grouplist.size();

    xmlNodePtr clusterPtr = xml_conf_xpath_singleton(config_doc_ptr.get(),
                                                     "//cluster[@name]");

    init_column_width(config_doc_ptr);

    exa_cli_info("CLUSTER '%s': %d NODES, %d GROUPS\n",
                 xml_get_prop(clusterPtr, "name"), nbNodes, nbGroups);

    { /* Display the list of nodes */
        std::set<std::string> node_list;
        for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = nodelist.begin();
             it != nodelist.end(); ++it)
        {
            const char *node = xml_get_prop(*it, "name");
            node_list.insert(node);
        }

        exa_cli_info("%-16s%s\n", "NODES:", display_nodes(node_list, 16).c_str());
    }

    if (softwares_info)
    {
        bool license_has_ha = (strcmp(xml_get_prop(clusterPtr, "license_has_ha"),
                                      ADMIND_PROP_TRUE) == 0);

        exa_display_softs(config_doc_ptr, nodelist, license_has_ha);
#ifdef WITH_MONITORING
        exa_display_monitoring(config_doc_ptr);
#endif

        exa_display_token_manager(config_doc_ptr, nodelist);

        exa_cli_info("\n");
    }

#ifdef WITH_FS
    if (display_filesystem_size | filesystems_info)
        /* gulm infos related to filesystems, display them in conjunction with fs infos */
        exa_display_gulm_nodes(config_doc_ptr);

#endif

    if (disks_info)
        exa_display_rdevs(config_doc_ptr);

    if (groups_info)
        exa_display_groups(config_doc_ptr, grouplist);

    if (display_group_config)
        exa_display_group_configurations(grouplist);

    if (volumes_info)
        exa_display_volumes_status(config_doc_ptr, nodelist);

#ifdef WITH_FS
    if (display_filesystem_size | filesystems_info)
        exa_display_fs_status(config_doc_ptr, nodelist);

#endif

    xml_conf_xpath_free(nodesSet);
    xml_conf_xpath_free(groupsSet);

    string license_status_str = xml_get_prop(clusterPtr, "license_status");
    string license_remaining_days_str = xml_get_prop(clusterPtr,
                                                     "license_remaining_days");
    string license_type = xml_get_prop(clusterPtr, "license_type");
    uint64_t license_remaining_days = lexical_cast<uint64_t>(
        license_remaining_days_str);

    if (license_status_str == "expired")
        exa_cli_warning(
            "\n%sWARNING%s: Your Exanodes %slicense has expired.\n "
            "               You cannot use start and create commands anymore.\n\n",
            COLOR_WARNING,
            COLOR_NORM,
            license_type == "eval" ? "evaluation " : "");
    else if (license_type == "eval")
        exa_cli_warning(
            "\n%sWARNING%s: Your Exanodes license is an evaluation license.\n "
            "               The service will be interrupted in %"
            PRIu64 " days.\n\n",
            COLOR_WARNING,
            COLOR_NORM,
            license_remaining_days);
    else if (license_status_str == "grace")
        exa_cli_warning(
            "\n%sWARNING%s: Your Exanodes license has recently expired.\n "
            "               The service will be interrupted in %"
            PRIu64 " days.\n\n",
            COLOR_WARNING,
            COLOR_NORM,
            license_remaining_days);
    else if (license_status_str == "(none)")
        exa_cli_info(
            "\n%sWARNING%s: Using Exanodes without license.\n "
            "              You cannot use start and create commands.\n\n",
            COLOR_WARNING,
            COLOR_NORM);
    else if (license_status_str == "ok")
        ;
    else
        EXA_ASSERT_VERBOSE(false,
                           "Unexpected license status '%s'",
                           license_status_str.c_str());
}


/** \brief Display softs info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodeslist: The list of the nodes
 *
 */
void exa_clinfo::exa_display_softs(shared_ptr<xmlDoc> config_doc_ptr,
                                   set<xmlNodePtr,
                                       exa_cmp_xmlnode_lt> &nodelist,
                                   bool license_has_ha)
{
    std::set<std::string> downnode_list = get_downnodes(config_doc_ptr,
                                                        nodelist);
    string resultstring;

    exa_cli_info("%-16s", "SOFTWARE: ");

    /*
     * Display Daemons infos
     */
    exa_display_daemons(config_doc_ptr, nodelist, resultstring);

    /*
     * Display modules infos
     */
    exa_display_modules(config_doc_ptr, nodelist, resultstring);

    /*
     * Display nodes down
     */
    if (downnode_list.size() == 0 && resultstring.empty())
        exa_cli_info("%sOK%s", COLOR_INFO, COLOR_NORM);
    else
    {
        exa_cli_warning("%sWARNING%s", COLOR_WARNING, COLOR_NORM);

        if (downnode_list.size() == 1)
            exa_cli_info("\n %-29s %s",
                         str(format(
                                 "%d node down") % downnode_list.size()).c_str(),
                         display_nodes(downnode_list, 31).c_str());
        else if (downnode_list.size())
            exa_cli_info("\n %-29s %s",
                         str(format(
                                 "%d nodes down") % downnode_list.size()).c_str(),
                         display_nodes(downnode_list, 31).c_str());

        if (!resultstring.empty())
            exa_cli_info("%s", resultstring.c_str());
    }

    if (license_has_ha)
        exa_cli_info("\n\t\t%sHA is enabled%s", COLOR_FEATURE, COLOR_NORM);

    exa_cli_info("\n");

    /*
     * Display info about SFS management
     */
    xmlNodePtr nodePtr =
        xml_conf_xpath_singleton(
            config_doc_ptr.get(),
            "//software/node[@status='%s']/node[@handle_sfs='%s']",
            ADMIND_PROP_UP, ADMIND_PROP_NOK);
    if (nodePtr)
    {
        exa_cli_info("%-16s", "SFS: ");
        exa_cli_info("%sNEED CLUSTER REBOOT%s\n",
                     COLOR_ERROR, COLOR_NORM);
    }
}


#ifdef WITH_MONITORING
/** \brief Display monitoring info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 *
 */
void exa_clinfo::exa_display_monitoring(shared_ptr<xmlDoc> config_doc_ptr)
{
    xmlNodePtr nodeSnmpPtr =
        xml_conf_xpath_singleton(config_doc_ptr.get(),
                                 "//cluster/monitoring");

    if (nodeSnmpPtr)
    {
        const string snmpd_host = xml_get_prop(nodeSnmpPtr,
                                               EXA_CONF_MONITORING_SNMPD_HOST);
        const string snmpd_port = xml_get_prop(nodeSnmpPtr,
                                               EXA_CONF_MONITORING_SNMPD_PORT);
        const string monitoring_started_on = xml_get_prop(
            nodeSnmpPtr,
            EXA_CONF_MONITORING_STARTED_ON);
        const string monitoring_stopped_on = xml_get_prop(
            nodeSnmpPtr,
            EXA_CONF_MONITORING_STOPPED_ON);

        exa_cli_info("MONITORING:\n");

        if (!monitoring_started_on.empty())
            exa_cli_info("%-*s%s%-*s%s %s\n",
                         16, "",
                         COLOR_INFO,
                         CLINFO_STATUS_WIDTH,
                         "STARTED   ",
                         COLOR_NORM,
                         monitoring_started_on.c_str());

        if (!monitoring_stopped_on.empty())
            exa_cli_info("%-*s%s%-*s%s %s\n",
                         16, "",
                         COLOR_NORM,
                         CLINFO_STATUS_WIDTH,
                         "WILL START",
                         COLOR_NORM,
                         monitoring_stopped_on.c_str());

    }
}


#endif

/**
 * Friendly display of a total size along with a used size.
 *
 * @param[in] sizeT             The total size
 * @param[in] sizeU             The used size
 * @param[in] disk_count        Number of disks
 * @param[in] rdev_info_maxlen  Maximum length of a displayed rdev info
 *                              (plus spof group)
 */
void exa_clinfo::display_total_used(uint64_t sizeT, uint64_t sizeU,
                                    uint disk_count, uint rdev_info_maxlen)
{
    string mant, unit;
    char line[EXA_MAXSIZE_LINE];
    int spacing = rdev_info_maxlen - strlen("TOTAL 01234 ");

    format_hum_friend(sizeT, mant, unit, force_kilo);
    exa_cli_info("   TOTAL %5u %-*s %5s%s", disk_count, spacing, "disks",
                 mant.c_str(), unit.c_str());

    EXA_ASSERT_VERBOSE(
        sizeT >= sizeU,
        "Should not happens, check disk size returned by admind\n");
    format_hum_friend(sizeT - sizeU, mant, unit, force_kilo);
    if (sizeT)
    {
        os_snprintf(line,
                    sizeof(line),
                    "(%.0f%% Used)",
                    sizeU * 100 / (double) sizeT);
        exa_cli_info(" %-11s", line);
    }
    exa_cli_info("\n");
}


/**
 * Friendly display of a total size alone (no used size).
 *
 * @param[in] sizeT             The total size
 * @param[in] disk_count        Number of disks
 * @param[in] rdev_info_maxlen  Maximum length of a displayed rdev info
 */
void exa_clinfo::display_total(uint64_t sizeT, uint disk_count,
                               uint rdev_info_maxlen)
{
    string mant, unit;
    int spacing = rdev_info_maxlen - strlen("TOTAL 01234 ");

    format_hum_friend(sizeT, mant, unit, force_kilo);
    exa_cli_info("   TOTAL %5u %-*s %5s%s\n", disk_count, spacing, "disks",
                 mant.c_str(), unit.c_str());
}


static uint get_max_rdev_info_len(shared_ptr<xmlDoc> config_doc_ptr)
{
    xmlNodePtr devptr;
    xmlNodeSetPtr devptr_list;
    uint rdev_info_maxlen = 50;
    int i;

    devptr_list = xml_conf_xpath_query(config_doc_ptr.get(),
                                       "//cluster/node[@name]/disk");

    xml_conf_xpath_result_for_each(devptr_list, devptr, i)
    {
        const char *rdev = xml_get_prop(devptr, "uuid");
        const char *path = xml_get_prop(devptr, "path");
        const char *node = xml_get_prop(devptr->parent, "name");
        string rdev_info;

        rdev_info = string(rdev) + " (" + node;
        if (path)
        {
            rdev_info += EXA_CONF_SEPARATOR;
            rdev_info += path;
        }
        rdev_info += ")";

        if (rdev_info.size() > rdev_info_maxlen)
            rdev_info_maxlen = rdev_info.size();
    }
    xml_conf_xpath_free(devptr_list);

    return rdev_info_maxlen;
}


/** \brief Display global rdev info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodesSet: The list of the nodes in xml format.
 *
 */
void exa_clinfo::exa_display_rdevs(shared_ptr<xmlDoc> config_doc_ptr)
{
    bool display_only_error = true;
    bool got_error;
    xmlNodePtr devptr_grp;
    xmlNodeSetPtr devptr_grp_list;

    set<xmlNodePtr, exa_cmp_xmlnode_lt> grplist;
    int i;
    int rdev_info_maxlen = get_max_rdev_info_len(config_doc_ptr);

    /* FIXME what does this test stands for... */
    if (disks_info
        && !volumes_info
        && !groups_info
#ifdef WITH_FS
        && !filesystems_info
        && !display_filesystem_size
#endif
        && !softwares_info)
        display_only_error = false;

    xmlNodeSetPtr disk_set =
        xml_conf_xpath_query(config_doc_ptr.get(),
                             "//cluster/node[@name]/disk");
    if (xml_conf_xpath_result_entity_count(disk_set) == 0)
    {
        xml_conf_xpath_free(disk_set);
        exa_cli_info("DISKS: None\n\n");
        return;
    }

    xml_conf_xpath_free(disk_set);

    exa_cli_info("DISKS:\n");

    devptr_grp_list = xml_conf_xpath_query(config_doc_ptr.get(),
                                           "//diskgroup");
    xml_conf_xpath_result_for_each(devptr_grp_list, devptr_grp, i)
    grplist.insert(devptr_grp);

    xml_conf_xpath_free(devptr_grp_list);

    for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = grplist.begin();
         it != grplist.end(); ++it)
    {
        xmlNodePtr devptr_grp = *it;
        bool sizeUavailable = false;
        const char *group = xml_get_prop(devptr_grp, "name");
        string group_status = xml_get_prop(devptr_grp, "status");
        multiset<xmlNodePtr, exa_cmp_xmlnode_name_lt> devlist;
        bool group_is_rebuilding = false;
        uint64_t sizeT = 0;
        uint64_t sizeU = 0;
        xmlNodePtr devptr;
        xmlNodeSetPtr devptr_list;
        uint disk_count = 0;

        if (xml_get_prop(devptr_grp, "rebuilding") &&
            strcmp(xml_get_prop(devptr_grp,
                                "rebuilding"), ADMIND_PROP_TRUE) == 0)
            group_is_rebuilding = true;

        exa_cli_info(" %-*s ", 14, group);

        devptr_list = xml_conf_xpath_query(
            config_doc_ptr.get(),
            "//diskgroup[@name='%s']/physical/disk[@uuid]",
            group);
        xml_conf_xpath_result_for_each(devptr_list, devptr, i)
        devlist.insert(devptr);

        xml_conf_xpath_free(devptr_list);

        got_error = false;

        /* compute rdevpath vector, remembering string maxlen for display */
        std::vector<std::string> rdevpaths;

        for (multiset<xmlNodePtr, exa_cmp_xmlnode_name_lt>::iterator it =
                 devlist.begin();
             it != devlist.end(); ++it)
        {
            xmlNodePtr devptr =  *it;
            const char *node = xml_get_prop(devptr, "node");
            const char *rdev = xml_get_prop(devptr, "uuid");
            xmlNodePtr node_disk_ptr =
                xml_conf_xpath_singleton(
                    config_doc_ptr.get(),
                    "//cluster/node/disk[@uuid='%s']",
                    rdev);

            const char *__path = xml_get_prop(node_disk_ptr, "path");
            string path = __path ? std::string(__path) : "";

            std::string rdevpath(string(rdev) + " (" + node);
            if (!path.empty())
                rdevpath += EXA_CONF_SEPARATOR + path;
            rdevpath += ")";

            rdevpaths.push_back(rdevpath);
        }

        i = 0;
        for (multiset<xmlNodePtr, exa_cmp_xmlnode_name_lt>::iterator it =
                 devlist.begin();
             it != devlist.end(); ++it)
        {
            xmlNodePtr devptr =  *it;
            const char *node = xml_get_prop(devptr, "node");
            const char *size = xml_get_prop(devptr, "size");
            const char *sizeused = xml_get_prop(devptr, "size_used");
            const char *rdev = xml_get_prop(devptr, "uuid");
            xmlNodePtr node_ptr =
                xml_conf_xpath_singleton(config_doc_ptr.get(),
                                         "//cluster/node[@name='%s']", node);
            const char *spof_id = xml_get_prop(node_ptr, "spof_id");
            xmlNodePtr node_disk_ptr = xml_get_child(node_ptr,
                                                     "disk",
                                                     "uuid",
                                                     rdev);

            std::string rdevpath = rdevpaths.at(i++);

            if (!filter_only.empty() &&
                filter_only != rdevpath)
                continue;

            EXA_ASSERT_VERBOSE(node_disk_ptr,
                               "cluster level rdev information not available");

            disk_count++;

            string statrdev = xml_get_prop(node_disk_ptr, "status");
            string statgdev;

            /* statgdev may be empty if the vrt is stopped */
            if (!xml_get_prop(devptr, "status"))
                statgdev = "";
            else
                statgdev = xml_get_prop(devptr, "status");

            if (display_only_error &&
                statrdev == ADMIND_PROP_UP &&
                (statgdev == ADMIND_PROP_OK ||
                 statgdev.empty()))
                continue;

            if (!got_error)
                exa_cli_info("\n");

            got_error = true;

            if (size == NULL)
            {
                /* Hum, no size, probably the group is stopped display the
                 * cluster->node->disk size instead
                 */
                size = xml_get_prop(node_disk_ptr, "size");
                sizeused = NULL;
            }

            /* rdev name */
            exa_cli_info("   %-*s", rdev_info_maxlen, rdevpath.c_str());

            string spof(std::string("spof group ") + spof_id);
            exa_cli_info("  %-*s", static_cast<int>(strlen(
                                                        "spof group ")) + 3,
                         spof.c_str());

            /* Size */
            uint64_t isize = 0;
            bool ret;
            if (size)
            {
                ret = conv_Qstr2u64(size, &isize);
                if (ret)
                {
                    string mant, unit;
                    format_hum_friend(isize, mant, unit, force_kilo);
                    if (isize)
                        exa_cli_info(" %5s%s ", mant.c_str(), unit.c_str());
                    else
                        exa_cli_info(" %6s ", "");

                    sizeT += isize;

                    /* Size used */
                    if (sizeused)
                    {
                        uint64_t isizeu = 0;
                        sizeUavailable = true;
                        ret = conv_Qstr2u64(sizeused, &isizeu);
                        if (ret)
                        {
                            if (isize > 0)
                                exa_cli_info("%-11s ",
                                             str(format(
                                                     "(%.0f%% Used)") %
                                                 (isizeu * 100 /
                                                  (double) isize)).c_str());
                            else
                                exa_cli_info("%11s ", "");

                            sizeU += isizeu;
                        }
                    }
                    else
                        exa_cli_info("%11s ", "");
                }
            }
            else
                exa_cli_info("%19s ", "");

            /* Display the disk status */
            if (statrdev == ADMIND_PROP_UP &&
                group_status != ADMIND_PROP_STOPPED)
            {
                /* Display the rebuild percent progress if needed */
                if (group_is_rebuilding)
                {
                    string rebuilt_size_str    = xml_get_prop(devptr,
                                                              "rebuilt_size");
                    string size_to_rebuild_str = xml_get_prop(devptr,
                                                              "size_to_rebuild");

                    uint64_t size_to_rebuild = lexical_cast<uint64_t>(
                        size_to_rebuild_str);
                    uint64_t rebuilt_size    = lexical_cast<uint64_t>(
                        rebuilt_size_str);

                    if (size_to_rebuild > 0)
                    {
                        int progress = rebuilt_size * 100 / size_to_rebuild;
                        /* FIXME: progress can grow *over* 100 %. As a workaround,
                         * we use min() to set its maximum value to 99 % (see bug #2270).
                         */
                        progress = std::min(progress, 99);
                        statgdev += str(format(" %d%%") % progress);
                    }
                }

                displayStatus(statgdev, CLINFO_RDEV_STATUS_WIDTH);
            }
            else
                displayStatus(statrdev, CLINFO_RDEV_STATUS_WIDTH);

            exa_cli_info("\n");
        }

        /* Display TOTAL */
        if (!display_only_error)
        {
            if (sizeUavailable)
                display_total_used(sizeT, sizeU, disk_count, rdev_info_maxlen +
                                   (int) strlen("spof group ") + 5);
            else
                display_total(sizeT,
                              disk_count,
                              rdev_info_maxlen + (int) strlen(
                                  "spof group ") + 5);
        }
        else if (!got_error)
            exa_cli_info("%sOK%s\n", COLOR_INFO, COLOR_NORM);
    }

    /* Display unassigned rdev */
    {
        multiset<xmlNodePtr, exa_cmp_xmlparentnode_name_lt> devlist;
        bool got_one = false;
        uint64_t sizeT = 0;
        xmlNodePtr devptr;
        xmlNodeSetPtr devptr_list;
        uint disk_count = 0;

        devptr_list = xml_conf_xpath_query(config_doc_ptr.get(),
                                           "//cluster/node[@name]/disk");

        xml_conf_xpath_result_for_each(devptr_list, devptr, i)
        devlist.insert(devptr);

        xml_conf_xpath_free(devptr_list);

        for (multiset<xmlNodePtr, exa_cmp_xmlparentnode_name_lt>::iterator it =
                 devlist.begin();
             it != devlist.end(); ++it)
        {
            xmlNodePtr devptr =  *it;
            const char *rdev = xml_get_prop(devptr, "uuid");
            const char *path = xml_get_prop(devptr, "path");
            const char *size = xml_get_prop(devptr, "size");
            string stat = xml_get_prop(devptr, "status");
            const char *node = xml_get_prop(devptr->parent, "name");
            xmlNodePtr node_ptr =
                xml_conf_xpath_singleton(config_doc_ptr.get(),
                                         "//cluster/node[@name='%s']", node);
            const char *spof_id = xml_get_prop(node_ptr, "spof_id");
            string rdev_info;

            /* XXX Duplicated from loop calculating max rdev info length (see above) */
            rdev_info = string(rdev) + " (" + node;
            if (path)
            {
                rdev_info += EXA_CONF_SEPARATOR;
                rdev_info += path;
            }
            rdev_info += ")";

            /* FIXME What is this for? */
            if (!filter_only.empty() &&
                filter_only != rdev_info)
                continue;

            /* Skip assigned rdev */
            if (xml_conf_xpath_singleton(config_doc_ptr.get(),
                                         "//diskgroup[@name]/physical/disk[@uuid='%s']",
                                         rdev))
                continue;

            disk_count++;

            if (!got_one)
            {
                exa_cli_info(" UNASSIGNED\n");
                if (!show_unassigned_rdev)
                    exa_cli_info(
                        "  Hidden, please use --unassigned to display them all.\n");
                got_one = true;
            }

            if (!show_unassigned_rdev && stat == ADMIND_PROP_UP)
                continue;

            /* rdev info */
            exa_cli_info("   %-*s", rdev_info_maxlen, rdev_info.c_str());

            string spof(std::string("spof group ") + spof_id);
            exa_cli_info("  %-*s", static_cast<int>(strlen(
                                                        "spof group ")) + 3,
                         spof.c_str());

            /* Size */
            uint64_t isize = 0;
            bool ret;
            if (size)
            {
                ret = conv_Qstr2u64(size, &isize);
                if (ret)
                {
                    string mant, unit;
                    format_hum_friend(isize, mant, unit, force_kilo);
                    if (isize)
                        exa_cli_info(" %5s%s ", mant.c_str(), unit.c_str());
                    else
                        exa_cli_info(" %6s ", "");

                    /* Space lost for size used */
                    exa_cli_info("%11s ", "");

                    sizeT += isize;
                }
            }
            else
                exa_cli_info("%19s ", "");

            displayStatus(stat, CLINFO_RDEV_STATUS_WIDTH);

            exa_cli_info("\n");
        }

        /* Display TOTAL */
        if (got_one && show_unassigned_rdev)
            display_total(sizeT, disk_count, rdev_info_maxlen +
                          (int) strlen("spof group ") + 5);
    }

    exa_cli_info("\n");
}


/** \brief Initialize the global node_column_width to the largest node name
 *
 * \param[in]   the config doc
 */
void exa_clinfo::init_column_width(shared_ptr<xmlDoc> config_doc_ptr)
{
    xmlNodePtr node;
    xmlNodeSetPtr node_list;
    int i;

    node_column_width = MIN_NODE_COLUMN_WIDTH;

    node_list = xml_conf_xpath_query(config_doc_ptr.get(),
                                     "//cluster/node[@name and @hostname]");
    xml_conf_xpath_result_for_each(node_list, node, i)
    {
        size_t node_width;

        node_width = strlen(xml_get_prop(node, EXA_CONF_NODE_NAME)) + 1;

        node_column_width = MAX(node_column_width, node_width);
    }

    xml_conf_xpath_free(node_list);
}


/** \brief Return then given nodes in one or more line depending on wrapping_in_nodes
 *
 * \param[in]   nodes: The list of the nodes.
 * \param[in]   indent: the left indentation to add on the left of each new lines
 *
 * \return a ready to display list of nodes
 */
string exa_clinfo::display_nodes(const std::set<std::string> &nodelist,
                                 uint indent)
{
    string retval;
    uint index = 0;

    /* If we are under the wrapping_in_nodes, just display them all */
    if (nodelist.size() <= wrapping_in_nodes)
        return strjoin(" ", nodelist);

    string req = strjoin(" ", nodelist);
    set<string> unexp_list = exa_unexpand(req);

    retval = "";

    /* Manage the line wrapping */
    for (set<string>::iterator it = unexp_list.begin();
         it != unexp_list.end();
         ++it)
    {
        if (index++ >= wrapping_in_nodes)
        {
            string str_indent;
            for (uint i = 0; i < indent; i++)
                str_indent += " ";

            index = 1;
            retval += '\n';
            retval += str_indent.c_str();
        }
        retval.append(*it);
        retval.append(" ");
    }

    return retval;
}


/** \brief display node status
 *
 */
void exa_clinfo::display_node_status(const std::set<std::string> &nodelist,
                                     const string &color,
                                     const string &status)
{
    int statwidth = 12;
    int indent = 29;

    if (nodelist.size())
        exa_cli_info("%-*s%s%-*s%s %s\n",
                     indent - statwidth - 1, "",
                     color.c_str(),
                     statwidth, status.c_str(),
                     COLOR_NORM,
                     display_nodes(nodelist, indent).c_str());
}


/** \brief calc the list of nodes down in the given nodelist
 *
 * \note in order not to calculate it each time, we keep the
 *       reference of the previous nodelist and return the
 *       local static previously calculated downnodes.
 *
 * \return a std::set<std::string> with the down nodes.
 */
std::set<std::string> exa_clinfo::get_downnodes(
    shared_ptr<xmlDoc> config_doc_ptr,
    set<xmlNodePtr,
        exa_cmp_xmlnode_lt> &nodelist)
{
    static set<xmlNodePtr, exa_cmp_xmlnode_lt> previous_nodelist;
    static std::set<std::string> downnode_list;

    if (previous_nodelist == nodelist)
        return downnode_list;

    previous_nodelist = nodelist;
    downnode_list.clear();

    for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = nodelist.begin();
         it != nodelist.end(); ++it)
    {
        const char *node = xml_get_prop(*it, "name");

        if (!isNodeUp(config_doc_ptr, node))
            downnode_list.insert(node);
    }
    return downnode_list;
}


/** \brief remove each string elements of list2 from list1
 *
 * \return the list of elements that have been removed from list1
 */
std::set<std::string> exa_clinfo::nodelist_remove(std::set<std::string> &list1,
                                                  std::set<std::string> &list2)
{
    std::set<std::string> retval;

    EXA_ASSERT_VERBOSE(&list1 != &list2,
                       "Trying to remove one list from itself is not supported");
    for (std::set<std::string>::iterator it = list2.begin();
         it != list2.end();
         ++it)
    {
        string item = *it;

        if (list1.count(item))
        {
            list1.erase(item);
            retval.insert(item);
        }
    }
    return retval;
}


/** \brief nodelist intersections return in list1 only elements that are in list1 and list2
 *
 * \return the list of elements that have been removed from list1
 */
std::set<std::string> exa_clinfo::nodelist_intersect(
    std::set<std::string> &list1,
    std::set<std::string> &
    list2)
{
    std::set<std::string> retval;

    EXA_ASSERT_VERBOSE(
        &list1 != &list2,
        "Trying to intersect one list from itself is not supported");
    for (std::set<std::string>::iterator it = list2.begin();
         it != list2.end();
         ++it)
    {
        string item = *it;

        if (!list1.count(item))
            retval.insert(item);
    }

    for (std::set<std::string>::iterator it = list1.begin();
         it != list1.end();
         ++it)
    {
        string item = *it;

        if (!list2.count(item))
            retval.insert(item);
    }

    nodelist_remove(list1, retval);

    return retval;
}


/** split a string on 4 columns and return each value
 *
 * \param instr[in]: the node list to parse
 * \return the set of string splitted on " "
 */
std::set<std::string> exa_clinfo::nodestring_split(const char *instr)
{
    std::set<std::string> SplitVec;

    if (instr && instr[0] != '\0')
        boost::split(SplitVec, instr, boost::algorithm::is_any_of(" "));

    return SplitVec;
}


/** \brief Display softs info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodelist: The list of the nodes in xml format.
 * \param[out]  the well formatted result string, ready for stdout
 * \return the number of error found
 */
int exa_clinfo::exa_display_modules(shared_ptr<xmlDoc> config_doc_ptr,
                                    set<xmlNodePtr,
                                        exa_cmp_xmlnode_lt> &nodelist,
                                    string &resultstring)
{
    int error_count = 0;

    /* FIXME Why <, not <= ? */
    for (int i = EXA_MODULE__FIRST; i < EXA_MODULE__LAST; i++)
    {
        std::set<std::string> error_list;
        bool got_error = false;

        for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = nodelist.begin();
             it != nodelist.end(); ++it)
        {
            const char *node = xml_get_prop(*it, "name");

            if (isNodeUp(config_doc_ptr, node))
            {
                xmlNodePtr component_ptr =
                    xml_conf_xpath_singleton(
                        config_doc_ptr.get(),
                        "//modules/node[@name='%s' and @status='%s']/component[@name='%s']",
                        node, ADMIND_PROP_UP,
                        exa_module_name((exa_module_id_t) i));

                if (component_ptr == NULL)
                {
                    /* Should not happens. If it does, it means a module name has changed in admind side
                     * and it must be updated in our list
                     */
                    got_error = true;
                    error_list.insert(node);
                    continue;
                }

                const char *stat = xml_get_prop(component_ptr, "status");

                if (stat == NULL || strcmp(stat, ADMIND_PROP_OK) != 0)
                {
                    got_error = true;
                    error_list.insert(node);
                }
            }
        }

        if (got_error)
        {
            resultstring += str(format(
                                    "\n %s not loaded %|32t|%s") %
                                exa_module_name(
                                    (exa_module_id_t) i) %
                                display_nodes(error_list, 31));
            error_count++;
        }
    }

    /* We do not display full success, the exanodes daemon check is enough */
    return error_count;
}


/** \brief Display softs info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodeslist: The list of the nodes in xml format.
 * \param[out]  the well formatted result string, ready for stdout
 * \return the number of error found
 */
int exa_clinfo::exa_display_daemons(shared_ptr<xmlDoc> config_doc_ptr,
                                    set<xmlNodePtr,
                                        exa_cmp_xmlnode_lt> &nodelist,
                                    string &resultstring)
{
    int error_count = 0;
    bool got_error;

    for (int i = EXA_DAEMON__FIRST; i <= EXA_DAEMON__LAST; i++)
    {
        std::set<std::string> error_list;
        got_error = false;

        for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = nodelist.begin();
             it != nodelist.end(); ++it)
        {
            const char *node = xml_get_prop(*it, "name");

            if (isNodeUp(config_doc_ptr, node))
            {
                xmlNodePtr component_ptr =
                    xml_conf_xpath_singleton(
                        config_doc_ptr.get(),
                        "//software/node[@name='%s' and @status='%s']/component[@name='%s']",
                        node, ADMIND_PROP_UP,
                        exa_daemon_name((exa_daemon_id_t) i));

                if (component_ptr == NULL)
                {
                    got_error = true;
                    error_list.insert(node);
                    continue;
                }

                const char *stat = xml_get_prop(component_ptr, "status");

                if (stat == NULL || strcmp(stat, ADMIND_PROP_OK) != 0)
                {
                    got_error = true;
                    error_list.insert(node);
                }
            }
        }

        if (got_error)
        {
            resultstring += str(format(
                                    "\n %s not running %|32t|%s") %
                                exa_daemon_name(
                                    (exa_daemon_id_t) i) %
                                display_nodes(error_list, 31));
            error_count++;
        }
    }

    return error_count;
}


/** \brief Display groups info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   grouplist: The list of the groups
 *
 */
void exa_clinfo::exa_display_groups(shared_ptr<xmlDoc> config_doc_ptr,
                                    set<xmlNodePtr,
                                        exa_cmp_xmlnode_lt> &grplist)
{
    if (grplist.size() == 0)
    {
        exa_cli_info("%-15s None created.\n", "DISK GROUPS:");
        return;
    }

    exa_cli_info(
        "DISK GROUP      STATUS       LAYOUT    #VOLS #PHYS LOGICAL CAPACITY\n");
    exa_cli_info(
        "                                                        DEV  SIZE    USED   AVAIL %%USED\n");

    for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = grplist.begin();
         it != grplist.end(); ++it)
    {
        xmlNodePtr grNd = *it;
        const char *grName   = xml_get_prop(grNd, "name");
        bool group_is_rebuilding = false;
        string qstat        = xml_get_prop(grNd, "status");
        string qgoal        = xml_get_prop(grNd, "goal");
        string qtainted     = xml_get_prop(grNd, "tainted");
        string qtransaction = xml_get_prop(grNd, "transaction");
        string qadministrable = xml_get_prop(grNd, "administrable");

        if (!filter_only.empty() &&
            filter_only != grName)
            continue;

        if (xml_get_prop(grNd, "rebuilding") &&
            strcmp(xml_get_prop(grNd, "rebuilding"), ADMIND_PROP_TRUE) == 0)
            group_is_rebuilding = true;

        /* Display status of the disk group. If the transaction
         * parameter is still in-progress, then the disk group is
         * shown as invalid.
         */
        exa_cli_info(" %-*s ", 14, grName);

        if (qtransaction == ADMIND_PROP_INPROGRESS)
            displayStatus("INVALID", CLINFO_STATUS_WIDTH);
        else if (qstat == ADMIND_PROP_STOPPED && qgoal == ADMIND_PROP_STARTED)
            displayStatus("WILL START", CLINFO_STATUS_WIDTH);
        else if (qstat == ADMIND_PROP_STARTED && qgoal == ADMIND_PROP_STOPPED)
            displayStatus("WILL STOP", CLINFO_STATUS_WIDTH);
        else if (group_is_rebuilding)
            displayStatus(ADMIND_PROP_REBUILDING, CLINFO_STATUS_WIDTH);
        else
            displayStatus(qstat, CLINFO_STATUS_WIDTH);

        if (qstat.empty())
            exa_cli_info("  group is not created");
        else
            exa_display_components_in_group(config_doc_ptr, grNd);
        exa_cli_info("\n");

        if ((qadministrable == ADMIND_PROP_FALSE && qgoal ==
             ADMIND_PROP_STARTED)
            || qtainted == ADMIND_PROP_TRUE)
            exa_cli_info("%*s ", 15, "");

        /* Rq: the property administrable is relevant only if the group is
         * started or trying to start.
         */
        if (qadministrable == ADMIND_PROP_FALSE && qgoal == ADMIND_PROP_STARTED)
            exa_cli_info("%sNON ADMINISTRABLE%s   ", COLOR_ERROR, COLOR_NORM);

        if (qtainted == ADMIND_PROP_TRUE)
            exa_cli_info("%sTAINTED%s", COLOR_ERROR, COLOR_NORM);

        if ((qadministrable == ADMIND_PROP_FALSE && qgoal ==
             ADMIND_PROP_STARTED)
            || qtainted == ADMIND_PROP_TRUE)
            exa_cli_info("\n");
    }

    exa_cli_info("\n");
}


/** \brief Display components ( rdevs and volumes) of one group.
 *
 * \param[in]    config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]    grp: group name.
 */
void exa_clinfo::exa_display_components_in_group(
    shared_ptr<xmlDoc> config_doc_ptr,
    xmlNodePtr groupptr)
{
    string mant, unit;
    string group_status = xml_get_prop(groupptr, "status");

    exa_cli_info(" %-*s", 9, xml_get_prop(groupptr, "layout"));

    /*
     * Get Logical sizes
     */
    uint64_t sizeLT = 0;
    const char *sizeUStr = xml_get_prop(groupptr, "usable_capacity");
    if (sizeUStr)
        if (!conv_Qstr2u64(sizeUStr, &sizeLT))
            throw CommandException("Invalid usable capacity");

    uint64_t sizeLU = 0;
    sizeUStr = xml_get_prop(groupptr, "size_used");
    if (sizeUStr)
        if (!conv_Qstr2u64(sizeUStr, &sizeLU))
            throw CommandException("Invalid used size");

    /*
     * Get physical and volume
     */
    uint64_t nbRdevs = lexical_cast<uint64_t>(xml_get_prop(groupptr, "nb_disks"));
    uint64_t nbVols  =
        lexical_cast<uint64_t>(xml_get_prop(groupptr, "nb_volumes"));

    exa_cli_info(" %5" PRIu64 "%5" PRIu64, nbVols, nbRdevs);

    if (group_status != ADMIND_PROP_STOPPED)
    {
        /*
         * Display sizes of this group
         */
        format_hum_friend(sizeLT, mant, unit, force_kilo);
        exa_cli_info(" %5s%s", mant.c_str(), unit.c_str());

        format_hum_friend(sizeLU, mant, unit, force_kilo);
        exa_cli_info(" %5s%s", mant.c_str(), unit.c_str());

        format_hum_friend(sizeLT - sizeLU, mant, unit, force_kilo);
        exa_cli_info(" %5s%s", mant.c_str(), unit.c_str());
        if (sizeLT)
            exa_cli_info(" %3.0f%%", sizeLU * 100 / (double) sizeLT);
    }
    else
        exa_cli_info(" %25s", "");

    int nb_spare;
    int nb_spare_available;

    /* XXX What if either of these values isn't valid? */
    if (to_int(xml_get_prop(groupptr, "nb_spare"), &nb_spare) != EXA_SUCCESS
        || nb_spare < 0)
        return;

    if (to_int(xml_get_prop(groupptr,
                            "nb_spare_available"), &nb_spare_available)
        != EXA_SUCCESS || nb_spare_available < 0)
        return;

    unsigned int nb_spare_used = nb_spare - nb_spare_available;
    exa_cli_info(" Spare used %u/%u", nb_spare_used, nb_spare);
}


void exa_clinfo::exa_display_group_configurations(set<xmlNodePtr,
                                                      exa_cmp_xmlnode_lt> &
                                                  grplist)
{
    if (grplist.size() == 0)
        return;

    exa_cli_info("DISK GROUP CONFIGURATIONS:\n");

    for (set<xmlNodePtr, exa_cmp_xmlnode_lt>::iterator it = grplist.begin();
         it != grplist.end(); ++it)
    {
        const xmlNodePtr node_ptr = *it;
        const char *name            = xml_get_prop(node_ptr, "name");
        const char *status          = xml_get_prop(node_ptr, "status");
        const char *slot_width      = xml_get_prop(node_ptr, "slot_width");
        const char *chunk_size      = xml_get_prop(node_ptr, "chunk_size");
        const char *su_size         = xml_get_prop(node_ptr, "su_size");
        const char *dirty_zone_size = xml_get_prop(node_ptr, "dirty_zone_size");
        const char *blended_stripes = xml_get_prop(node_ptr, "blended_stripes");

        if (strcmp(status, ADMIND_PROP_STOPPED) == 0)
        {
            exa_cli_info(" %-15s (N/A)\n", name);
            continue;
        }

        exa_cli_info(" %-15s\n", name);
        exa_cli_info("    slot width : %9s\n"
                     "    chunk size : %9s KiB\n"
                     "    SU size    : %9s KiB\n",
                     slot_width, chunk_size, su_size);
        if (dirty_zone_size)
            exa_cli_info("    dirty zone : %9s KiB\n", dirty_zone_size);
        if (blended_stripes)
            exa_cli_info("    blended    : %9s\n", blended_stripes);
    }
    exa_cli_info("\n");
}


/** \brief Display one volume info
 *
 * \param[in]   the volume
 */
void exa_clinfo::exa_display_volume(shared_ptr<xmlDoc> config_doc_ptr,
                                    xmlNodePtr &vol, const string &group_status)
{
    const char *name   = xml_get_prop(vol, "name");
    const char *nameGr = xml_get_prop(vol->parent->parent, "name");
    const char *access_mode = xml_get_prop(vol, "accessmode");
    string volname(string(nameGr) + ":" + name);
    string qsize = NOT_AVAILABLE;

    if (group_status != ADMIND_PROP_STOPPED)
        qsize = exa_format_human_readable_string(
            xml_get_prop(vol, "size"), force_kilo);

    exa_cli_info(" %-27s %-5s %-8s\n",
                 volname.c_str(), qsize.c_str(),
                 access_mode ? access_mode : "UNKNOWN");
}


/** \brief Display volumes status info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodelist: The list of the nodes
 *
 */
void exa_clinfo::exa_display_volumes_status(shared_ptr<xmlDoc> config_doc_ptr,
                                            set<xmlNodePtr,
                                                exa_cmp_xmlnode_lt> &nodelist)
{
    set<xmlNodePtr, exa_cmp_xmlnode_volume_lt> logdevlist;
    xmlNodePtr volume;
    int i;
    get_downnodes(config_doc_ptr, nodelist);

    xmlNodeSetPtr volume_set =
        xml_conf_xpath_query(config_doc_ptr.get(), "//diskgroup/logical/volume");

    xml_conf_xpath_result_for_each(volume_set, volume, i)
    logdevlist.insert(volume);

    xml_conf_xpath_free(volume_set);

    /*
     * Display title volumes status
     */
    if (logdevlist.size() < 1)
        return;

    exa_cli_info("VOLUMES:\n");

    /*
     * Display info volumes
     */
    for (set<xmlNodePtr, exa_cmp_xmlnode_volume_lt>::iterator
         it = logdevlist.begin(); it != logdevlist.end(); ++it)
    {
        xmlNodePtr vol = *it;
        const char *statGr = xml_get_prop(vol->parent->parent, "status");
        const char *name   = xml_get_prop(vol, "name");
        const char *nameGr = xml_get_prop(vol->parent->parent, "name");
        string volname(string(nameGr) + ":" + name);

        /* The group status */
        string group_status(statGr);

        if (group_status.empty())
            continue;

        if (!filter_only.empty() && filter_only != volname)
            continue;

        string strTransaction = xml_get_prop(vol, "transaction");

        /*
         * Display info volumes
         */

        exa_display_volume(config_doc_ptr, vol, group_status);

        if (isInProgress(strTransaction.c_str()))
            exa_cli_info(" %-*s %s%-*s%s\n", 27, "",
                         COLOR_ERROR, 60, "INVALID", COLOR_NORM);
        else if (group_status == ADMIND_PROP_STOPPED)
        {
            /* The group is stopped, then the volume is stopped, nothing to display */
        }
        else
            for (xmlNodePtr node = vol->children;
                 node != NULL; node = node->next)
                if (xmlStrEqual(node->name, BAD_CAST("export")))
                    exa_display_export_status(vol, node, group_status);
    }
}


/** comparison operator for 'gateway' XML nodes */
struct gateway_node_lt
{
    bool operator ()(const xmlNodePtr &item1, const xmlNodePtr &item2) const
    {
        const char *c1(xml_get_prop((xmlNodePtr) item1, "node_name"));
        const char *c2(xml_get_prop((xmlNodePtr) item2, "node_name"));

        EXA_ASSERT(c1);
        EXA_ASSERT(c2);

        return os_strverscmp(c1, c2) < 0;
    }
};

/** \brief Display one export info
 *
 * \param[in]   the element "export"
 * \param[in]   the group status
 */
void exa_clinfo::exa_display_export_status(xmlNodePtr volume_node,
                                           xmlNodePtr export_node,
                                           const string &group_status)
{
    const char *method = xml_get_prop(export_node, "method");
    const char *id_type = xml_get_prop(export_node, "id_type");
    const char *id_value = xml_get_prop(export_node, "id_value");

    exa_cli_info("    %s | %s%s%s",
                 method ? method : "UNKNOWN",
                 id_type ? id_type : "",
                 /* prevent from having '  ' when id_type is empty */
                 id_type && id_type[0] ? " " : "",
                 id_value ? id_value : "UNKNOWN");
#ifdef WITH_FS
    /* Check the volume is part of a filesystem. If so, display it */
    for (xmlNodePtr fs_node = volume_node->children;
         fs_node != NULL; fs_node = fs_node->next)
        if (xmlStrEqual(fs_node->name, BAD_CAST("fs")))
            exa_cli_info("  (type '%s')", xml_get_prop(fs_node, "type"));

#endif
    exa_cli_info("\n");

    std::set<std::string> goal_stopped =
        nodestring_split(xml_get_prop(volume_node, "goal_stopped"));
    std::set<std::string> goal_started =
        nodestring_split(xml_get_prop(volume_node, "goal_started"));
    std::set<std::string> goal_readonly =
        nodestring_split(xml_get_prop(volume_node, "goal_readonly"));
    std::set<std::string> status_in_use =
        nodestring_split(xml_get_prop(volume_node, "status_in_use"));
    std::set<std::string> status_started =
        nodestring_split(xml_get_prop(volume_node, "status_started"));
    std::set<std::string> status_readonly =
        nodestring_split(xml_get_prop(volume_node, "status_readonly"));

    /* Display all relevant information, most important on top, each on one line */

    nodelist_remove(goal_started, status_in_use);
    nodelist_remove(goal_started, status_started);

    nodelist_intersect(goal_stopped, status_started);

    nodelist_remove(status_started, status_in_use);

    std::set<std::string> status_started_ro = nodelist_remove(status_started,
                                                              status_readonly);
    std::set<std::string> status_in_use_ro = nodelist_remove(status_in_use,
                                                             status_readonly);

    std::set<std::string> goal_started_ro = nodelist_remove(goal_started,
                                                            goal_readonly);
    std::set<std::string> goal_stopped_ro = nodelist_remove(goal_stopped,
                                                            goal_readonly);

    if (group_status == ADMIND_PROP_OFFLINE)
    {
        display_node_status(status_in_use_ro, COLOR_ERROR, "*IN USE RO*");
        display_node_status(status_in_use, COLOR_ERROR, "*IN USE*");
        display_node_status(status_started_ro, COLOR_ERROR, "*EXPORTED RO*");
        display_node_status(status_started, COLOR_ERROR, "*EXPORTED*");
    }
    else
    {
        display_node_status(status_in_use_ro, COLOR_USED, "IN USE RO");
        display_node_status(status_in_use, COLOR_USED, "IN USE");
        display_node_status(status_started_ro, COLOR_INFO, "EXPORTED RO");
        display_node_status(status_started, COLOR_INFO, "EXPORTED");
    }
    display_node_status(goal_started, COLOR_NORM, "WILL EXPORT");
    display_node_status(goal_stopped, COLOR_NORM, "WILL UNEXPORT");
    display_node_status(goal_started_ro, COLOR_NORM, "WILL EXPORT RO");
    display_node_status(goal_stopped_ro, COLOR_NORM, "WILL UNEXPORT RO");

    /* create sorted set of child 'gateway' nodes*/
    set<xmlNodePtr, gateway_node_lt> gateway_set;
    for (xmlNodePtr node_it = export_node->children;
         node_it != NULL; node_it = node_it->next)
        if (xmlStrEqual(node_it->name, BAD_CAST("gateway")))
            gateway_set.insert(node_it);

    /* Print the information from each gateway node */
    set<xmlNodePtr, gateway_node_lt>::iterator gateway_it;
    for (gateway_it = gateway_set.begin();
         gateway_it != gateway_set.end(); ++gateway_it)
    {
        xmlNodePtr gateway_node = *gateway_it;

        if (!xmlStrEqual(gateway_node->name, BAD_CAST("gateway")))
            continue;

        if (iscsi_details && !strcmp(method, "iSCSI"))
        {
            /* Print the target IQN and the list of connected initiators */
            exa_cli_info("        %s (%s, %s)\n",
                         xml_get_prop(gateway_node, "node_name"),
                         xml_get_prop(gateway_node, "iqn"),
                         xml_get_prop(gateway_node, "listen_address"));
            for (xmlNodePtr iqn_node = gateway_node->children;
                 iqn_node != NULL; iqn_node = iqn_node->next)
            {
                if (!xmlStrEqual(iqn_node->name, BAD_CAST("iqn")))
                    continue;

                exa_cli_info("          <- %s\n", xml_get_prop(iqn_node, "id"));
            }
        }
    }

    exa_cli_info("\n");
}


/** \brief Helper function : Displaying a size humanly-readable if it is not null,
 *         and different from "-1".
 *
 * \param[in]   config_string:
 * \param[in]   force_kilo:
 * \return String representing the value if valid : value+unit or N/A if invalid.
 */
string exa_clinfo::exa_format_human_readable_string(const char *config_string,
                                                    bool force_kilo)
{
    if (config_string)
        if (strcmp(config_string, "-1"))
        {
            uint64_t size = 0;
            bool ret;
            ret = conv_Qstr2u64(config_string, &size);
            if (ret)
            {
                string mant, unit;
                format_hum_friend(size, mant, unit, force_kilo);
                return mant + unit;
            }
        }
    return NOT_AVAILABLE;
}


#ifdef WITH_FS

/**
 * \brief Display the status of a file system on stdout
 */
void exa_clinfo::exa_display_one_fs_status(shared_ptr<xmlDoc> config_doc_ptr,
                                           xmlNodePtr fs, const string &name)
{
    int fstype_field_width = 8;

    std::set<std::string> goal_started      =
        nodestring_split(xml_get_prop(fs, "goal_started"));
    std::set<std::string> goal_started_ro   =
        nodestring_split(xml_get_prop(fs, "goal_started_ro"));
    std::set<std::string> status_mounted    =
        nodestring_split(xml_get_prop(fs, "status_mounted"));
    std::set<std::string> status_mounted_ro =
        nodestring_split(xml_get_prop(fs, "status_mounted_ro"));

    string transaction = xml_get_prop(fs, "transaction");

    if (!filter_only.empty() &&
        filter_only != name)
        return;

    /* Get the group name (assuming we have a single volume per FS) */
    xmlNodePtr volume = fs->parent;
    xmlNodePtr group = volume->parent->parent;
    string group_name = xml_get_prop(group, "name");

    if (isInProgress(transaction.c_str()))
        exa_cli_info(" %-*s %s%-*s%s\n", 27, (group_name + ":" + name).c_str(),
                     COLOR_ERROR, 60, "INVALID", COLOR_NORM);
    else
    {
        /* Get the group status */
        string group_status(xml_get_prop(group, "status"));

        const char *type  = xml_get_prop(fs, "type");
        const char *total_size  = xml_get_prop(fs, "size");
        const char *size_used  = xml_get_prop(fs, "used");
        const char *size_available  = xml_get_prop(fs, "available");
        const char *mountpoint = xml_get_prop(fs, "mountpoint");
        const char *handle_gfs_str = xml_get_prop(fs, EXA_CONF_FS_HANDLE_GFS);
        const char *mount_option = xml_get_prop(fs, "mount_option");
        if (mount_option == NULL)
            mount_option = "";

        bool handle_gfs = true;
        if (handle_gfs_str)
            handle_gfs = strcmp(handle_gfs_str, ADMIND_PROP_NOK);

        string final_total_size(exa_format_human_readable_string(total_size,
                                                                 force_kilo));
        string final_size_used(exa_format_human_readable_string(size_used,
                                                                force_kilo));
        string final_size_available(exa_format_human_readable_string(
                                        size_available,
                                        force_kilo));

        if (final_total_size != NOT_AVAILABLE)
            exa_cli_info(" %-*s %-*s %-*s %-*s %-*s %-*s %-20s\n",
                         27,
                         (group_name + ":" + name).c_str(),
                         5,
                         final_total_size.c_str(),
                         5,
                         final_size_used.c_str(),
                         5,
                         final_size_available.c_str(),
                         fstype_field_width,
                         type,
                         7,
                         mount_option,
                         mountpoint);
        else
        {
            string need_cluster_reboot = string(COLOR_ERROR) +
                                         "(NEED CLUSTER REBOOT)" + COLOR_NORM;
            exa_cli_info(" %-*s %-17s %-*s %-*s %-20s\n",
                         27,
                         (group_name + ":" + name).c_str(),
                         handle_gfs ? "(NO INFO)" : need_cluster_reboot.c_str(),
                         fstype_field_width,
                         type,
                         7,
                         mount_option,
                         mountpoint);
        }

        nodelist_remove(goal_started, status_mounted);
        nodelist_remove(goal_started, status_mounted_ro);
        nodelist_remove(goal_started, goal_started_ro);
        nodelist_remove(status_mounted, status_mounted_ro);
        nodelist_remove(goal_started_ro, status_mounted);
        nodelist_remove(goal_started_ro, status_mounted_ro);

        if (group_status == ADMIND_PROP_OFFLINE)
        {
            display_node_status(status_mounted, COLOR_ERROR, "*MOUNTED*");
            display_node_status(status_mounted_ro, COLOR_ERROR, "*MOUNTED RO*");
        }
        else
        {
            display_node_status(status_mounted, COLOR_USED, "MOUNTED");
            display_node_status(status_mounted_ro, COLOR_USED, "MOUNTED RO");
        }
        display_node_status(goal_started, COLOR_NORM, "WILL START");
        display_node_status(goal_started_ro, COLOR_NORM, "WILLSTART RO");
    }
    exa_cli_info("\n");
}


void exa_clinfo::exa_display_gulm_nodes(shared_ptr<xmlDoc> config_doc_ptr)
{
    int i;
    xmlNodePtr fsptr;
    xmlNodeSetPtr fsptr_set;

    fsptr_set = xml_conf_xpath_query(
        config_doc_ptr.get(),
        "//volume/fs[@type = 'sfs'][@goal_started != '']");

    bool sfs_started(xml_conf_xpath_result_entity_count(fsptr_set) > 0);
    xml_conf_xpath_free(fsptr_set);

    if (sfs_started == false)
        return;

    fsptr_set = xml_conf_xpath_query(config_doc_ptr.get(),
                                     "//software/node[@gulm_mode]");

    exa_cli_info("%s\n", "SFS MASTERS:");

    /* a temporary sets for sorting by node name... */
    std::set<std::string> masters_up;
    std::set<std::string> masters_down;

    xml_conf_xpath_result_for_each(fsptr_set, fsptr, i)
    {
        std::string node(xml_get_prop(fsptr, "name"));

        boost::to_lower(node);
        std::string status(xml_get_prop(fsptr, "status"));
        boost::to_lower(status);
        std::string mode(xml_get_prop(fsptr, "gulm_mode"));
        boost::to_lower(mode);

        if (mode == "client")
            continue;

        if (status == "up")
            masters_up.insert(node);
        else
            masters_down.insert(node);
    }
    xml_conf_xpath_free(fsptr_set);

    if (masters_up.empty() == false)
    {
        exa_cli_info(" UP  ");
        for (std::set<std::string>::const_iterator it = masters_up.begin();
             it != masters_up.end(); ++it)
            exa_cli_info(" %s", it->c_str());
        exa_cli_info("\n");
    }

    if (masters_down.empty() == false)
    {
        exa_cli_info(" %sDOWN%s", COLOR_WARNING, COLOR_NORM);
        for (std::set<std::string>::const_iterator it = masters_down.begin();
             it != masters_down.end(); ++it)
            exa_cli_info(" %s", it->c_str());
        exa_cli_info("\n");
    }

    exa_cli_info("\n");
}


/** \brief Display filesystems status info
 *
 * \param[in]   config_doc_ptr: The doc xml tree of the response from admind.
 * \param[in]   nodeslist: The list of the nodes
 * \param[in]   nbNodes: number of nodes.
 *
 */
void exa_clinfo::exa_display_fs_status(shared_ptr<xmlDoc> config_doc_ptr,
                                       set<xmlNodePtr,
                                           exa_cmp_xmlnode_lt> &nodelist)
{
    int fstype_field_width = 8;

    set<xmlNodePtr, exa_cmp_xmlnode_fs_lt> fslist;
    xmlNodePtr fsptr;
    int i;

    xmlNodeSetPtr fsptr_set =
        xml_conf_xpath_query(
            config_doc_ptr.get(), "//diskgroup/logical/volume/fs");
    xml_conf_xpath_result_for_each(fsptr_set, fsptr, i)
    fslist.insert(fsptr);

    xml_conf_xpath_free(fsptr_set);

    if (fslist.size() < 1)
        return;

    exa_cli_info("FILE SYSTEMS %-*s %-*s%-*s%-*s%-*s%-*s%-20s\n",
                 15,
                 "",
                 6,
                 "SIZE",
                 6,
                 "USED",
                 6,
                 "AVAIL",
                 fstype_field_width + 1,
                 "TYPE",
                 8,
                 "OPTIONS",
                 "MOUNTPOINT");

    for (set<xmlNodePtr, exa_cmp_xmlnode_fs_lt>::iterator it = fslist.begin();
         it != fslist.end(); ++it)
    {
        xmlNodePtr fs = *it;
        exa_display_one_fs_status(config_doc_ptr,
                                  fs,
                                  xml_get_prop(fs->parent, "name"));
    }
}


#endif /* WITH_FS */

void exa_clinfo::reset_all_info_flags()
{
    volumes_info = false;
    groups_info = false;
    disks_info = false;
#ifdef WITH_FS
    filesystems_info = false;
#endif
    softwares_info = false;
}


void exa_clinfo::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('g') != opt_args.end())
    {
        reset_all_info_flags();
        groups_info = true;
    }

    if (opt_args.find('G') != opt_args.end())
        display_group_config = true;

    if (opt_args.find('l') != opt_args.end())
    {
        reset_all_info_flags();
        volumes_info = true;
    }

    if (opt_args.find('i') != opt_args.end())
    {
        reset_all_info_flags();
        disks_info = true;
    }

    if (opt_args.find('D') != opt_args.end())
        iscsi_details = true;

    if (opt_args.find('S') != opt_args.end())
    {
        reset_all_info_flags();
        softwares_info = true;
    }

#ifdef WITH_FS
    if (opt_args.find('f') != opt_args.end())
    {
        reset_all_info_flags();
        filesystems_info = true;
    }

    if (opt_args.find('F') != opt_args.end())
        display_filesystem_size = true;

#endif

    if (opt_args.find('k') != opt_args.end())
        force_kilo = true;

    if (opt_args.find('u') != opt_args.end())
        show_unassigned_rdev = true;

    if (opt_args.find('o') != opt_args.end())
        filter_only = opt_args.find('o')->second;

    if (opt_args.find('w') != opt_args.end())
    {
        if (to_uint(opt_args.find('w')->second.c_str(), &wrapping_in_nodes)
            != EXA_SUCCESS)
            throw CommandException("Invalid wrapping number of nodes");
        if (wrapping_in_nodes == 0)
            /* Disabling node wrapping by setting a large enough number */
            wrapping_in_nodes = UINT32_MAX;
    }
    if (opt_args.find('0') != opt_args.end())
        config_ptr = shared_ptr<xmlDoc>(
            xml_conf_init_from_file(opt_args.find(
                                        '0')->second.c_str()), xmlFreeDoc);

    if (opt_args.find('x') != opt_args.end())
        xml_dump = true;
}


void exa_clinfo::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Display the current state of an Exanodes cluster.";
}


void exa_clinfo::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Display the current state of the Exanodes cluster " <<
    ARG_CLUSTERNAME
        << "." << std::endl << std::endl;
}


void exa_clinfo::dump_output_section(std::ostream &out, bool show_hidden) const
{
    out <<
    "The output shows the status of each Exanodes item as defined below. ";
#ifdef WITH_FS
    out <<
    "Volumes and file systems might have different status at a given time. ";
    out <<
    "Each volume and file system status applies to the nodes listed after it.";
#else
    out << "Volumes might have different status at a given time. ";
    out << "Each volume status applies to the nodes listed after it.";
#endif
    out << std::endl;

    Subtitle(out, "Disk group status:");
    ItemizeBegin(out);
    Item(out, "STOPPED", "The disk group is stopped.");
    Item(out, "OK", "The disk group is started and healthy.");
    Item(
        out,
        "USING SPARE",
        "The disk group is started and one or more spare disks "
        "are currently used. A new failure can be supported but you should fix the "
        "DOWN, MISSING or BROKEN disks as soon as possible.");

    Item(out,
         "DEGRADED",
         "The disk group is started and all spare disks are currently used. "
         + Boldify(
             "CAUTION! A new failure is not supported") +
         ": it will bring the disk group to the OFFLINE state. " +
         "You should fix the DOWN, MISSING or BROKEN disks as soon as possible.");

    Item(out,
         "OFFLINE",
         "Too many disks are DOWN, MISSING or BROKEN. Until this is "
#ifdef WITH_FS
         "fixed, the writes to the volumes or file systems of this "
#else
         "fixed, the writes to the volumes of this "
#endif
         "disk group is denied. Reading might be tried but this might fail.");
    Item(
        out,
        "REBUILDING",
        "One or more disks from the group are currently being rebuilt to adapt to a change of disk states.");
    ItemizeEnd(out);

    Subtitle(out, "Disk group flags:");
    ItemizeBegin(out);
    Item(out, "NON ADMINISTRABLE",
#ifdef WITH_FS
         "Volumes or file systems from this disk group cannot be "
#else
         "Volumes from this disk group cannot be "
#endif
         "created or deleted since too many disks are MISSING or BROKEN.");
    ItemizeEnd(out);

    Subtitle(out, "Disk status:");
    ItemizeBegin(out);
    Item(out,
         "UP",
         "The disk is healthy and currently not used in any disk group.");
    Item(out, "OK", "The disk is healthy and currently used in a disk group.");
    Item(out, "DOWN", "The node containing the disk is down.");
    Item(out,
         "BROKEN",
         "The disk returned some IO errors and has been excluded. "
         "It should be replaced by a new disk.");
    Item(out, "MISSING", "Exanodes was not able to find the disk. "
         "Maybe it is not accessible anymore. "
         "Maybe it has been overwritten by a third party application.");
    Item(out, "UPDATING", "The disk is being updated.");
    Item(out, "REPLICATING", "Some spare room of the disk is being filled.");
    Item(out, "OUT-DATED", "The disk contains out of date data. "
         "It will be updated as soon as up to date data is available.");
    Item(out,
         "BLANK",
         "The disk does not contain data. This is an intermediate state.");
    Item(
        out,
        "ALIEN",
        "The disk is recognized as an Exanodes disk but some metadata are corrupted. "
        "It was probably partially overwritten by a third party application.");
    ItemizeEnd(out);

    Subtitle(out, "Volume status:");
    ItemizeBegin(out);
    Item(out, "EXPORTED", "Exported.");
    Item(out, "IN USE", "Exported and mounted or opened by an application.");
    Item(out,
         "WILL EXPORT",
         "Will be exported as soon as possible, eg. when the node will be up.");
    Item(out, "WILL UNEXPORT", "Will be unexported as soon as possible.");
    Item(out, "... RO", "Read-only mode.");
    Item(out,
         "*...* (red)",
         "The disk group is currently OFFLINE (see disk group status above).");
    ItemizeEnd(out);

#ifdef WITH_FS
    Subtitle(out, "Filesystem status:");
    ItemizeBegin(out);
    Item(out, "MOUNTED", "Started.");
    Item(out,
         "WILL START",
         "Will start as soon as possible, eg. when the node will be up.");
    Item(out, "WILL STOP", "Will stop as soon as possible.");
    Item(out, "... RO", "Read-only mode.");
    Item(out,
         "*...* (red)",
         "The disk group is currently OFFLINE (see disk group status above).");
    ItemizeEnd(out);
#endif
}


void exa_clinfo::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Display the current state of the cluster " << Boldify("mycluster")
        << std::endl;
    out << std::endl;
    out << "  " << "exa_clinfo mycluster" << std::endl;
    out << std::endl;

    if (show_hidden == false)
        return;

    out << "Display the current state of the volume " << Boldify("myvolume")
        << " in the group " << Boldify("mygroup") << " in the cluster " <<
    Boldify("mycluster") << ":"
                         << std::endl;
    out << std::endl;
    out << "  " << "exa_clinfo -l -o mygroup:myvolume mycluster" << std::endl;
    out << std::endl;
}


