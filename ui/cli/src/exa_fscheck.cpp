/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_fscheck.h"

#include "common/include/exa_constants.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"

using std::string;

const std::string exa_fscheck::OPT_ARG_NODE_HOSTNAME(Command::Boldify(
                                                         "HOSTNAME"));
const std::string exa_fscheck::OPT_ARG_PARAMETERS_STRING(Command::Boldify(
                                                             "STRING"));

exa_fscheck::exa_fscheck()
    : _node("")
    , _repair(false)
    , _parameters("")
{
    add_option('n', "node", "Node to check the filesystem on. If not specified "
               "a node will be chosen automatically.", 0, false, true,
               OPT_ARG_NODE_HOSTNAME);
    add_option('r', "repair", "Ask not only to check, but also to repair if "
               "corrupted. If this option is not set, the command will not "
               "change anything on the volume.", 0, false, false);
    add_option('p', "parameters", "Optional parameters string to provide to "
               "fsck, should be protected by quotes \"...\".", 0, true, true,
               OPT_ARG_PARAMETERS_STRING);
}


void exa_fscheck::init_see_alsos()
{
    add_see_also("exa_fscreate");
    add_see_also("exa_fsdelete");
    add_see_also("exa_fsresize");
    add_see_also("exa_fsstart");
    add_see_also("exa_fsstop");
    add_see_also("exa_fstune");
}


void exa_fscheck::run()
{
    string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    /*
     * Check command
     */
    AdmindCommand command("fscheck", exa.get_cluster_uuid());
    command.add_param("volume_name", _fs_name);
    command.add_param("group_name", _group_name);
    command.add_param("host_name", _node);
    command.add_param("options", _parameters);
    command.add_param("repair", _repair);

    exa_cli_info(
        "Checking file system '%s' in group '%s' for cluster '%s' with the repair option set to '%s'\n",
        _fs_name.c_str(),
        _group_name.c_str(),
        exa.get_cluster().c_str(),
        _repair ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);

    /* Send the command and receive the response */
    exa_error_code error_code;
    string error_message;
    send_command(command, "Checking file system:", error_code, error_message);

    if (error_code != EXA_SUCCESS)
        throw CommandException(error_code);
}


void exa_fscheck::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_fscommand::parse_opt_args(opt_args);

    if (opt_args.find('p') != opt_args.end())
        _parameters = opt_args.find('p')->second;

    if (opt_args.find('n') != opt_args.end())
        _node = opt_args.find('n')->second;

    if (opt_args.find('r') != opt_args.end())
        _repair = true;
}


void exa_fscheck::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Check a file system managed by Exanodes.";
}


void exa_fscheck::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Check a file system managed by Exanodes." << std::endl;
}


void exa_fscheck::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Check the file system " << Boldify("myfs") << " of group "
        << Boldify("mygroup") << " of cluster " << Boldify("mycluster") <<
    ":" << std::endl;
    out << "  " << "exa_fscheck mycluster:mygroup:myfs" << std::endl;
    out << std::endl;
}


