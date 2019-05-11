/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_VLDELETE_H__
#define  __EXA_VLDELETE_H__


#include "ui/cli/src/exa_vlcommand.h"
#include "ui/cli/src/cli.h"


class exa_vldelete : public exa_vlcommand
{

 public:

  exa_vldelete (int argc, char * argv []);
  ~exa_vldelete ();

  static constexpr const char *name() { return "exa_vldelete"; }

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
  bool metadata_recovery;

};


#endif  // __EXA_VLDELETE_H__
