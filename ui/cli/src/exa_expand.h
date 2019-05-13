/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_EXPAND_H__
#define  __EXA_EXPAND_H__

#include "ui/cli/src/command.h"

class exa_expand : public Command
{

public:

    static const std::string ARG_STRING;

    static constexpr const char *name() { return "exa_expand"; }

    exa_expand();
    void init_see_alsos();

    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_non_opt_args (const std::vector<std::string>& non_opt_args);
    void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

    std::string _request;

};

#endif  // __EXA_EXPAND_H__
