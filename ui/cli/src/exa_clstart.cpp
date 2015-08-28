/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clstart.h"

#include "common/include/exa_mkstr.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"

using boost::shared_ptr;
using std::string;

exa_clstart::exa_clstart(int argc, char *argv[])
    : exa_clcommand(argc, argv)
{}


exa_clstart::~exa_clstart()
{ }

void exa_clstart::init_options()
{
    exa_clcommand::init_options();
}


void exa_clstart::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
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
 * \return EXA_ERR_DEFAULT to stop command processing
 *         EXA_SUCCESS to continue command processing to next nodes
 */
struct clstart_filter : private boost::noncopyable
{
    const uint nb_nodes;
    uint       nb_node_in_error;
    uint       nb_node_need_recover;
    uint       nb_node_unreachable;
    uint       nb_node_already_started;
    uint       nb_node_ready;
    uint       nb_node_ready_to_start;
    string     last_node_that_init;

    clstart_filter(uint _nb_nodes) :
        nb_nodes(_nb_nodes),
        nb_node_in_error(0),
        nb_node_need_recover(0),
        nb_node_unreachable(0),
        nb_node_already_started(0),
        nb_node_ready(0),
        nb_node_ready_to_start(0)
    {}


    void operator ()(const std::string &node, exa_error_code error_code,
                     shared_ptr<const AdmindMessage> message)
    {
        int ret = EXA_SUCCESS;

        switch (error_code)
        {
        case EXA_SUCCESS:
            last_node_that_init = node;
            nb_node_ready++;
            exa_cli_log("\n%s: Ready to start", node.c_str());
            break;

        case EXA_ERR_CONNECT_SOCKET:
            /* Connection error to this node (already displayed by admind.cpp) */
            nb_node_unreachable++;
            exa_cli_warning("\n%sWARNING%s, %s: This node will not be started.",
                            COLOR_WARNING, COLOR_NORM, node.c_str());
            break;

        case ADMIND_ERR_NOTHINGTODO:
            /* This node is already ready to start */
            nb_node_ready_to_start++;
            last_node_that_init = node;
            exa_cli_warning(
                "\n%sWARNING%s, %s: This node is already ready to start.",
                COLOR_WARNING,
                COLOR_NORM,
                node.c_str());
            break;

        case EXA_ERR_ADMIND_STOPPED:
        case EXA_ERR_ADMIND_STOPPING:
            nb_node_in_error++;
            exa_cli_error("\n%sERROR%s, %s: Exanodes is not properly stopped.",
                          COLOR_ERROR, COLOR_NORM, node.c_str());
            break;

        case EXA_ERR_ADMIND_STARTING:
            nb_node_ready_to_start++;
            last_node_that_init = node;
            break;

        case EXA_ERR_ADMIND_NOCONFIG:
            exa_cli_warning("\n%sWARNING%s, %s: Is not configured, please use "
                            "exa_clnoderecover to initialize it", COLOR_WARNING,
                            COLOR_NORM, node.c_str());
            nb_node_need_recover++;
            break;

        case EXA_ERR_ADMIND_STARTED:
            exa_cli_warning("\n%sWARNING%s, %s: This node is already started",
                            COLOR_WARNING, COLOR_NORM, node.c_str());
            nb_node_already_started++;
            break;

        case ADMIND_ERR_QUORUM_TIMEOUT:
            exa_cli_error("\n%sERROR%s: %s", COLOR_ERROR, COLOR_NORM,
                          message->get_error_msg().c_str());
            break;

        default:
            nb_node_in_error++;
            exa_cli_error("\n%sERROR%s, %s: %s", COLOR_ERROR, COLOR_NORM,
                          node.c_str(),
                          message ? message->get_error_msg().c_str()
                          : exa_error_msg(error_code));
        }

        /* If a quorum FAILED to accept the command and cannot start, we
         * no more need to continue */
        ret = ((nb_node_unreachable + nb_node_need_recover)
               < nb_nodes / 2 + 1) ? EXA_SUCCESS : EXA_ERR_DEFAULT;

        if (ret != EXA_SUCCESS)
            exa_cli_warning(
                "\n%sWARNING%s: The cluster has not a quorum of nodes ready to accept this command. The command is aborted.\n",
                COLOR_WARNING,
                COLOR_NORM);
    }
};

