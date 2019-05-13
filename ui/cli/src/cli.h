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
#include "ui/common/include/cli_log.h"

#include <memory>

template <class... Commands>
class Cli
{
    template<class T>
    static std::shared_ptr<Command> factory() {
        return std::make_shared<T>();
    }

public:

    std::shared_ptr<Command> find_cmd(const std::string& name)
    {
        static const std::map<std::string, std::shared_ptr<Command>(*)()> _exa_commands = {
            { Commands::name(), factory<Commands> } ...
        };

        auto it = _exa_commands.find(name);
        if (it == _exa_commands.end())
            return nullptr;
        auto cmd = it->second();
        if (cmd != nullptr) {
            cmd->init_see_alsos();
        }
        return cmd;
    }

    void usage();

private:

};


#include <iostream>
#include <iomanip>

/* Index of each group of commands */
#define INDEX_CL    0
#define INDEX_NODE  1
#define INDEX_DG    2
#define INDEX_VL    3
#define INDEX_MISC  4

#define INDEX__FIRST  INDEX_CL
#define INDEX__LAST   INDEX_MISC

/* Number of command groups */
#define NUM_GROUPS  (INDEX__LAST + 1)

static inline const std::string group_names[NUM_GROUPS] = {
    "CLUSTER", "NODE", "GROUP", "VOLUME", "MISC"
};

static inline bool begins_with(const std::string &str, const std::string &prefix)
{
    return str.substr(0, prefix.length()) == prefix;
}


/**
 * Get the index of the group the specified command belongs to.
 *
 * @return Group index
 */
static inline int index_of_command(const std::string &cmd)
{
    if (begins_with(cmd, "exa_clnode"))
        return INDEX_NODE;
    else if (begins_with(cmd, "exa_cl"))
        return INDEX_CL;
    else if (begins_with(cmd, "exa_dg"))
        return INDEX_DG;
    else if (begins_with(cmd, "exa_vl"))
        return INDEX_VL;
    else
        return INDEX_MISC;
}


template <class... Commands>
void Cli<Commands...>::usage()
{
    std::list<std::string> cmd_groups[NUM_GROUPS];
    size_t maxlen[NUM_GROUPS];
    int index;

    exa_cli_info("Known commands (exa_cli -h):\n");

    for (index = INDEX__FIRST; index <= INDEX__LAST; index++)
        maxlen[index] = 0;

    for (auto cmd_name : { std::string(Commands::name())... })
    {
        index = index_of_command(cmd_name);
        cmd_groups[index].push_back(cmd_name);
        if (cmd_name.length() > maxlen[index])
            maxlen[index] = cmd_name.length();
    }

    /* Align column headers and data to the left */
    std::cout << std::setiosflags(std::ios::left);

    for (index = INDEX__FIRST; index <= INDEX__LAST; index++)
    {
        if (index > INDEX__FIRST)
            std::cout << " ";
        std::cout << std::setw(maxlen[index]) << group_names[index];
    }
    std::cout << std::endl;

    bool done = false;
    while (!done)
    {
        int empty_groups = 0;

        for (index = INDEX__FIRST; index <= INDEX__LAST; index++)
        {
            std::string cmd_name;

            if (cmd_groups[index].size() == 0)
            {
                cmd_name = "";
                empty_groups++;
            }
            else
            {
                cmd_name = cmd_groups[index].front();
                cmd_groups[index].pop_front();
            }

            if (index > INDEX__FIRST)
                std::cout << " ";
            std::cout << std::setw(maxlen[index]) << cmd_name;
        }

        std::cout << std::endl;

        done = (empty_groups == NUM_GROUPS);
    }
}

#endif  // __CLI_H__
