/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clnoderecover.h"

#include "os/include/os_network.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_mkstr.h"
#include "common/include/uuid.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/common_utils.h"

#include <fstream>

using std::string;
using std::shared_ptr;

const std::string exa_clnoderecover::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                                "HOSTNAMES"));

exa_clnoderecover::exa_clnoderecover()
{
    add_option('n', "node", "Specify the node(s) to recover.", 1, false, true,
               OPT_ARG_NODE_HOSTNAMES);
    add_option('j', "join", "Request a recovery of Exanodes configuration.",
               2, false, false);
    add_option('l', "leave", "Request an uninitialization of Exanodes.",
               2, false, false);
}


void exa_clnoderecover::init_see_alsos()
{
    add_see_also("exa_expand");
    add_see_also("exa_clnodeadd");
    add_see_also("exa_clnodedel");
    add_see_also("exa_clnodestart");
    add_see_also("exa_clnodestop");
}


void exa_clnoderecover::run()
{
    exa_error_code error_code(EXA_ERR_DEFAULT);

    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_trace("cluster=%s\n", exa.get_cluster().c_str());

    switch (action)
    {
    case JOIN:
        if ((error_code = node_join()) == EXA_SUCCESS)
            error_code = node_init();
        break;

    case LEAVE:
        error_code = node_leave();
        break;

    case NONE:
        throw CommandException("Please specify and action for this command, "
                               "--join or --leave.");
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


exa_error_code exa_clnoderecover::node_join()
{
    bool got_error(false);

    shared_ptr<xmlDoc> cfg;
    exa_error_code error_code = get_configclustered(cfg);

    if (error_code != EXA_SUCCESS)
        throw CommandException("Please check exanodes is started");

    if (!cfg || !cfg->children || !cfg->children->name
        || !xmlStrEqual(cfg->children->name, BAD_CAST("Exanodes")))
        throw CommandException(
            "Failed to parse admind returned initialization file");

    /* The initialization file is ready now */
    {
        string error_message;
        if (exa.update_cache_from_config(cfg.get(),
                                         error_message) != EXA_SUCCESS)
            exa_cli_warning("%sWARNING%s: %s\n", COLOR_WARNING, COLOR_NORM,
                            error_message.c_str());
    }

    /* Send it to each nodes */
    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Recover nodes:");

    AdmindCommand command_create("clcreate", exa_uuid_zero);
    command_create.add_param("config", cfg.get());

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

    command_create.add_param("join", ADMIND_PROP_TRUE);
    for (std::set<std::string>::iterator it = nodes.begin();
         it != nodes.end();
         ++it)
    {
        string node = *it;

        char canonical_hostname[EXA_MAXSIZE_HOSTNAME + 1];
        /* FIXME: fail if cannot get canonical name */
        if (os_host_canonical_name(node.c_str(), canonical_hostname,
                                   sizeof(canonical_hostname)) == 0)
            command_create.replace_param("hostname", string(canonical_hostname));

        shared_ptr<AdmindMessage> message(
            send_admind_to_node(node, command_create, error_code));

        if (error_code != EXA_SUCCESS)
        {
            if (!got_error)
                exa_cli_info("\n");

            got_error = true;

            switch (error_code)
            {
            case EXA_ERR_DEFAULT:
                exa_cli_error(
                    "%sERROR%s: Recovery of node '%s' failed. Please "
                    "check exanodes is started on this node.\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_CONNECT_SOCKET:
                exa_cli_error("%sERROR%s: Cannot recover node '%s', node is "
                              "unreachable\n", COLOR_ERROR, COLOR_NORM,
                              node.c_str());
                break;

            case ADMIND_ERR_CLUSTER_ALREADY_CREATED:
                exa_cli_error(
                    "%sERROR%s: Recovery of node '%s' is not possible "
                    "because it's already created:\n       %s\n"
                    "       To start this node again, just run "
                    "exa_clnodestart\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_error_msg().c_str());
                break;

            default:
                exa_cli_error(
                    "%sERROR%s: Recovery of node '%s' failed with error:\n"
                    "  %s\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_error_msg().c_str());
            }
        }
    }

    if (!got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);

    return got_error ? EXA_ERR_DEFAULT : EXA_SUCCESS;
}


exa_error_code exa_clnoderecover::node_init()
{
    bool got_error(false);
    exa_error_code error_code(EXA_ERR_DEFAULT);
    AdmindCommand command_init("clinit", exa.get_cluster_uuid());

    /* Now run clinit on each node */
    exa_cli_info("Initializing the nodes (Please wait):\n");

    for (std::set<std::string>::iterator it = nodes.begin();
         it != nodes.end();
         ++it)
    {
        string node = *it;
        shared_ptr<AdmindMessage> message(
            send_admind_to_node(node, command_init, error_code));

        if (error_code != EXA_SUCCESS)
        {
            got_error = true;
            switch (error_code)
            {
            case  EXA_ERR_DEFAULT:
                exa_cli_error(
                    "%sERROR%s: Initialization of node '%s' failed. "
                    "Please check exanodes is started on this node.\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_CONNECT_SOCKET:
                exa_cli_error("%sERROR%s, %s: Cannot initialize, node is "
                              "unreachable\n", COLOR_ERROR, COLOR_NORM,
                              node.c_str());
                break;

            case EXA_ERR_ADMIND_NOCONFIG:
                exa_cli_warning(
                    "\n%sWARNING%s, %s: Is not configured, please use "
                    "exa_clnoderecover --join to include it in your "
                    "cluster\n",
                    COLOR_WARNING,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_ADMIND_STOPPING:
            case EXA_ERR_ADMIND_STARTING:
            case EXA_ERR_ADMIND_STARTED:
            case EXA_ERR_ADMIND_STOPPED:
                exa_cli_error("%sERROR%s, %s: %s\n", COLOR_ERROR, COLOR_NORM,
                              node.c_str(), message->get_error_msg().c_str());
                break;

            default:
                exa_cli_error(
                    "%sERROR%s, %s: Initialization failed with error:\n"
                    "  %s\n",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_error_msg().c_str());
            }
        }
    }

    if (!got_error)
        exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s %sSUCCESS%s\n",
                     "Node initialization:", COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    return got_error ? EXA_ERR_DEFAULT : EXA_SUCCESS;
}


exa_error_code exa_clnoderecover::node_leave()
{
    AdmindCommand command("cldelete", exa.get_cluster_uuid());

    command.add_param("recursive", ADMIND_PROP_TRUE);

    bool got_error(false);

    /* Now run cldelete on each node */
    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Ask the nodes to leave the cluster:");
    exa_cli_log("\n");

    for (std::set<std::string>::iterator it = nodes.begin();
         it != nodes.end();
         ++it)
    {
        string node = *it;
        exa_error_code error_code;

        shared_ptr<AdmindMessage> message(
            send_admind_to_node(node, command, error_code));

        if (error_code != EXA_SUCCESS)
        {
            got_error = true;
            switch (error_code)
            {
            case EXA_ERR_DEFAULT:
                exa_cli_error(
                    "\n%sERROR%s: Initialization of node '%s' failed. "
                    "Please check exanodes is started on this node.",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_CONNECT_SOCKET:
                exa_cli_error("\n%sERROR%s, %s: Cannot initialize, node is "
                              "unreachable", COLOR_ERROR, COLOR_NORM,
                              node.c_str());
                break;

            case EXA_ERR_ADMIND_NOCONFIG:
                exa_cli_warning(
                    "\n%sWARNING%s, %s: \nIs not configured, please use "
                    "exa_clnoderecover --join to include it in your cluster",
                    COLOR_WARNING,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_ADMIND_STARTED:
                exa_cli_error(
                    "\n%sERROR%s, %s: Is already started you must stop "
                    "your cluster with exa_clstop or exa_clnodestop first.",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str());
                break;

            case EXA_ERR_ADMIND_STOPPING:
            case EXA_ERR_ADMIND_STARTING:
            case EXA_ERR_ADMIND_STOPPED:
                exa_cli_error("\n%sERROR%s, %s: %s", COLOR_ERROR, COLOR_NORM,
                              node.c_str(), message->get_error_msg().c_str());
                break;

            default:
                exa_cli_error(
                    "\n%sERROR%s, %s: Node leave failed with error:\n  %s",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_error_msg().c_str());
            }
        }
    }

    if (!got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    return got_error ? EXA_ERR_DEFAULT : EXA_SUCCESS;
}


void exa_clnoderecover::parse_opt_args(const std::map<char,
                                                      std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('n') != opt_args.end())
        nodes = exa_expand(opt_args.find('n')->second);

    if (opt_args.find('j') != opt_args.end())
        action = JOIN;
    if (opt_args.find('l') != opt_args.end())
        action = LEAVE;
}


void exa_clnoderecover::dump_short_description(std::ostream &out,
                                               bool show_hidden) const
{
    out << "Recover or cleanup Exanodes configuration on "
        << "some nodes that belong to an Exanodes cluster.";
}


void exa_clnoderecover::dump_full_description(std::ostream &out,
                                              bool show_hidden) const
{
    out <<
    "exa_clnoderecover --join recovers the Exanodes configuration on nodes "
        << OPT_ARG_NODE_HOSTNAMES << " that already belong to the cluster " <<
    ARG_CLUSTERNAME
        << ", eg. after a complete reinstallation of the system. "
        << "This command should be followed by exa_clnodestart." << std::endl;

    out << "exa_clnoderecover --leave uninitializes Exanodes on nodes " <<
    OPT_ARG_NODE_HOSTNAMES
        <<
    ". These nodes will not be capable to join the Exanodes cluster anymore, "
        << "but they will still be known by the other nodes of the cluster "
        << ARG_CLUSTERNAME << "." << std::endl;

    out << OPT_ARG_NODE_HOSTNAMES <<
    " is a regular expansion (see exa_expand)." << std::endl;

    out <<
    "WARNING! --leave do not completely remove the nodes from the cluster. "
        << "To remove nodes use the command exa_clnodedel instead." <<
    std::endl;
}


void exa_clnoderecover::dump_examples(std::ostream &out,
                                      bool show_hidden) const
{
    out << "Recover Exanodes configuration on " << Boldify("node2") <<
    " in cluster " << ARG_CLUSTERNAME
                   << " after it has been reinstalled:" << std::endl;
    out << "  " << "exa_clnoderecover --join --node node2 mycluster" <<
    std::endl;
    out << std::endl;
}


