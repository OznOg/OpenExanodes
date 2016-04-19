/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMAND_PARAM_H__
#define __COMMAND_PARAM_H__


#include <memory>
#include <string>
#include <set>
#include <vector>



/**
 * Abstract class for a command line parameter.
 *
 * A command line parameter belongs to one or several parameter group, each
 * parameter group being identified by its number.
 *
 * From the command point of view, a param group represents a set of exclusive
 * parameters.
 * In other words, for a command to be valid, the typed command line must
 * provide a value for one and only parameter for each group.
 */
class CommandParam
{
private:

protected:

    const bool _hidden;
    const std::set<int> _param_groups;

    CommandParam(bool hidden, const std::set<int> &mandatory_groups);

public:

    virtual ~CommandParam();
    bool is_hidden() const { return _hidden; }
    bool is_mandatory() const;
    const std::set<int> &get_param_groups() const { return _param_groups; }

    virtual std::ostream &write(std::ostream &out) const = 0;
};


typedef std::vector<std::shared_ptr<CommandParam> > CommandParams;

std::ostream &operator << (std::ostream & os, const CommandParam &op);


#endif
