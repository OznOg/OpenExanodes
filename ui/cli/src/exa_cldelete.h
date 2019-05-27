/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef       __EXA_CLDELETE_H__
#define       __EXA_CLDELETE_H__

#include "ui/cli/src/exa_clcommand.h"



class exa_cldelete : public exa_clcommand
{

 public:

  exa_cldelete();

  static constexpr const char *name() { return "exa_cldelete"; }

  void run();

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  bool _forcemode;
  bool _recursive;

};


#endif  // __EXA_CLDELETE_H__
