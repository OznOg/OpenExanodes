/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/command_param.h"
#include <iostream>

CommandParam::CommandParam(bool hidden,
                           const std::set<int> &param_groups)
    : _hidden(hidden)
    , _param_groups(param_groups)
{}


CommandParam::~CommandParam()
{ }

bool CommandParam::is_mandatory() const
{
    return _param_groups.empty() == false;
}


std::ostream &operator <<(std::ostream &os, const CommandParam &op)
{
    return op.write(os);
}


