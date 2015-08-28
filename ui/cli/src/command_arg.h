/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMAND_ARG_H__
#define __COMMAND_ARG_H__


#include "ui/cli/src/command_param.h"

#include <map>



class CommandArg : public CommandParam
{
private:

    const size_t _position;
    const std::string _arg_name;
    const std::string _default_value;
    const bool _multiple;

public:

    CommandArg(size_t position,
	       const std::string &arg_name,
	       const std::set<int>& param_groups,
	       bool hidden,
	       const std::string &default_value,
	       bool multiple);

    size_t get_position() const { return _position; }
    const std::string & get_arg_name() const { return _arg_name; }
    const std::string & get_default_value() const { return _default_value; }
    bool is_multiple() const { return _multiple; }

    std::ostream& write(std::ostream& out) const;
};



typedef std::map<int,boost::shared_ptr<CommandArg> > CommandArgs;


#endif
