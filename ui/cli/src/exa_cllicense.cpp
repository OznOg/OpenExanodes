/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <fstream>
#include <iostream>

#include "ui/cli/src/exa_cllicense.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_mkstr.h"
#include "common/include/uuid.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

using boost::lexical_cast;
using boost::shared_ptr;
using std::string;

const std::string exa_cllicense::OPT_ARG_FILENAME(Command::Boldify("FILENAME"));
const std::string exa_cllicense::OPT_ARG_HOSTNAME(Command::Boldify("HOSTNAME"));

exa_cllicense::exa_cllicense(int argc, char *argv[])
    : exa_clcommand(argc, argv),
    _license_file()
{}


exa_cllicense::~exa_cllicense()
{}


void exa_cllicense::init_options()
{
    exa_clcommand::init_options();

    add_option('s', "set-license",
               "Deploy a new license on all the nodes of the cluster.",
               1, false, true, OPT_ARG_FILENAME);
}


void exa_cllicense::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
}


/** @brief filter error received by each nodes in a send_admind_by_node sequence */
struct getclustername_filter_t : private boost::noncopyable
{
    bool         got_error;
    unsigned int connect_socket_error;
    string       ref_cluster_name;
    unsigned int nb_node_match;

    getclustername_filter_t(string cluster_name) :
        got_error(false),
        connect_socket_error(0),
        ref_cluster_name(cluster_name),
        nb_node_match(0)
    {}


    /** @brief filter error received by a node
     *
     * @param[in] node         node replying
     * @param[in] error_code   error received
     * @param[in] message      message received
     *
     */
    void operator ()(const std::string &node, exa_error_code error_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_ERR_ADMIND_NOCONFIG:
            exa_cli_error("\n%sERROR%s, %s: Cluster not configured",
                          COLOR_ERROR, COLOR_NORM, node.c_str());
            got_error = true;
            break;

        case EXA_SUCCESS:
            if (ref_cluster_name == message->get_payload())
                nb_node_match++;
            else
            {
                exa_cli_error(
                    "\n%sERROR%s, %s: Cluster %s configured while %s is expected ",
                    COLOR_ERROR,
                    COLOR_NORM,
                    node.c_str(),
                    message->get_payload().c_str(),
                    ref_cluster_name.c_str());
                got_error = true;
            }
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_error("\n%sERROR%s, %s: Failed to connect",
                          COLOR_ERROR, COLOR_NORM, node.c_str());
            connect_socket_error++;
            got_error = true;
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

/** @brief filter error received by each nodes in a send_admind_by_node sequence */
class setlicense_filter_t : private boost::noncopyable
{
public:

    bool got_error;
    unsigned int connect_socket_error;

    setlicense_filter_t() :
        got_error(false),
        connect_socket_error(0)
    {}


    /** @brief filter error received by a node
     *
     * @param[in] node         node replying
     * @param[in] error_code   error received
     * @param[in] message      message received
     *
     */
    void operator ()(const std::string &node, exa_error_code error_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_error("\n%sERROR%s, %s: Failed to connect",
                          COLOR_ERROR, COLOR_NORM, node.c_str());
            connect_socket_error++;
            got_error = true;
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

void exa_cllicense::run()
{
    string license_str;
    string error_msg;
    xmlDocPtr license_dptr = NULL;

    string error_connection_msg(
        "Failed to contact one or more nodes of your cluster.\n"
        " - First, check you can reach your nodes on your network,\n"
        " - then check exa_admind daemon is started on them.\n");

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    /*******************************   STEP 1   ********************************/

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", "Checking nodes are stopped");

    AdmindCommand command_getname("get_cluster_name", exa_uuid_zero);
    getclustername_filter_t getclustername_filter(exa.get_cluster());

    send_admind_by_node(command_getname, exa.get_hostnames(),
                        boost::ref(getclustername_filter));

    /* Display the global status */
    if (getclustername_filter.got_error)
    {
        exa_cli_info("\n");
        if (getclustername_filter.connect_socket_error)
            throw CommandException(error_connection_msg);
        else
            throw CommandException(EXA_ERR_DEFAULT);
    }

    exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);

    /*******************************   STEP 2   ********************************/

    /* Read the license file and encapsulate it into XML */
    EXA_ASSERT(!_license_file.empty());
    string line;
    std::ifstream license_fs(_license_file.c_str(), std::ios::in);

    license_dptr = xml_new_doc("1.0");
    license_dptr->children = xml_new_doc_node(license_dptr,
                                              NULL,
                                              "license",
                                              NULL);

    if (license_fs.fail())
        throw CommandException("Failed to read file " + _license_file);

    while (getline(license_fs, line))
        license_str += line + "\n";

    license_fs.close();
    xml_set_prop(license_dptr->children, "raw", license_str.c_str());

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Pushing new license on the nodes");

    /* Create command */
    AdmindCommand setlicense_command("setlicense", exa.get_cluster_uuid());
    setlicense_filter_t setlicense_filter;
    setlicense_command.add_param("license", license_dptr);

    /* Send the command and receive the response */
    send_admind_by_node(setlicense_command, exa.get_hostnames(),
                        boost::ref(setlicense_filter));

    if (setlicense_filter.got_error)
    {
        exa_cli_info("\n");
        if (getclustername_filter.connect_socket_error)
            throw CommandException(error_connection_msg);
        else
            throw CommandException(EXA_ERR_DEFAULT);
    }
    exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);

    if (exa.set_license(license_str, error_msg) != EXA_SUCCESS)
        exa_cli_error("%sERROR%s: Failed to update the local cluster cache:\n"
                      "       %s\n", COLOR_ERROR, COLOR_NORM, error_msg.c_str());
}


void exa_cllicense::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('s') != opt_args.end())
        _license_file = opt_args.find('s')->second;
}


void exa_cllicense::dump_short_description(std::ostream &out,
                                           bool show_hidden) const
{
    out << "Manage the license installed on a cluster." << std::endl;
}


void exa_cllicense::dump_full_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Manage the license installed on a cluster." << std::endl;
    out << "The command must be called on a stopped cluster." << std::endl;
}


void exa_cllicense::dump_examples(std::ostream &out, bool show_hidden) const
{
    Subtitle(out, "Change the cluster license:");
    out << "Deploy the license " << Boldify("new_license.txt")
        << " on cluster " << Boldify("mycluster") << ":" << std::endl;
    out << "       exa_cllicense --set-license new_license.txt mycluster" <<
    std::endl;
    out << std::endl;
}


