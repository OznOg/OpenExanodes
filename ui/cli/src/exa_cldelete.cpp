/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_cldelete.h"

#include "common/include/uuid.h"
#include "common/include/exa_mkstr.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_stdio.h"

using std::string;

exa_cldelete::exa_cldelete() : _forcemode(false) , _recursive(false)
{
    /* short_opt, long_opt, description, mandatory, arg_expected, default_value */
    add_option('r', "recursive",
#ifdef WITH_FS
               "Recursively delete stopped groups, volumes and file systems.",
#else
               "Recursively delete stopped groups and volumes.",
#endif
               0, false, false);
    add_option('f', "force", "Force the deletion even if one or more nodes are "
               "not ready to accept the command. A cleaner way is to run "
               "exa_clnodestop on these nodes.", 0, false, false);

    add_see_also("exa_clcreate");
    add_see_also("exa_clstart");
    add_see_also("exa_clstop");
    add_see_also("exa_clinfo");
    add_see_also("exa_clstats");
    add_see_also("exa_cltune");
    add_see_also("exa_clreconnect");
}


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
struct cldelete_filter : private boost::noncopyable
{
    bool filter_got_error;
    bool filter_cluster_not_empty;
    uint ready_to_delete;
    uint already_deleted;
    bool node_started;

    cldelete_filter() :
        filter_got_error(false),
        filter_cluster_not_empty(false),
        ready_to_delete(0),
        already_deleted(0),
        node_started(false)
    {}


    void operator ()(const std::string &node, exa_error_code error_code,
                     std::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_ERR_ADMIND_STOPPED:
            ready_to_delete++;
            break;

        case EXA_SUCCESS:
            ready_to_delete++;
            break;

        case EXA_ERR_ADMIND_NOCONFIG:
            /* It's was already deleted, no problem */
            ready_to_delete++;
            already_deleted++;
            break;

        case EXA_ERR_ADMIND_STARTED:
            /* The cluster is started, display it once after the processing */
            filter_got_error = true;
            node_started = true;
            break;

        case EXA_ERR_ADMIND_STOPPING:
            filter_got_error = true;
            exa_cli_warning("\n%sWARNING%s, %s: %s", COLOR_WARNING, COLOR_NORM,
                            node.c_str(),
                            "This node is not properly stopped.\n         "
                            "You can stop it by using exa_clnodestop");
            break;

        case EXA_ERR_ADMIND_STARTING:
            filter_got_error = true;
            exa_cli_warning("\n%sWARNING%s, %s: %s", COLOR_WARNING, COLOR_NORM,
                            node.c_str(),
                            "This node is trying to start.\n         "
                            "You can change its goal to 'stopped' by using "
                            "exa_clnodestop");
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_warning("\n%sWARNING%s, %s: Connection failure.",
                            COLOR_WARNING, COLOR_NORM, node.c_str());
            filter_got_error = true;
            break;

        case ADMIND_ERR_CLUSTER_NOT_EMPTY:
            filter_cluster_not_empty = true;
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

void exa_cldelete::run()
{
    char msg_str[80];
    string error_msg;

    if (set_cluster_from_cache(_cluster_name.c_str(), error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    printf("Deleting Exanodes cluster initialization for cluster '%s'\n",
           exa.get_cluster().c_str());

    os_snprintf(msg_str,
                sizeof(msg_str),
                "volumes of the cluster '%s'.",
                exa.get_cluster().c_str());

    /*
     * Send a command to each node for each cluster to check nodes are free
     *
     * We don't accept to delete a cluster is not all nodes are ready do to so.
     * We send a "get_cluster_name" to each individual nodes but don't
     * relly care of the response content, just the fact that we have a response
     * matters.
     * The command embedded UUID check for us that the nodes we talk to are really
     * part of our cluster.
     */

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", "Checking nodes are ready");

    AdmindCommand command_getname("get_cluster_name", exa_uuid_zero);

    cldelete_filter myfilter1;

    send_admind_by_node(command_getname, exa.get_hostnames(),
                                      std::ref(myfilter1));

    /* Display the global status */
    if (!myfilter1.filter_got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else if (myfilter1.filter_cluster_not_empty)
        exa_cli_error("\n%sERROR%s: The cluster still contains one or more "
                      "disk group.\n       Cluster deletion is not possible.\n",
                      COLOR_ERROR, COLOR_NORM);

    if (_forcemode == false)
        if (myfilter1.ready_to_delete != exa.get_hostnames().size())
            throw CommandException(
                "Cluster deletion is not possible. Please fix previous error first.");

    /*
     * Send command to each node for each cluster
     */
    AdmindCommand command_delete("cldelete", exa.get_cluster_uuid());
    command_delete.add_param("recursive", _recursive);

    cldelete_filter myfilter2;

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Deleting the initialization file");

    send_admind_by_node(command_delete, exa.get_hostnames(), std::ref(myfilter2));

    /* Display the global status */
    if (!myfilter2.filter_got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else if (myfilter2.filter_cluster_not_empty)
        exa_cli_error("\n%sERROR%s: The cluster still contains one or more "
                      "disk group.\n       Cluster deletion is not possible.\n",
                      COLOR_ERROR, COLOR_NORM);

    if (myfilter2.ready_to_delete != exa.get_hostnames().size())
    {
        if (myfilter2.node_started)
            throw CommandException(
                "It is not allowed to delete a started cluster. Please use exa_clstop first.");
        else if (myfilter2.ready_to_delete == 0)
            throw CommandException(
                "Exanodes cluster is not deleted. Please check previous error.");
        else throw CommandException(
                "Exanodes cluster is not deleted on one or more node.");
    }
    else if (myfilter2.already_deleted == exa.get_hostnames().size())
        exa_cli_warning("\n%sWARNING%s: %s\n",
                        COLOR_WARNING, COLOR_NORM,
                        "Cluster was already deleted on all nodes.");


    /* It worked, we need to remove the nodes cache file */
    exa.del_cluster();
}


void exa_cldelete::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('f') != opt_args.end())
        _forcemode = true;

    if (opt_args.find('r') != opt_args.end())
        _recursive = true;
}


void exa_cldelete::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Delete an Exanodes cluster.";
}


void exa_cldelete::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Delete the cluster " << ARG_CLUSTERNAME <<
    ". All disk groups should be deleted "
        << "from the cluster before, unless you use the --recursive option." <<
    std::endl << std::endl;
}


void exa_cldelete::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Delete the cluster " << Boldify("mycluster")
        << " which does not contain any disk group." << std::endl;
    out << std::endl;
    out << "  " << "exa_cldelete mycluster" << std::endl;
    out << std::endl;

    out << "Delete the cluster " << Boldify("mycluster")
#ifdef WITH_FS
        << " and all its disk groups, volumes and file systems." << std::endl;
#else
        << " and all its disk groups and volumes." << std::endl;
#endif
    out << std::endl;
    out << "  " << "exa_cldelete -r mycluster" << std::endl;
    out << std::endl;
}


