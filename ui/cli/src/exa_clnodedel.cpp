/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clnodedel.h"

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"

using std::shared_ptr;
using std::string;

const std::string exa_clnodedel::OPT_ARG_NODE_HOSTNAME(Command::Boldify(
                                                           "HOSTNAME"));

exa_clnodedel::exa_clnodedel()
{
    add_option('n', "node", "Specify the node to remove from the cluster.", 1,
               false, true, OPT_ARG_NODE_HOSTNAME);
}


void exa_clnodedel::init_see_alsos()
{
    add_see_also("exa_clnodeadd");
    add_see_also("exa_clnodestart");
    add_see_also("exa_clnodestop");
    add_see_also("exa_clnoderecover");
}


void exa_clnodedel::run()
{
    string error_msg;
    exa_error_code error_code;
    string msg_str;

    if (set_cluster_from_cache(_cluster_name.c_str(), error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    msg_str = "Removing node '" + _node + "' from cluster '" +
              exa.get_cluster() + "'";

    AdmindCommand command_del("clnodedel", exa.get_cluster_uuid());
    command_del.add_param("nodename", _node);

    shared_ptr<AdmindMessage> message_del(
        send_command(command_del, msg_str, error_code, error_msg));

    if (message_del)
        if (error_code == EXA_SUCCESS)
        {
            if (!message_del->get_payload().empty())
            {
                if (exa.set_config_node_del(message_del->get_payload(),
                                            error_msg) != EXA_SUCCESS)
                {
                    exa_cli_error(
                        "%sERROR%s: "
                        "The removal of your node has been successful but we failed "
                        "to update the local node cache:\n"
                        "       %s\n",
                        COLOR_ERROR,
                        COLOR_NORM,
                        error_msg.c_str());
                    error_code = EXA_ERR_DEFAULT;
                }

                /* Now send the CLDELETE to this node */
                AdmindCommand command_delete("cldelete",
                                             exa.get_cluster_uuid());
                command_delete.add_param("recursive", ADMIND_PROP_TRUE);

                shared_ptr<AdmindMessage> message_delete(
                    send_admind_to_node(
                        message_del->get_payload(), command_delete,
                        error_code));

                /* We can assume the node is not accessible and won't
                 * handle the CLDELETE but it's not a problem because other
                 * nodes know it is deleted. If it comes back with it won't
                 * be accepted anyway until a clnodeadd is sent
                 */
                if (error_code != EXA_SUCCESS)
                {
                    if (message_delete
                        && !message_delete->get_error_msg().empty())
                        exa_cli_warning("%sWARNING%s: %s\n",
                                        COLOR_WARNING, COLOR_NORM,
                                        message_delete->get_error_msg().c_str());

                    error_code = EXA_SUCCESS;
                }
            }
            else
                throw CommandException(
                    "Admind did not provide the hostname.\n"
                    "       Cannot remove the hostname from the local node cache.");
        }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_clnodedel::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('n') != opt_args.end())
        _node = opt_args.find('n')->second;
}


void exa_clnodedel::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Remove a node and its disks from an Exanodes cluster.";
}


void exa_clnodedel::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Remove the node " << OPT_ARG_NODE_HOSTNAME << " from the cluster "
        << ARG_CLUSTERNAME << ".";
    out <<
    "The cluster must be started but the node to delete must be stopped." <<
    std::endl;
    out <<
    "The command fails if a disk of the node to delete is used in a disk group."
   << std::endl;
    out << "You have to delete this disk group before." << std::endl;
}


void exa_clnodedel::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Remove the node " << Boldify("node3") << " from the cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_clnodedel --node node3 mycluster" << std::endl;
    out << std::endl;
}


