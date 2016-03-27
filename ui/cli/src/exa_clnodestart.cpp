/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clnodestart.h"

#include "common/include/exa_mkstr.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_clnodestart::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                              "HOSTNAMES"));

exa_clnodestart::exa_clnodestart(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , node_expand("")
    , all_nodes(false)
{}


exa_clnodestart::~exa_clnodestart()
{}

void exa_clnodestart::init_options()
{
    exa_clcommand::init_options();

    add_option('n', "node", "Specify the nodes to start.", 1, false, true,
               OPT_ARG_NODE_HOSTNAMES);
    add_option('a', "all", "Start all nodes of the cluster.", 1, false, false);
}


void exa_clnodestart::init_see_alsos()
{
    add_see_also("exa_expand");
    add_see_also("exa_clnodeadd");
    add_see_also("exa_clnodedel");
    add_see_also("exa_clnodestop");
    add_see_also("exa_clnoderecover");
}


struct clinit_filter : private boost::noncopyable
{
    size_t nr_already_started;

    clinit_filter() :
        nr_already_started(0)
    {}


    void operator ()(const std::string &node, exa_error_code err_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (err_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_warning(
                "\n%sWARNING%s: %s is unreachable. It won't be started.\n",
                COLOR_WARNING,
                COLOR_NORM,
                node.c_str());
            break;

        case EXA_ERR_ADMIND_STARTED:
            exa_cli_warning("\n%sWARNING%s: %s is already started.\n",
                            COLOR_WARNING, COLOR_NORM, node.c_str());
            nr_already_started++;
            break;

        case EXA_ERR_ADMIND_STARTING:
            break;

        case EXA_ERR_ADMIND_NOCONFIG:
            exa_cli_error(
                "\n%sERROR%s: %s is not configured.\n"
                "       Use exa_clnoderecover --join to include it in your cluster.\n",
                COLOR_ERROR,
                COLOR_NORM,
                node.c_str());
            break;

        case ADMIND_ERR_QUORUM_TIMEOUT:
            exa_cli_error("\n%sERROR%s: %s", COLOR_ERROR, COLOR_NORM,
                          message->get_error_msg().c_str());
            break;

        default:
            if (get_error_type(err_code) == ERR_TYPE_WARNING)
                exa_cli_warning("\n%sWARNING%s: %s",
                                COLOR_WARNING,
                                COLOR_NORM,
                                exa_error_msg(err_code));
            else if (get_error_type(err_code) == ERR_TYPE_ERROR)
                exa_cli_error("\n%sERROR%s: %s",
                              COLOR_ERROR,
                              COLOR_NORM,
                              exa_error_msg(err_code));
            break;
        }
    }
};

void exa_clnodestart::run()
{
    exa_error_code err_clinit = EXA_SUCCESS;
    string msg_str;
    unsigned int nr_errors;

    string err_msg;

    /* Set cluster (especially its UUID). */
    if (set_cluster_from_cache(_cluster_name.c_str(), err_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    std::set<std::string> nodelist;
    if (all_nodes)
    {
        nodelist = exa.get_hostnames();
        msg_str = "Starting all nodes";
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
        msg_str = "Starting one or several nodes";

    /* Non clustered part.
     * Send the "clinit" command to the nodes to start.
     */
    AdmindCommand command_init("clinit", exa.get_cluster_uuid());

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s\n", msg_str.c_str());

    clinit_filter myfilter;
    nr_errors = send_admind_by_node(command_init, nodelist,
                                    std::ref(myfilter));

    if (nr_errors)
    {
        exa_cli_info("\n");
        err_clinit =
            (myfilter.nr_already_started ==
             nr_errors) ? EXA_SUCCESS : EXA_ERR_DEFAULT;
    }
    else
        err_clinit = EXA_SUCCESS;

    if (all_nodes)
        msg_str = "Start all nodes";
    else
        msg_str = "Start one or several nodes";
    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", msg_str.c_str());
    if (err_clinit == EXA_SUCCESS)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else if (err_clinit == EXA_ERR_DEFAULT)
        exa_cli_error("%sERROR%s\n", COLOR_ERROR, COLOR_NORM);

    if (err_clinit != EXA_SUCCESS)
        throw CommandException(err_clinit);
}


void exa_clnodestart::parse_opt_args(const std::map<char,
                                                    std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('n') != opt_args.end())
        node_expand = opt_args.find('n')->second;
    if (opt_args.find('a') != opt_args.end())
        all_nodes = true;
}


void exa_clnodestart::dump_short_description(std::ostream &out,
                                             bool show_hidden) const
{
    out << "Start Exanodes on some nodes.";
}


void exa_clnodestart::dump_full_description(std::ostream &out,
                                            bool show_hidden) const
{
    out << "Start Exanodes on nodes " << OPT_ARG_NODE_HOSTNAMES <<
    " of the cluster "
        << ARG_CLUSTERNAME << "." << std::endl;
    out << OPT_ARG_NODE_HOSTNAMES <<
    " is a regular expansion (see exa_expand)." << std::endl;
}


void exa_clnodestart::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Start Exanodes on nodes " << Boldify("node2") << " and " << Boldify(
        "node3")
        << " which belong to the cluster " << Boldify("mycluster") << ":" <<
    std::endl;
    out << "  " << "exa_clnodestart --node node/2-3/ mycluster" << std::endl;
    out << std::endl;
}


