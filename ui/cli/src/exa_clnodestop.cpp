/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clnodestop.h"

#include "common/include/exa_mkstr.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_clnodestop::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                             "HOSTNAMES"));

exa_clnodestop::exa_clnodestop(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , all_nodes(false)
    , force(false)
    , recursive(false)
    , node_expand("")
    , ignore_offline(false)
{}


exa_clnodestop::~exa_clnodestop()
{}

void exa_clnodestop::init_options()
{
    exa_clcommand::init_options();

    /* FIXME At this point, all_nodes can only be true if set by the
     * constructor. Which is the case *iff* called from class clstop
     * (which inherits this class).
     * It is an ugly workaround to avoid having options -n and -a creep
     * up in clstop, where they do not belong (even more so since they
     * are mandatory).
     */
    if (!all_nodes)
    {
        add_option('n', "node", "Specify the nodes to stop.", 1, false, true,
                   OPT_ARG_NODE_HOSTNAMES);
        add_option('a', "all", "Stop all nodes of the cluster.", 1, false,
                   false);
        add_option('i', "ignore-offline", "Stop even though some disk groups "
                   "go offline.", 0, false, false);
    }
    add_option('f', "force", "Continue the stop even if something goes wrong. "
               "CAUTION! This option is very dangerous.", 0, true, false);
    /* FIXME this is buggy, recursive should only be authorized when
     * stoping all nodes (with the -a option OR when the regexp represents
     * all nodes. */
    add_option('r', "recursive", "Set the stopped state on all disk groups"
#ifdef WITH_FS
               ", volumes and file systems"
#else
               " and volumes"
#endif
               " so that the next exa_cl(node)start will start"
               " Exanodes and nothing more.", 0, false, false);
}


void exa_clnodestop::init_see_alsos()
{
    add_see_also("exa_expand");
    add_see_also("exa_clnodeadd");
    add_see_also("exa_clnodedel");
    add_see_also("exa_clnodestart");
    add_see_also("exa_clnoderecover");
}


struct clnodestop_filter : private boost::noncopyable
{
    Exabase &exa; /* FIXME this is needed for hostnames to nodenames resolution
                 *       but this is really ugly.... this should be reworked
                 *       when cleaning up Exabase */
    bool     got_error;

    clnodestop_filter(Exabase &_exa) :
        exa(_exa),
        got_error(false)
    {}


    void operator ()(const std::string &hostname, exa_error_code err_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        std::string nodename = exa.to_nodename(hostname);

        switch (err_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_CONNECT_SOCKET:
            /* Ignore this error, the shutdown will complain for that, anyway,
             * no neede to bother end user with this now */
            break;

        case ADMIND_ERR_NOTLEADER:
        case EXA_ERR_ADMIND_STOPPED:
        case EXA_ERR_ADMIND_STARTED:
        case EXA_ERR_ADMIND_STOPPING:
        case EXA_ERR_ADMIND_STARTING:
        case EXA_ERR_ADMIND_NOCONFIG:
            /* Ignore these errors. */
            break;

        default:
            exa_cli_error("%sERROR%s, %s: %s\n", COLOR_ERROR, COLOR_NORM,
                          nodename.c_str(),
                          message ? message->get_error_msg().c_str()
                          : exa_error_msg(err_code));
            /* This is really a serious error, that will prevent us from
             * sending the shutdown command. As a result, we set the
             * 'got_error' attribute to true.
             */
            got_error = true;
            break;
        }
    }
};

struct clshutdown_filter : private boost::noncopyable
{
    Exabase &exa; /* FIXME this is needed for hostnames to nodenames resolution
                 *       but this is really ugly.... this should be reworked
                 *       when cleaning up Exabase */
    size_t   nr_warnings;
    size_t   nr_errors;
    std::set<std::string> nodelist;
    std::set<std::string> nodes_allready_stopped;
    std::set<std::string> nodelist_remains;

    clshutdown_filter(Exabase &_exa, std::set<std::string> _nodelist) :
        exa(_exa),
        nr_warnings(0),
        nr_errors(0),
        nodelist(_nodelist),
        nodelist_remains(_nodelist)
    {}


    void operator ()(const std::string &hostname, exa_error_code err_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        std::string nodename = exa.to_nodename(hostname);

        switch (err_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_warning(
                "%sWARNING%s: %s is unreachable. (Even if requested, it won't be stopped.)\n",
                COLOR_WARNING,
                COLOR_NORM,
                nodename.c_str());
            nr_warnings++;
            break;

        case EXA_ERR_ADMIND_STOPPED:
            /* We don't care if it comes from a node that we are not currently stopping */
            if (nodelist.count(nodename))
            {
                nodes_allready_stopped.insert(nodename);
                nr_warnings++;
            }
            break;

        case EXA_ERR_ADMIND_NOCONFIG:
            /* We don't care if it comes from a node that we are not currently stopping */
            if (nodelist.count(nodename))
            {
                exa_cli_error(
                    "%sERROR%s: %s is not configured.\n"
                    "       Use exa_clnoderecover --join to include it in your cluster.\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    nodename.c_str());
                nr_errors++;
            }
            break;

        default:
            exa_cli_error("%sERROR%s, %s: %s\n", COLOR_ERROR, COLOR_NORM,
                          nodename.c_str(),
                          message ? message->get_error_msg().c_str()
                          : exa_error_msg(err_code));
            nr_errors++;
            break;
        }

        nodelist_remains.erase(nodename);

        /* if the answer was from a node we were trying to stop and that
         * was the last one to answer, display the warning */
        if (nodelist.count(nodename)
            && nodelist_remains.empty()
            && !nodes_allready_stopped.empty())
            exa_cli_warning("%sWARNING%s: %s %s already stopped.\n",
                            COLOR_WARNING, COLOR_NORM,
                            strjoin(" ",
                                    exa_unexpand(
                                        strjoin(" ", nodes_allready_stopped))
                                    ).c_str(),
                            nodes_allready_stopped.size() == 1 ?
                            "was" : "were");
    }
};

void exa_clnodestop::run()
{
    string err_msg;
    string msg_str;

    /* Set cluster (especially its UUID). */
    if (set_cluster_from_cache(_cluster_name.c_str(), err_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    std::set<std::string> nodelist;
    if (all_nodes)
    {
        /* An empty string means "all nodes". */
        nodelist = exa.get_nodenames();
        msg_str = "Stopping all nodes: ";
    }
    else
        try
        {
            nodelist = exa_expand(node_expand);
        }
        catch (string e)
        {
            throw CommandException(e);
        }
        msg_str = "Stopping node(s): ";
    msg_str += strjoin(" ", exa_unexpand(strjoin(" ", nodelist)));

    AdmindCommand command_stop("clnodestop", exa.get_cluster_uuid());
    command_stop.add_param("node_names", strjoin(" ", nodelist));
    command_stop.add_param("force", force);
    command_stop.add_param("recursive", recursive);
    if (ignore_offline)
        command_stop.add_param("ignore_offline", ignore_offline);

    clnodestop_filter stop_filter(exa);

    exa_cli_info("%s\n", msg_str.c_str());
    send_admind_by_node(command_stop, exa.get_hostnames(),
                                    std::ref(stop_filter));

    if (stop_filter.got_error)
    {
        exa_cli_error(
            "\n%sERROR%s: Failed to stop Exanodes on one or more nodes."
            " Please check the message above.\n",
            COLOR_ERROR,
            COLOR_NORM);
        throw CommandException(EXA_ERR_DEFAULT);
    }

    /* Non clustered part.
     * Send the EXA_CLSHUTDOWN command to the nodes to stop.
     */
    AdmindCommand command_shutdown("clshutdown", exa.get_cluster_uuid());
    command_shutdown.add_param("node_names", strjoin(" ", nodelist));

    clshutdown_filter shutdown_filter(exa, nodelist);

    send_admind_by_node(command_shutdown, exa.get_hostnames(),
                                    std::ref(shutdown_filter));

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", "Stopping requested nodes");

    if (shutdown_filter.nr_errors == 0 &&
        shutdown_filter.nodelist_remains.size() == 0)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    if (shutdown_filter.nr_errors > 0 || shutdown_filter.nodelist_remains.size())
    {
        exa_cli_error(
            "\n%sERROR%s: Failed to stop Exanodes on one or more nodes."
            " Please check the message above.\n",
            COLOR_ERROR,
            COLOR_NORM);
        throw CommandException(EXA_ERR_DEFAULT);
    }
}


void exa_clnodestop::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('n') != opt_args.end())
        node_expand = opt_args.find('n')->second;
    if (opt_args.find('a') != opt_args.end())
        all_nodes = true;
    if (opt_args.find('f') != opt_args.end())
        force = true;
    if (opt_args.find('r') != opt_args.end())
        recursive = true;
    if (opt_args.find('i') != opt_args.end())
        ignore_offline = true;
}


void exa_clnodestop::dump_short_description(std::ostream &out,
                                            bool show_hidden) const
{
    out << "Stop Exanodes on some nodes.";
}


void exa_clnodestop::dump_full_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Stop Exanodes on nodes " << OPT_ARG_NODE_HOSTNAMES <<
    " of the cluster "
        << ARG_CLUSTERNAME << "." << std::endl;
    out << OPT_ARG_NODE_HOSTNAMES <<
    " is a regular expansion (see exa_expand)." << std::endl;
}


void exa_clnodestop::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Stop Exanodes on nodes " << Boldify("node2") << " and " << Boldify(
        "node3")
        << " which belong to the cluster " << Boldify("mycluster") << ":" <<
    std::endl;
    out << "  " << "exa_clnodestop --node node/2-3/ mycluster" << std::endl;
    out << std::endl;
}


