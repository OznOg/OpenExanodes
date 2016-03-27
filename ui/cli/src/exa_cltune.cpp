/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <fstream>

#include "ui/cli/src/exa_cltune.h"

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
using std::shared_ptr;
using std::string;

/* TODO : grep all these constants and see if should be moved at an */
/* upper level (CommandOption or CommandArg for instance) */
const std::string exa_cltune::OPT_ARG_SAVE_FILE(Command::Boldify("FILE"));
const std::string exa_cltune::OPT_ARG_LOAD_FILE(Command::Boldify("FILE"));
const std::string exa_cltune::ARG_PARAMETER_PARAMETER(Command::Boldify(
                                                          "PARAMETER"));
const std::string exa_cltune::ARG_PARAMETER_VALUE(Command::Boldify("VALUE"));

exa_cltune::exa_cltune(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , display_params(false)
    , verbose(false)
{}


void exa_cltune::init_options()
{
    exa_clcommand::init_options();

    add_option('V', "verbose", "When listing parameters, show their "
               "description.", 0, false, false);

    add_option('l', "list", "List all parameters with their default and "
               "current values.", 1, false, false);

    add_option('L', "load", "Load all parameters from the given file.",
               1, false, true, OPT_ARG_LOAD_FILE);

    add_option('S', "save", "Save all current parameters in the given file.",
               1, false, true, OPT_ARG_SAVE_FILE);

    add_arg(ARG_PARAMETER_PARAMETER + "=[" + ARG_PARAMETER_VALUE + "]",
            1,
            false);
}


void exa_cltune::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
    add_see_also("exa_clstart");
    add_see_also("exa_clstop");
    add_see_also("exa_clinfo");
    add_see_also("exa_clstats");
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
class cltune_filter : private boost::noncopyable
{
    bool got_invalid_param;

public:

    bool got_error;

    cltune_filter() :
        got_invalid_param(false),
        got_error(false)
    {}


    void operator ()(const std::string &node, exa_error_code error_code,
                     std::shared_ptr<const AdmindMessage> message)
    {
        switch (error_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_INVALID_PARAM:
            got_error = true;
            if (!got_invalid_param)
                exa_cli_error(
                    "\n%sERROR%s: %s\n"
                    "       Please check possible parameter values with the --list option.",
                    COLOR_ERROR,
                    COLOR_NORM,
                    message->get_error_msg().c_str());
            got_invalid_param = true;
            break;

        case EXA_ERR_CONNECT_SOCKET:
            /* At connection time, an error has already been displayed */
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

void exa_cltune::run()
{
    string error_msg;
    exa_error_code error_code;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (display_params || !dump_file.empty())
    {
        display_all_params(_cluster_name);

        /* exit here when called just for parameter display */
        exa::Exception ex("", EXA_SUCCESS);
        throw ex;
    }

    if (!load_file.empty())
        error_code = send_param_from_file(load_file);
    else
    {
        string param, value;
        if (set_parameter(_parameter, param, value) != EXA_SUCCESS)
            throw CommandException(
                "The parameter '" + _parameter + "' is not suffixed with '='");
        error_code = send_single_param(param, value);
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


exa_error_code exa_cltune::send_param_from_file(string load_file)
{
    exa_error_code error_code = EXA_SUCCESS;
    xmlDocPtr tuneDocPtr(xml_conf_init_from_file(load_file.c_str()));
    xmlNodePtr node;
    int i;

    if (!tuneDocPtr)
    {
        exa_cli_error("Failed to parse file '%s'\n", load_file.c_str());
        return EXA_ERR_DEFAULT;
    }

    xmlNodeSetPtr node_set = xml_conf_xpath_query(tuneDocPtr,
                                                  "//Exanodes/tunables/tunable");
    xml_conf_xpath_result_for_each(node_set, node, i)
    {
        const char *param = xml_get_prop(node, "name");
        const char *value = xml_get_prop(node, "value");
        exa_error_code retval;

        if (!param || !value)
        {
            exa_cli_error(
                "At least one entry in the tunable file is missing a 'name'"
                "or a 'value'\n");
            break;
        }

        if ((retval = send_single_param(param, value)) != EXA_SUCCESS)
            error_code = retval;
    }
    xml_conf_xpath_free(node_set);

    return error_code;
}


exa_error_code exa_cltune::send_single_param(string param, string value)
{
    string value_to_set;

    std::set<std::string> nodelist = exa.get_hostnames();
    exa_error_code error_code;

    /* No value provided, get its default value */
    if (value.empty())
        value_to_set = "Set to default";
    else
        value_to_set = value;

    /* Display a summary to let the user know what we are going to do */
    exa_cli_info("Setting parameter: %47s %s\n",
                 param.c_str(),
                 value_to_set.c_str());

    /*
     * Send the command to each node
     */

    /* Create the command */
    unsigned int nb_error;
    int ret;

    AdmindCommand commandl("cltune", exa.get_cluster_uuid());
    commandl.add_param("param", param.c_str());
    commandl.add_param("value", value.c_str());

    cltune_filter myfilter;

    exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ",
                 "Setting parameter:");

    nb_error = send_admind_by_node(commandl, nodelist, std::ref(myfilter));

    /* Display the global status */
    if (!myfilter.got_error)
        exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    else
        exa_cli_info("\n");

    /* If a quorum succeeds to do the command, we assume it is a success */
    ret =
        (nb_error < exa.get_hostnames().size() / 2 +
         1) ? EXA_SUCCESS : EXA_ERR_DEFAULT;

    if (ret != EXA_SUCCESS)
    {
        exa_cli_error("The setting of parameters failed on %d nodes.\n",
                      nb_error);
        error_code = EXA_ERR_DEFAULT;
    }
    else
        error_code = EXA_SUCCESS;

    return error_code;
}


void exa_cltune::dump_param(xmlNodePtr nodePtr, std::ofstream &dumpfd)
{
    string name;
    string value;
    string default_value;

    name = xml_get_prop(nodePtr, EXA_PARAM_NAME);
    value = xml_get_prop(nodePtr, EXA_PARAM_VALUE);

    string line = "    <tunable name=\"" + name + "\" value=\"" + value +
                  "\"/>\n";
    dumpfd.write(line.c_str(), line.length());
}


void exa_cltune::display_param(xmlNodePtr nodePtr)
{
    string admind_value;
    string default_value;
    string description;
    string name;
    string value;
    const char *param_info;

    description = xml_get_prop(nodePtr, EXA_PARAM_DESCRIPTION);
    boost::algorithm::replace_all(description, "\n", "\n    ");

    value = xml_get_prop(nodePtr, EXA_PARAM_VALUE);
    name = xml_get_prop(nodePtr, EXA_PARAM_NAME);
    param_info = xml_get_prop(nodePtr, EXA_PARAM_TYPE_INFO);
    default_value = xml_get_prop(nodePtr, EXA_PARAM_DEFAULT);

    if (verbose)
    {
        if (value != default_value)
            admind_value = "Set to '" + value + "'";
        else
            admind_value = string("Set to default ('") + value + "')";

        exa_cli_info("  %s%s: %s%s\n"
                     "    %s\n",
                     value != default_value ? COLOR_WARNING : "",
                     name.c_str(),
                     admind_value.c_str(),
                     value != default_value ? COLOR_NORM : "",
                     description.c_str());

        /*Display message related to the type*/
        printf("    %s", param_info);

        printf("\n");
    }
    else
        exa_cli_info("%s%s: '%s'%s%s\n",
                     value != default_value ? COLOR_WARNING : "",
                     name.c_str(), value.c_str(),
                     value != default_value ? COLOR_NORM : "",
                     value == default_value ? " (default)" : "");
}


void exa_cltune::display_all_params(string cluster)
{
    shared_ptr<xmlDoc> config_ptr;
    string error_msg;
    std::ofstream dumpfd;
    EXA_ASSERT(!cluster.empty());

    if (!cluster.empty()
        && set_cluster_from_cache(cluster, error_msg) != EXA_SUCCESS)
    {
        exa::Exception ex(error_msg, EXA_ERR_DEFAULT);
        throw ex;
    }

    get_param(config_ptr);

    if (!config_ptr)
        throw CommandException(EXA_ERR_DEFAULT);

    /* Find our "param" xml node */
    int i;
    shared_ptr<xmlNodeSet> xmlnode_set(xml_conf_xpath_query(config_ptr.get(),
                                                            "//param"),
                                       __xml_conf_xpath_free);

    if (!dump_file.empty())
    {
        string line = string("<Exanodes>\n  <tunables>\n");
        dumpfd.open(dump_file.c_str(), std::ios::trunc);
        dumpfd.write(line.c_str(), line.length());
    }

    xmlNodePtr param_ptr;
    xml_conf_xpath_result_for_each(xmlnode_set.get(), param_ptr, i)
    {
        if (!dump_file.empty())
            dump_param(param_ptr, dumpfd);
        else
            display_param(param_ptr);
    }

    if (!dump_file.empty())
    {
        string line = "  </tunables>\n</Exanodes>\n";
        dumpfd.write(line.c_str(), line.length());

        dumpfd.close();

        if (dumpfd.fail())
        {
            exa::Exception ex(string("Failed to write in '")
                              + dump_file
                              + string("'."), EXA_ERR_DEFAULT);
            throw ex;
        }
    }
}


void exa_cltune::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    exa_clcommand::parse_non_opt_args(non_opt_args);

    if (!display_params && dump_file.empty() && load_file.empty())
        _parameter = non_opt_args.at(1);
}


void exa_cltune::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('l') != opt_args.end())
        display_params = true;

    if (opt_args.find('S') != opt_args.end())
        dump_file = opt_args.find('S')->second;

    if (opt_args.find('L') != opt_args.end())
        load_file = opt_args.find('L')->second;

    if (opt_args.find('V') != opt_args.end())
    {
        if (!display_params)
            throw CommandException("Option -V can only be specified with -l");
        verbose = true;
    }
}


void exa_cltune::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Tune internal parameters of an Exanodes cluster.";
}


void exa_cltune::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Set the value " << ARG_PARAMETER_VALUE << " to the parameter "
        << ARG_PARAMETER_PARAMETER << " of stopped the cluster "
        << ARG_CLUSTERNAME << "." << std::endl;

    out << "Use the exa_cltune --list option to display all parameters, "
        << "their default, and their current values. If " <<
    ARG_PARAMETER_VALUE
        << " is empty, the default value will be assigned to the parameter." <<
    std::endl;
}


void exa_cltune::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Set the time between each heartbeat message send to "
        << Boldify("10") << " seconds:" << std::endl;
    out << "  " << "exa_cltune mycluster heartbeat_period=10" << std::endl;
    out << std::endl;

    out <<
    "Set the time between each heartbeat message to its default value:" <<
    std::endl;
    out << "  " << "exa_cltune mycluster heartbeat_period=" << std::endl;
    out << std::endl;

    out << "Display the current value of all the parameters of cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_cltune --list mycluster" << std::endl;
    out << std::endl;
}


