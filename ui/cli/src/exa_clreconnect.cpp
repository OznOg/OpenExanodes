/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_clreconnect.h"

#include "common/include/uuid.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"

using std::shared_ptr;
using std::string;

const std::string exa_clreconnect::OPT_ARG_NODE_HOSTNAME(Command::Boldify(
                                                             "HOSTNAME"));

exa_clreconnect::exa_clreconnect()
{
    add_option('n', "node", "One of the nodes of the cluster.", 1, false, true,
               OPT_ARG_NODE_HOSTNAME);

    add_see_also("exa_clnodeadd");
    add_see_also("exa_clnodedel");
    add_see_also("exa_clnodestart");
    add_see_also("exa_clnodestop");
}


void exa_clreconnect::run()
{
    string error_msg;
    string license;

    /* Reconnect if requested */
    /* exa.set_cluster_from_node(argv[optind], reconnect_node); */

    AdmindCommand command("getconfig", exa_uuid_zero);
    exa_error_code dummy1; /* Unused */

    shared_ptr<AdmindMessage> message(send_admind_to_node(_reconnect_node,
                                                          command, dummy1));

    if (!message)
        throw CommandException(
            "Error retrieving the list of nodes "
            "of the cluster from node '" + _reconnect_node +
            "'.\n"
            "       Please try again this command with another node.");

    shared_ptr<xmlDoc> config_ptr(
        xmlReadMemory(message->get_payload().c_str(),
                      message->get_payload().size(),
                      NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOERROR |
                      XML_PARSE_NOWARNING),
        xmlFreeDoc);

    if (message->get_error_code() == EXA_ERR_ADMIND_NOCONFIG)
        throw CommandException(
            _reconnect_node + ": This node is not configured.");
    else if (get_error_type(message->get_error_code()) != ERR_TYPE_SUCCESS)
        throw CommandException(_reconnect_node + ": " +
                               exa_error_msg(message->get_error_code()));
    else if (!config_ptr || !config_ptr->children ||
             !config_ptr->children->name
             || !xmlStrEqual(config_ptr->children->name, BAD_CAST("Exanodes")))
        throw CommandException("Failed to receive a valid response from admind");

    /* Retrieve the license if any */

    AdmindCommand license_command("getlicense", exa_uuid_zero);
    exa_error_code error_code;
    shared_ptr<AdmindMessage> license_message(send_admind_to_node(
                                                  _reconnect_node,
                                                  license_command,
                                                  error_code));

    if (error_code == EXA_SUCCESS)
        license = license_message->get_payload();

    if (exa.set_cluster_from_config(_cluster_name,
                                    config_ptr.get(),
                                    license,
                                    error_msg) != EXA_SUCCESS)
        throw CommandException(error_msg);

    /* Check it now to display the status */
    if (exa.set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
    {
        exa_cli_error("%s\n", error_msg.c_str());
        throw CommandException(
            "The node list for cluster '" + _cluster_name +
            "' has been retrieved, but we failed to save it.");
    }
    else exa_cli_info(
            "%sSUCCESS%s: The node list for cluster '%s' has been created. You can manage your cluster now.\n",
            COLOR_INFO,
            COLOR_NORM,
            _cluster_name.c_str());
}  /* exa_clreconnect::run */


void exa_clreconnect::parse_opt_args(const std::map<char,
                                                    std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('n') != opt_args.end())
        _reconnect_node = opt_args.find('n')->second;
}


void exa_clreconnect::dump_short_description(std::ostream &out,
                                             bool show_hidden) const
{
    out << "Reconnect the administration machine to an Exanodes cluster.";
}


void exa_clreconnect::dump_full_description(std::ostream &out,
                                            bool show_hidden) const
{
    out << "Reconnect the administration machine to the cluster " <<
    ARG_CLUSTERNAME
        << ", retrieving information from the node " << OPT_ARG_NODE_HOSTNAME
        <<
    ". This command is useful when the administration machine does not know the cluster"
        << " anymore, eg. after a reinstallation." << std::endl;
}


void exa_clreconnect::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Reconnect to the cluster " << Boldify("mycluster") <<
    " using the node "
        << Boldify("node1") << "." << std::endl;
    out << Boldify("node1") <<
    " must be running and be part of the cluster:" << std::endl;
    out << "  " << "exa_clreconnect --node node1 mycluster" << std::endl;
    out << std::endl;
}


