/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_FSTUNE_H__
#define  __EXA_FSTUNE_H__

#include "ui/cli/src/exa_fscommand.h"
#include "ui/cli/src/cli.h"



class exa_fstune : public exa_fscommand
{

public:

    static const std::string ARG_PARAMETER_PARAMETER;
    static const std::string ARG_PARAMETER_VALUE;

    exa_fstune();

    static constexpr const char *name() { return "exa_fstune"; }

    void init_options();
    void init_see_alsos();

    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);
    void parse_non_opt_args (const std::vector<std::string>& non_opt_args);

private:

    bool _display_params;
    std::string _option_str;
    std::string _value_str;

};


#endif  // __EXA_FSTUNE_H__
