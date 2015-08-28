/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/command_option.h"
#include <iostream>

CommandOption::CommandOption(const char short_opt,
                             const std::string &long_opt,
                             const std::string &description,
                             const std::set<int> &mandatory_groups,
                             bool hidden,
                             bool arg_expected,
                             const std::string &arg_name,
                             const std::string &default_value)
    : CommandParam(hidden, mandatory_groups)
    , _short_opt(short_opt)
    , _long_opt(long_opt)
    , _description(description)
    , _arg_expected(arg_expected)
    , _arg_name(arg_name)
    , _default_value(default_value)
{ }

std::ostream &CommandOption::write(std::ostream &os) const
{
    os << "--" << get_long_opt();
    if (is_arg_expected())
        os << " " << get_arg_name();
    return os;
}


