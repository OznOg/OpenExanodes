/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLSTATS_H__
#define __EXA_CLSTATS_H__

#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/cli.h"


class exa_clstats : public exa_clcommand
{
public:

    static const std::string OPT_ARG_WRAPPING_N;

    exa_clstats(int argc, char *argv[]);

  static constexpr const char *name() { return "exa_clstats"; }

  void init_options();
  void init_see_alsos();

    void run();

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);


private:

    bool reset;
    int display_style;
    uint wrapping_in_nodes;
};


#endif /* __EXA_CLSTATS_H__ */
