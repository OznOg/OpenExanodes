/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMAND_OPTION_H__
#define __COMMAND_OPTION_H__


#include "ui/cli/src/command_param.h"
#include "ui/common/include/exa_conversion.hpp"

#include <map>



class CommandOption : public CommandParam
{
private:

    const char _short_opt;
    const std::string _long_opt;
    const std::string _description;
    const bool _arg_expected;
    const std::string _arg_name;  //!< for help generation
    const std::string _default_value;

public:

    CommandOption(const char short_opt,
                  const std::string &long_opt,
                  const std::string &description,
                  const std::set<int>& mandatory_groups,
                  bool hidden,
                  bool arg_expected,
                  const std::string &arg_name,
                  const std::string &default_value);


    char get_short_opt() const { return _short_opt; }
    const std::string & get_long_opt() const { return _long_opt; }
    const std::string & get_description() const { return _description; }
    bool is_arg_expected() const { return _arg_expected; }
    const std::string & get_arg_name() const { return _arg_name; }
    const std::string & get_default_value() const { return _default_value; }

    std::ostream& write(std::ostream& out) const;

};


struct CommandOptionCmp
{
    bool operator()(const char &op1, const char &op2) const
	{
	    if (exa::to_lower(op1) == exa::to_lower(op2))
		return op2 < op1;
	    return exa::to_lower(op1) < exa::to_lower(op2);
	}
};



typedef std::map<char,std::shared_ptr<CommandOption>,CommandOptionCmp> CommandOptions;


#endif
