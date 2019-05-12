/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_CLCREATE_H__
#define  __EXA_CLCREATE_H__

#include "ui/cli/src/exa_clcommand.h"

using std::string;
using std::list;

class exa_clcreate : public exa_clcommand
{

 public:

    static const std::string OPT_ARG_CONFIG_FILE;
    static const std::string OPT_ARG_HOSTNAME;
    static const std::string OPT_ARG_DISK_PATH;
    static const std::string OPT_ARG_LOAD_TUNING_FILE;
    static const std::string OPT_ARG_LICENSE_FILE;
    static const std::string OPT_ARG_DATANETWORK_HOSTNAME;
    static const std::string OPT_ARG_SPOFGROUP;

  static constexpr const char *name() { return "exa_clcreate"; }
  void init_options();
  void init_see_alsos();

  void run();

  exa_error_code load_tune_file(std::string file, xmlDocPtr xml_cfg, bool policy);
  static list<list<string> > parse_spof_groups(const string spof_groups);

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  int exa_send_create ();

  std::string _config_file;
  std::string _datanetwork;
  std::string _disks;
  std::string _tuning_file;
  std::string _spof_groups;
  std::string _license_file;
};



#endif  // __EXA_CLCREATE_H__
