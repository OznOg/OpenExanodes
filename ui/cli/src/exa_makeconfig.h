/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef       __EXA_MAKECONFIG_H__
#define       __EXA_MAKECONFIG_H__


#include "ui/cli/src/command.h"

class exa_makeconfig : public Command
{

 public:

    static const std::string OPT_ARG_CLUSTER_CLUSTERNAME;
    static const std::string OPT_ARG_NODE_HOSTNAMES;
    static const std::string OPT_ARG_DISKGROUP_DGNAME;
    static const std::string OPT_ARG_GROUP_N;
    static const std::string OPT_ARG_GROUP_M;
    static const std::string OPT_ARG_DISK_PATHS;

    static const std::string OPT_ARG_DATANETWORK_DATANETWORK;

    static const std::string OPT_ARG_LAYOUT_LAYOUT;
    static const std::string OPT_ARG_LAYOUT_SSTRIPING;
    static const std::string OPT_ARG_LAYOUT_RAINX;

    static const std::string OPT_ARG_EXTRA_DGOPTION;

  exa_makeconfig();

  static constexpr const char *name() { return "exa_makeconfig"; }

  void init_options();
  void init_see_alsos();

  void run();

protected:

  void dump_synopsis (std::ostream& out, bool show_hidden = false) const;
  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_non_opt_args (const std::vector<std::string>& non_opt_args) {}
  void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:

  bool        want_group;
  std::string cluster_name;
  std::string group_name;
  std::string layout_type;
  std::string datanetwork;
  std::string nodes;
  std::string disks;
  uint        group_id;
  uint        number_of_group;
  std::vector<std::string> extra_option_list;

};


#endif  // __EXA_MAKECONFIG_H__
