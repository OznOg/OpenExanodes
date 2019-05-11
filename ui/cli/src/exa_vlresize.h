/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_VLRESIZE_H__
#define  __EXA_VLRESIZE_H__

#include "ui/cli/src/exa_vlcommand.h"


class exa_vlresize : public exa_vlcommand
{

 public:

    static const std::string OPT_ARG_SIZE_SIZE;

  exa_vlresize (int argc, char * argv []);
  ~exa_vlresize ();

  static constexpr const char *name() { return "exa_vlresize"; }

  void init_options();
  void init_see_alsos();

  void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  bool nofscheck;
  uint64_t sizeKB_uu64;
  bool size_max;
};


#endif  // __EXA_VLRESIZE_H__
