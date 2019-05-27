/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_VLCOMMAND_H__
#define __EXA_VLCOMMAND_H__


#include "ui/cli/src/command.h"



class exa_vlcommand : public Command
{
protected:

    static const std::string ARG_VOLUME_CLUSTERNAME;
    static const std::string ARG_VOLUME_GROUPNAME;
    static const std::string ARG_VOLUME_VOLUMENAME;

public:

    static void check_name(const std::string &vlname);

protected:

    exa_vlcommand();

    virtual void parse_opt_args(const std::map<char, std::string> &opt_args) = 0;
    virtual void parse_non_opt_args(const std::vector<std::string> &non_opt_args);

protected:

    std::string _cluster_name;
    std::string _group_name;
    std::string _volume_name;

};






#endif /* __EXA_VLCOMMAND_H__ */
