/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_vltune.h"

#include <boost/lexical_cast.hpp>

#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"

using std::string;
using boost::shared_ptr;
using boost::lexical_cast;

const std::string exa_vltune::ARG_PARAMETER_PARAMETER(Command::Boldify(
                                                          "PARAMETER"));
const std::string exa_vltune::ARG_PARAMETER_VALUE(Command::Boldify("VALUE"));

exa_vltune::exa_vltune(int argc, char *argv[])
    : exa_vlcommand(argc, argv)
#ifdef WITH_FS
    , nofscheck(false)
#endif
    , _vltune_mode(VLTUNE_NONE)
    , verbose(false)
{}


exa_vltune::~exa_vltune()
{}

void exa_vltune::init_options()
{
    exa_vlcommand::init_options();

    std::set<int> exclude_all;
    exclude_all.insert(1);
    exclude_all.insert(2);

    add_option('l', "list", "List all parameters with their default and "
               "current values.", exclude_all, false, false);

    add_option('p', "param", "The parameter to be tuned.", 1, false, true,
               ARG_PARAMETER_PARAMETER);

    add_option('s', "set", "Set the parameter to the value.", 2, false, true,
               ARG_PARAMETER_VALUE);
    add_option('g', "get", "Get the value of a parameter.", 2, false, false);
    add_option('a', "add", "Add a value to the parameter (only for list "
               "parameters).", 2, false, true, ARG_PARAMETER_VALUE);
    add_option('r', "remove", "Remove a value from the parameter (only for "
               "list parameters).", 2, false, true, ARG_PARAMETER_VALUE);
    add_option('z', "reset", "Reset the parameter value to the default.",
               2, false, false);

    add_option('V', "verbose", "When listing parameters, show their "
               "description.", 0, false, false);

#ifdef WITH_FS
    add_option('F', "nofscheck", "Set the parameter even if the volume is "
               "part of a file system. WARNING: you should always use "
               "'exa_fs*' commands to manage a file system.",
               0, true, false);
#endif

    add_arg(ARG_PARAMETER_PARAMETER + "=" + "[" + ARG_PARAMETER_VALUE + "]",
            exclude_all, false);
}


void exa_vltune::init_see_alsos()
{}


void exa_vltune::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    switch (_vltune_mode)
    {
    case VLTUNE_SET_PARAM:
    case VLTUNE_GET_PARAM:
    case VLTUNE_ADD_PARAM:
    case VLTUNE_REMOVE_PARAM:
    case VLTUNE_RESET_PARAM:
        cmd_tune_param();
        break;

    case VLTUNE_LIST:
        cmd_display_param_list();
        break;

    default:
        EXA_ASSERT_VERBOSE(false, "Unexpected tuning mode %i", _vltune_mode);
    }
}


void exa_vltune::cmd_tune_param()
{
    string error_msg;
    string command_msg;
    exa_error_code error_code = EXA_SUCCESS;
    AdmindCommand command("vltune", exa.get_cluster_uuid());

    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);

    command.add_param("param_name", _param_name);
    command.add_param("param_value", _param_value);

#ifdef WITH_FS
    command.add_param("no_fs_check", nofscheck);
#endif

    switch (_vltune_mode)
    {
    case VLTUNE_SET_PARAM:
        command_msg = "Set '" + _param_name + "' to '" + _param_value + "'";
        command.add_param("operation", "set");
        break;

    case VLTUNE_ADD_PARAM:
        command_msg = "Add '" + _param_value + "' to '" + _param_name + "'";
        command.add_param("operation", "add");
        break;

    case VLTUNE_REMOVE_PARAM:
        command_msg = "Remove '" + _param_value + "' from '" + _param_name +
                      "'";
        command.add_param("operation", "remove");
        break;

    case VLTUNE_RESET_PARAM:
        command_msg = "Reset '" + _param_name + "' to default value";
        command.add_param("operation", "reset");
        break;

    case VLTUNE_GET_PARAM:
        command_msg.clear();
        command.add_param("operation", "get");
        break;

    default:
        EXA_ASSERT_VERBOSE(false, "Unexpected tuning mode %i", _vltune_mode);
    }

    /* Send the command and receive the response */
    shared_ptr<AdmindMessage> msg_tune(
        send_command(command, command_msg, error_code, error_msg));
    if (msg_tune == NULL)
        throw CommandException(EXA_ERR_DEFAULT);

    switch (error_code)
    {
    case EXA_SUCCESS:
        if (_vltune_mode == VLTUNE_GET_PARAM)
            display_value_from_xml(msg_tune->get_payload());
        break;

    case VRT_ERR_GROUP_NOT_STARTED:
        exa_cli_error("\n%sERROR%s: The disk group is not started. "
                      "Please use exa_dgstart first.\n",
                      COLOR_ERROR, COLOR_NORM);

    default:
        throw CommandException(error_code);
    }
}


