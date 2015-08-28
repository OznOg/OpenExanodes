/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/command_arg.h"
#include <iostream>

CommandArg::CommandArg(size_t position,
                       const std::string &arg_name,
                       const std::set<int> &param_groups,
                       bool hidden,
                       const std::string &default_value,
                       bool multiple)
    : CommandParam(hidden, param_groups)
    , _position(position)
    , _arg_name(arg_name)
    , _default_value(default_value)
    , _multiple(multiple)
{}


std::ostream &CommandArg::write(std::ostream &os) const
{
    os << get_arg_name();
    return os;
}


