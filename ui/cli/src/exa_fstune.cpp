/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fstune.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_config.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"

using std::string;
using std::shared_ptr;

const std::string exa_fstune::ARG_PARAMETER_PARAMETER(Command::Boldify(
                                                          "PARAMETER"));
const std::string exa_fstune::ARG_PARAMETER_VALUE(Command::Boldify("VALUE"));

exa_fstune::exa_fstune()
    : _display_params(false)
{
    add_option('l', "list", "Display all parameters and their current values.",
               1, false, false);

    add_arg(ARG_PARAMETER_PARAMETER + "=" +
            "[" + ARG_PARAMETER_VALUE + "]", 1, false);

    add_see_also("exa_fscreate");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fsresize");
    add_see_also("exa_fsstart");
    add_see_also("exa_fsstop");
    add_see_also("exa_fscheck");
    add_see_also("exa_fstune");
}


void exa_fstune::run()
{
    string error_msg;
    exa_error_code error_code;
    string error_message;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    if (_display_params)
    {
        AdmindCommand command("fsgettune", exa.get_cluster_uuid());
        command.add_param("volume_name", _fs_name);
        command.add_param("group_name", _group_name);

        exa_cli_info(
            "Tuning fs '%s' in group '%s' for cluster '%s' : get values\n",
            _fs_name.c_str(),
            _group_name.c_str(),
            exa.get_cluster().c_str());

        shared_ptr<AdmindMessage>
        msg_tune(send_command(command,
                              "",   /* Keeping this empty to avoid message */
                              error_code, error_message));
        if (msg_tune == NULL)
            throw CommandException(EXA_ERR_DEFAULT);

        if (error_code == EXA_SUCCESS)
        {
            int index_param;
            xmlNodePtr param_ptr;
            string fstunelist = msg_tune->get_payload();
            xmlDocPtr fstunelist_doc = xml_conf_init_from_buf(fstunelist.c_str(),
                                                              fstunelist.size());
            xmlNodeSetPtr xmlnode_set = xml_conf_xpath_query(fstunelist_doc,
                                                             "//" EXA_PARAM);
            printf("\n");
            xml_conf_xpath_result_for_each(xmlnode_set, param_ptr, index_param)
            {
                const char *description = xml_get_prop(param_ptr,
                                                       EXA_PARAM_DESCRIPTION);
                const char *value = xml_get_prop(param_ptr, EXA_PARAM_VALUE);
                const char *key = xml_get_prop(param_ptr, EXA_PARAM_NAME);

                EXA_ASSERT(description != NULL);
                EXA_ASSERT(value != NULL);
                EXA_ASSERT(key != NULL);

                printf("  %s: Set to '%s'\n"
                       "    %s\n\n",
                       key, value, description);
            }
            xmlFree(xmlnode_set);
            xmlFree(fstunelist_doc);
        }
    }
    else
    {
        AdmindCommand command("fstune", exa.get_cluster_uuid());
        command.add_param("volume_name", _fs_name);
        command.add_param("group_name", _group_name);
        command.add_param("option",  _option_str);
        command.add_param("value", _value_str);

        exa_cli_info(
            "Tuning fs '%s' in group '%s' for cluster '%s' : set '%s' to '%s'\n",
            _fs_name.c_str(),
            _group_name.c_str(),
            exa.get_cluster().c_str(),
            _option_str.c_str(),
            _value_str.c_str());

        send_command(command, "File system tune:", error_code, error_message);
    }

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fstune::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    exa_fscommand::parse_non_opt_args(non_opt_args);

    if (_display_params)
        return;

    if (!column_split("=", non_opt_args.at(1), _option_str, _value_str))
        throw CommandException(
            "Malformed argument, " + ARG_PARAMETER_PARAMETER + "=" +
            ARG_PARAMETER_VALUE + " is expected.");
}


void exa_fstune::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('l') != opt_args.end())
        _display_params = true;
}


void exa_fstune::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Tune parameters of a file system managed by Exanodes.";
}


void exa_fstune::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Set the parameter " << ARG_PARAMETER_PARAMETER << " to the value "
        << ARG_PARAMETER_VALUE << " for the file system " <<
    ARG_FILESYSTEM_FSNAME
        << " of the group " << ARG_FILESYSTEM_GROUPNAME << " of the cluster "
        << ARG_FILESYSTEM_CLUSTERNAME << ". If the " << ARG_PARAMETER_VALUE
        << " of a parameter is not provided, then its default value is used."
        << std::endl;
}


void exa_fstune::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Set the number of SFS logs to " << Boldify("12")
        << " for the file system " << Boldify("myfs") << " of the group "
        << Boldify("mygroup") << " of the cluster "
        << Boldify("mycluster") << ":" << std::endl;
    out << "  "  << "exa_fstune mycluster:mygroup:myfs sfs_logs=12" <<
    std::endl;
    out << std::endl;
}