void exa_vltune::display_value_from_xml(const string &raw_xml)
{
    xmlDocPtr xml_doc = xml_conf_init_from_buf(raw_xml.c_str(), raw_xml.size());
    xmlNodePtr param_xml = xml_conf_xpath_singleton(xml_doc, "//" EXA_PARAM);

    const char *value = xml_get_prop(param_xml, EXA_PARAM_VALUE);

    if (value != NULL)
        exa_cli_info("%s\n", value);
    else
    {
        int index_value_item;
        xmlNodePtr value_item_ptr;
        xmlNodeSetPtr xml_nodeset = xml_conf_xpath_query(
            xml_doc,
            "//" EXA_PARAM "/"
            EXA_PARAM_VALUE_ITEM);
        xml_conf_xpath_result_for_each(xml_nodeset,
                                       value_item_ptr,
                                       index_value_item)
        {
            value = xml_get_prop(value_item_ptr, EXA_PARAM_VALUE);
            exa_cli_info("%s\n", value);
        }
    }

    xmlFree(param_xml);
    xmlFree(xml_doc);
}


void exa_vltune::cmd_display_param_list()
{
    string error_msg;
    exa_error_code error_code = EXA_SUCCESS;

    AdmindCommand command("vlgettune", exa.get_cluster_uuid());

    command.add_param("group_name", _group_name);
    command.add_param("volume_name", _volume_name);

    /* Send the command and receive the response */
    shared_ptr<AdmindMessage> msg_tune(
        send_command(command, std::string(""), error_code, error_msg));
    if (msg_tune == NULL || error_code != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    int index_param;
    xmlNodePtr param_ptr;
    string vltunelist = msg_tune->get_payload();
    xmlDocPtr vltunelist_doc = xml_conf_init_from_buf(vltunelist.c_str(),
                                                      vltunelist.size());
    xmlNodeSetPtr xmlnode_set = xml_conf_xpath_query(vltunelist_doc,
                                                     "//" EXA_PARAM);
    xml_conf_xpath_result_for_each(xmlnode_set, param_ptr, index_param)
    {
        const char *key = xml_get_prop(param_ptr, EXA_PARAM_NAME);
        const char *value = xml_get_prop(param_ptr, EXA_PARAM_VALUE);
        const char *default_value = xml_get_prop(param_ptr, EXA_PARAM_DEFAULT);
        const char *description = xml_get_prop(param_ptr, EXA_PARAM_DESCRIPTION);
        bool at_default = value == NULL || strcmp(value, default_value) == 0;

        EXA_ASSERT(key != NULL);
        EXA_ASSERT(description != NULL);

        std::stringstream ss;

        if (verbose)
        {
            if (!at_default)
                ss << COLOR_WARNING;

            ss << key << ": ";
            if (value != NULL)
                ss << "Set to '" << value << "' ";
            else
                ss << "Set to default ";

            if (!at_default)
                ss << COLOR_NORM;

            if (default_value != NULL)
                ss << "(default is '" << default_value << "')";

            ss << std::endl;

            indented_dump(ss.str(), std::cout, 2);
            indented_dump(description, std::cout, 4);
            std::cout << std::endl;
        }
        else
        {
            const char *current_value = value ? value : default_value;

            std::cout << (!at_default ? COLOR_WARNING : "")
                      << key << ": '" << current_value << "'"
                      << (!at_default ? COLOR_NORM : "")
                      << (at_default ? " (default)" : "")
                      << std::endl;
        }
    }
    xmlFree(xmlnode_set);
    xmlFree(vltunelist_doc);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_vltune::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    string _parameter;

    exa_vlcommand::parse_non_opt_args(non_opt_args);

    if (_vltune_mode != VLTUNE_NONE)
        return;

    _parameter = non_opt_args.at(1);

    if (set_parameter(_parameter, _param_name, _param_value) != EXA_SUCCESS)
        throw CommandException("The parameter '"
                               + _parameter + "' is not suffixed with '='");

    if (_param_value.empty())
        _vltune_mode = VLTUNE_RESET_PARAM;
    else
        _vltune_mode = VLTUNE_SET_PARAM;
}


void exa_vltune::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_vlcommand::parse_opt_args(opt_args);

    if (opt_args.find('l') != opt_args.end())
        _vltune_mode = VLTUNE_LIST;

    if (opt_args.find('p') != opt_args.end())
        _param_name = opt_args.find('p')->second;

    if (opt_args.find('s') != opt_args.end())
    {
        _vltune_mode = VLTUNE_SET_PARAM;
        _param_value = opt_args.find('s')->second;
    }
    else if (opt_args.find('g') != opt_args.end())
    {
        _vltune_mode = VLTUNE_GET_PARAM;
        _param_value = opt_args.find('g')->second;
    }
    else if (opt_args.find('a') != opt_args.end())
    {
        _vltune_mode = VLTUNE_ADD_PARAM;
        _param_value = opt_args.find('a')->second;
    }
    else if (opt_args.find('r') != opt_args.end())
    {
        _vltune_mode = VLTUNE_REMOVE_PARAM;
        _param_value = opt_args.find('r')->second;
    }
    else if (opt_args.find('z') != opt_args.end())
    {
        _vltune_mode = VLTUNE_RESET_PARAM;
        _param_value = opt_args.find('z')->second;
    }

#ifdef WITH_FS
    if (opt_args.find('F') != opt_args.end())
        nofscheck = true;
#endif

    if (opt_args.find('V') != opt_args.end())
    {
        if (_vltune_mode != VLTUNE_LIST)
            throw CommandException("Option -V can only be specified with -l");
        verbose = true;
    }
}


