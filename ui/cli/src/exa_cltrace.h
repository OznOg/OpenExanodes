/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLTRACE_H__
#define __EXA_CLTRACE_H__


#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/cli.h"

class exa_cltrace : public exa_clcommand
{

public:

    static const std::string OPT_ARG_LEVEL_LEVEL;
    static const std::string OPT_ARG_COMPONENT_COMPONENT;
    static const std::string OPT_ARG_NODE_HOSTNAMES;

  exa_cltrace (int argc, char *argv[]);
  ~exa_cltrace ();

  void init_options();
  void init_see_alsos();

  void run();

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  std::set<std::string> nodelist;
  bool allnodes;
  int component_index;
  int level_index;
};

#endif /* __EXA_CLTRACE_H__ */
