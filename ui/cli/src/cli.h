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

    typedef std::shared_ptr<Command>(*factory_t)(int argc, char *argv[]);

    template<class T>
    static std::shared_ptr<Command> command_factory(int argc, char *argv[]) {
        std::shared_ptr<Command> inst(new T(argc, argv));
        inst->init_options();
        inst->init_see_alsos();
        inst->parse ();
        return inst;
    }

    template <class Command>
    void register_cmd()
    {
        _exa_commands.insert(std::make_pair(Command::name(), command_factory<Command>));
    }

    template <class Command>
    void register_cmd(const std::string& cmd_name)
    {
        _exa_commands.insert(std::make_pair(cmd_name, command_factory<Command>));
    }

    factory_t find_cmd_factory(const std::string& name);

    void usage();


protected:

private:

    static Cli& instance();
    std::map<std::string, factory_t > _exa_commands;

};





#endif  // __CLI_H__