void exa_vltune::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Tune parameters of an Exanodes volume.";
}


void exa_vltune::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Tune parameters of the volume " << ARG_VOLUME_VOLUMENAME
        << " of the disk group " << ARG_VOLUME_GROUPNAME << " of the cluster "
        << ARG_VOLUME_CLUSTERNAME << "." << std::endl;
}


void exa_vltune::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Set " << Boldify("param") << " to " << Boldify("value") <<
    " for the volume "
        << Boldify("myvolume") << " of the group " << Boldify("mygroup")
        << " of the cluster " << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume -p param -s value" <<
    std::endl;
    out << "  " << "or " << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume param=value" <<
    std::endl;
    out << std::endl;

    out << "Add value " << Boldify("param") << " to " << Boldify("param") <<
    " for the volume "
        << Boldify("myvolume") << " of the group " << Boldify("mygroup")
        << " of the cluster " << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume -p param -a value" <<
    std::endl;
    out << std::endl;

    out << "Get value of " << Boldify("param") << " for the volume "
        << Boldify("myvolume") << " of the group " << Boldify("mygroup")
        << " of the cluster " << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume -p param -g" <<
    std::endl;
    out << std::endl;

    out << "Remove value " << Boldify("param") << " from " <<
    Boldify("param") << " for the volume "
                     << Boldify("myvolume") << " of the group " << Boldify(
        "mygroup")
                     << " of the cluster " << Boldify("mycluster") << ":" <<
    std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume -p param -d value" <<
    std::endl;
    out << std::endl;

    out << "Reset " << Boldify("param") << " for the volume " << Boldify(
        "myvolume")
        << " of the group " << Boldify("mygroup") << " of the cluster "
        << Boldify("mycluster") << " to its default value:" << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume -p param -r" <<
    std::endl;
    out << "  " << "or " << std::endl;
    out << "  " << "exa_vltune mycluster:mygroup:myvolume param=" << std::endl;
    out << std::endl;

    out <<
    "Display the list of parameters and their current value for the volume "
        << Boldify("myvolume") << " of the group " << Boldify("mygroup")
        << " of the cluster " << Boldify("mycluster") << ":" << std::endl;
    out << "  " << "exa_vltune -l mycluster:mygroup:myvolume" << std::endl;
    out << std::endl;
}


