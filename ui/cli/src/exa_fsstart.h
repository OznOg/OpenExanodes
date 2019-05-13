/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_FSSTART_H__
#define  __EXA_FSSTART_H__


#include "ui/cli/src/exa_fscommand.h"


class exa_fsstart : public exa_fscommand
{

 public:

    static const std::string OPT_ARG_NODE_HOSTNAMES;
    static const std::string OPT_ARG_MOUNTPOINT_PATH;

  exa_fsstart();

  static constexpr const char *name() { return "exa_fsstart"; }

  void init_see_alsos();

  void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  std::string nodes;
  bool allnodes;
  std::string mount_point;
  bool read_only;

};


#endif  // __EXA_FSSTART_H__
