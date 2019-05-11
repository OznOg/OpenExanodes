/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_DGDISKADD_H__
#define  __EXA_DGDISKADD_H__


#include "ui/cli/src/exa_dgcommand.h"
#include "ui/cli/src/cli.h"


class exa_dgdiskadd : public exa_dgcommand
{

 public:

    static const std::string OPT_ARG_DISK_PATH;
    static const std::string OPT_ARG_DISK_HOSTNAME;

    using exa_dgcommand::exa_dgcommand;

    static constexpr const char *name() { return "exa_dgdiskadd"; }

    void init_options();
    void init_see_alsos();

    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:
    std::string _node_name, _disk_path;
};


#endif  // __EXA_DGDISKADD_H__
