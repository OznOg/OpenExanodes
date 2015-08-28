/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLCOMMAND_H__
#define __EXA_CLCOMMAND_H__


#include "ui/cli/src/command.h"



class exa_clcommand : public Command
{
protected:

    static const std::string ARG_CLUSTERNAME;

    exa_clcommand(int argc, char *argv[]);

public:

    virtual ~exa_clcommand();

    static void check_name(const std::string &clname);

protected:

    virtual void init_options();

    virtual void parse_opt_args(const std::map<char, std::string> &opt_args) = 0;
    virtual void parse_non_opt_args(const std::vector<std::string> &non_opt_args);

protected:

    std::string _cluster_name;

};






#endif /* __EXA_CLCOMMAND_H__ */
