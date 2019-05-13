/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_FSCOMMAND_H__
#define __EXA_FSCOMMAND_H__


#include "ui/cli/src/command.h"



class exa_fscommand : public Command
{
protected:

    static const std::string ARG_FILESYSTEM_CLUSTERNAME;
    static const std::string ARG_FILESYSTEM_GROUPNAME;
    static const std::string ARG_FILESYSTEM_FSNAME;

    exa_fscommand();

public:

    static void check_name(const std::string &fsname);

protected:

    virtual void parse_opt_args(const std::map<char, std::string> &opt_args) = 0;
    virtual void parse_non_opt_args(const std::vector<std::string> &non_opt_args);

protected:

    std::string _cluster_name;
    std::string _group_name;
    std::string _fs_name;

};






#endif /* __EXA_FSCOMMAND_H__ */
