/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_DGSTART_H__
#define  __EXA_DGSTART_H__

#include "ui/cli/src/exa_dgcommand.h"

class exa_dgstart final : public exa_dgcommand
{

 public:

  using exa_dgcommand::init_options;

  static constexpr const char *name() { return "exa_dgstart"; } 
  void init_see_alsos() override;

  void run() override;

protected:

    std::string get_short_description(bool show_hidden) const override;
    std::string get_full_description(bool show_hidden) const override;
    void dump_examples(std::ostream& out, bool show_hidden = false) const override;
};


#endif  // __EXA_DGSTART_H__
