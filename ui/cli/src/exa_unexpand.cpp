/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/exa_unexpand.h"

#include "ui/cli/src/cli.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

using std::string;

const std::string exa_unexpand::ARG_STRING(Command::Boldify("STRING"));

exa_unexpand::exa_unexpand()
    : _request("")
{}

void exa_unexpand::init_options()
{
    Command::init_options();
    add_arg(ARG_STRING, 10, false, "", true);
}


void exa_unexpand::init_see_alsos()
{
    add_see_also("exa_expand");
}


void exa_unexpand::run()
{
    try
    {
        printf("%s\n", strjoin(" ", ::exa_unexpand(_request)).c_str());
    }
    catch (string msg)
    {
        throw CommandException("Failed to unexpand : " + msg);
    }
}


void exa_unexpand::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    Command::parse_opt_args(opt_args);
}


void exa_unexpand::parse_non_opt_args(
    const std::vector<std::string> &non_opt_args)
{
    Command::parse_non_opt_args(non_opt_args);
    _request = non_opt_args.at(0);
}


void exa_unexpand::dump_short_description(std::ostream &out,
                                          bool show_hidden) const
{
    out << "Unexpand a string using Exanodes regular expansion.";
}


void exa_unexpand::dump_full_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "The " << ARG_STRING <<
    " will be unexpanded using regular expansion of the form:"
        << std::endl;
    out << "PREFIX/" << Boldify("NUMBERING") << "/SUFFIX." << std::endl;
    out << std::endl;
    out << "" << Boldify("NUMBERING") <<
    " is a list of numbers or ranges separated by a colon."
        << std::endl;
    out << "A range is two numbers separated by a dash." << std::endl;
    out << "Numbers may contain leading zeroes." << std::endl;

    out << "Note: the " << ARG_STRING
        << " itself can contain one or more space separated elements but they"
        << " cannot include regular expansions." << std::endl;
}


void exa_unexpand::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "  " << "exa_unexpand node1 node2 node3 node4        => node/1-4/"
        << std::endl;
    out << "  " <<
    "exa_unexpand node1 node2 node3 node4 node10 => node/1-4:10/"
        << std::endl;
    out << "  " <<
    "exa_unexpand node3 node8 node9 node1        => node/1:3:8-9/"
        << std::endl;
    out << "  " <<
    "exa_unexpand node-3 node-4 node-5 node-12   => node-/3-5:12/"
        << std::endl;
    out << "  " <<
    "exa_unexpand node-1-3 node-1-4 node-2-3 node-2-4   => node-/1-2/-/3-4/"
        << std::endl;
}


