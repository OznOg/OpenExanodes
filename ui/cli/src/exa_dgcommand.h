/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_DGCOMMAND_H__
#define __EXA_DGCOMMAND_H__


#include "ui/cli/src/command.h"



class exa_dgcommand : public Command
{
protected:

    static const std::string ARG_DISKGROUP_CLUSTERNAME;
    static const std::string ARG_DISKGROUP_GROUPNAME;

    exa_dgcommand(int argc, char *argv[]);

public:

    virtual ~exa_dgcommand();

    static void check_name(const std::string &dgname);

protected:

    virtual void init_options();

    virtual void parse_opt_args(const std::map<char, std::string> &opt_args) = 0;
    virtual void parse_non_opt_args(const std::vector<std::string> &non_opt_args);

protected:

    std::string _cluster_name;
    std::string _group_name;

};






#endif /* __EXA_DGCOMMAND_H__ */
