/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLTUNE_H__
#define __EXA_CLTUNE_H__



#include "ui/cli/src/exa_clcommand.h"

class exa_cltune : public exa_clcommand
{
public:

    static const std::string OPT_ARG_SAVE_FILE;
    static const std::string OPT_ARG_LOAD_FILE;
    static const std::string ARG_PARAMETER_PARAMETER;
    static const std::string ARG_PARAMETER_VALUE;

  exa_cltune();

  static constexpr const char *name() { return "exa_cltune"; }

  void init_options();
  void init_see_alsos();

  void run();

protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_non_opt_args(const std::vector<std::string> &non_opt_args);
  void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

  void display_all_params(std::string cluster);
  void dump_param(xmlNodePtr param_ptr, std::ofstream &dumpfd);
  void display_param(xmlNodePtr param_ptr);
  exa_error_code send_single_param(std::string param, std::string value);
  exa_error_code send_param_from_file(std::string load_file);

  bool display_params;
  bool verbose;
  std::string _parameter;
  std::string dump_file;
  std::string load_file;
};



#endif /* __EXA_CLTUNE_H__ */
