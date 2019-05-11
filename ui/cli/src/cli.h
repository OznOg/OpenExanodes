/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __CLI_H__
#define  __CLI_H__



#include "ui/cli/src/command.h"



class Cli
{
public:

    Command::factory_t register_cmd_factory(const std::string& cmd_name, Command::factory_t factory);
    Command::factory_t find_cmd_factory(const std::string& name);

    void usage();


protected:

private:

    static Cli& instance();
    std::map<std::string, Command::factory_t > _exa_commands;

};





#endif  // __CLI_H__
