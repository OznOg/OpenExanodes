/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_FSRESIZE_H__
#define  __EXA_FSRESIZE_H__

#include "ui/cli/src/exa_fscommand.h"
#include "ui/cli/src/cli.h"


class exa_fsresize : public exa_fscommand
{

 public:

    static const std::string OPT_ARG_SIZE_SIZE;


  exa_fsresize (int argc, char *argv[]);
  ~exa_fsresize ();

  void init_options();
  void init_see_alsos();

  void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:
  uint64_t sizeKB_uu64;
  bool size_max; /** If true, the file system will fill the group */
};


#endif  // __EXA_FSRESIZE_H__