void exa_clstart::run()
{
    string error_msg;
    unsigned int quorum;
    string msg_str;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    std::set<std::string> nodelist = exa.get_hostnames();

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    /* If a quorum succeeds to do the command, we assume it is a success */
    quorum = nodelist.size() / 2 + 1;

    /*
     * First non clustered part, send the clinit to admind
     * ---------------------------------------------------
     */
    AdmindCommand command_init("clinit", exa.get_cluster_uuid());

    clstart_filter myfilter(exa.get_hostnames().size());

    exa_cli_info("%-" exa_mkstr(
                     FMT_TYPE_H1) "s\n",
                 "Initializing the cluster (Please wait):");

    send_admind_by_node(command_init, nodelist, boost::ref(myfilter));

    /* Display the global status */
    if (myfilter.nb_node_unreachable
        + myfilter.nb_node_need_recover
        + myfilter.nb_node_already_started
        + myfilter.nb_node_ready_to_start
        + myfilter.nb_node_in_error == 0)
        exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s %sSUCCESS%s\n",
                     "Initializing the cluster",
                     COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    if (myfilter.nb_node_ready + myfilter.nb_node_ready_to_start < quorum)
    {
        if (myfilter.nb_node_already_started >= quorum)
        {
            if (myfilter.nb_node_ready)
                exa_cli_warning(
                    "\n"
                    "%sWARNING%s: The cluster is already started but some nodes are not.\n"
                    "         These nodes are being started now.\n\n",
                    COLOR_WARNING,
                    COLOR_NORM);
                /* Continue processing the clwaitstart in this case */
            else
            {
                exa_cli_warning(
                    "%sWARNING%s: The cluster '%s' is already started.\n",
                    COLOR_WARNING,
                    COLOR_NORM,
                    exa.get_cluster().c_str());
                throw CommandException(EXA_ERR_DEFAULT);
            }
        }
        else if (myfilter.nb_node_ready_to_start >= quorum)
        {
            exa_cli_warning(
                "%sWARNING%s: There are enough nodes ready to start, cluster will be started soon.\n",
                COLOR_WARNING,
                COLOR_NORM);
            throw CommandException(EXA_ERR_DEFAULT);
        }
        else throw CommandException(
                "There are not enough nodes ready to start.");
    }
    else
    {
        if (myfilter.nb_node_ready + myfilter.nb_node_ready_to_start <
            exa.get_hostnames().size())
            /* Not all nodes are ready to start, explain what's going on */
            exa_cli_warning(
                "\n"
                "%sWARNING%s: One or more nodes are not available for a cluster start.\n"
                "         The start will continue normally without these nodes.\n"
                "         When these nodes are available again, use exa_clnodestart\n"
                "         (or exa_clnoderecover --join) to start them within your Exanodes cluster.\n\n",
                COLOR_WARNING,
                COLOR_NORM);
        else if (myfilter.nb_node_ready_to_start)
            exa_cli_warning(
                "\n"
                "%sWARNING%s: One or more nodes are trying to start (or to stop).\n"
                "         - In case the nodes are starting, it will continue normally.\n"
                "         - In case some nodes are stopping, you should run an exa_clstop\n"
                "           followed by an exa_clstart to resync the nodes state.\n\n",
                COLOR_WARNING,
                COLOR_NORM);
    }
}


void exa_clstart::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);
    /* nothing to do */
}


void exa_clstart::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Start an Exanodes cluster.";
}


void exa_clstart::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Start the Exanodes cluster " << ARG_CLUSTERNAME
#ifdef WITH_FS
        << ". This command restarts the groups, volumes and file systems "
#else
        << ". This command restarts the groups and volumes "
#endif
        << "if they were started at previous exa_clstop." << std::endl;
}


void exa_clstart::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Start the cluster " << ARG_CLUSTERNAME << std::endl;
    out << "  " << "exa_clstart mycluster" << std::endl;
    out << std::endl;
}


