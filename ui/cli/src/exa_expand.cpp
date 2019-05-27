/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

#include "ui/cli/src/exa_expand.h"

using std::string;

const std::string exa_expand::ARG_STRING(Command::Boldify("STRING"));

exa_expand::exa_expand()
{
    add_arg(ARG_STRING, 10, false, "", true);
    add_see_also("exa_unexpand");
}


void exa_expand::run()
{
    try
    {
        std::set<std::string> nodelist(::exa_expand(_request));
        if (nodelist.empty())
            throw CommandException("Failed to expand");
        printf("%s\n", strjoin(" ", nodelist).c_str());
    }
    catch (string msg)
    {
        throw CommandException("Failed to expand : " + msg);
    }
}


void exa_expand::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    Command::parse_opt_args(opt_args);
}


void exa_expand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);
    _request = non_opt_args.at(0);
}


void exa_expand::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Expand a string using Exanodes regular expansion.";
}


void exa_expand::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "The " << ARG_STRING <<
    " will be expanded using regular expansion of the form:"
        << std::endl;
    out << "PREFIX/" << Boldify("NUMBERING") << "/SUFFIX." << std::endl;
    out << std::endl;
    out << "" << Boldify("NUMBERING") <<
    " is a list of numbers or ranges separated by a colon."
        << std::endl;
    out << "A range is two numbers separated by a dash." << std::endl;
    out << "Numbers may contain leading zeroes." << std::endl;
    out << "Regular expansions are mainly used to specify a range "
        << "of hostnames in some Exanodes commands. You can use exa_expand "
        << "to provide regular expansion in your own shell scripts." <<
    std::endl;
}


void exa_expand::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "  " << "exa_expand node/1-4/          => node1 node2 node3 node4"
        << std::endl;
    out << "  " <<
    "exa_expand node/1-4/ node10   => node1 node2 node3 node4 node10"
        << std::endl;
    out << "  " << "exa_expand node/3:8-9/ node1  => node1 node3 node8 node9"
        << std::endl;
    out << "  " <<
    "exa_expand node-/3-5:7:10-12/ => node-3 node-4 node-5 node-7 node-10 node-11 node-12"
        << std::endl;
    out << "  " <<
    "exa_expand node-/1-2/-/3-4/   => node-1-3 node-1-4 node-2-3 node-2-4"
        << std::endl;
    out << "  " << "exa_expand node/01-02:08/     => node01 node02 node08"
        << std::endl;
}


