/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLNODERECOVER_H__
#define __EXA_CLNODERECOVER_H__


#include "ui/cli/src/exa_clcommand.h"


class exa_clnoderecover: public exa_clcommand
{
public:

    static const std::string OPT_ARG_NODE_HOSTNAMES;

    static constexpr const char *name() { return "exa_clnoderecover"; }

    exa_clnoderecover();

    void run();

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

  int exa_send_create();
  exa_error_code node_join();
  exa_error_code node_init();
  exa_error_code node_leave();
  std::set<std::string> nodes;

  enum {
    NONE,
    JOIN,
    LEAVE,
  } action;
};



#endif /* __EXA_CLNODERECOVER_H__ */
