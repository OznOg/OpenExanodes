/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_CLNODEADD_H__
#define __EXA_CLNODEADD_H__


#include "ui/cli/src/exa_clcommand.h"
#include "ui/cli/src/cli.h"
#include "common/include/exa_config.h"


class exa_clnodeadd: public exa_clcommand
{
public:

    static const std::string OPT_ARG_CONFIG_FILE;
    static const std::string OPT_ARG_DISK_PATH;
    static const std::string OPT_ARG_NODE_HOSTNAME;
    static const std::string OPT_ARG_DATANETWORK_HOSTNAME;

  exa_clnodeadd();
  ~exa_clnodeadd();

  static constexpr const char *name() { return "exa_clnodeadd"; }

  void init_options();
  void init_see_alsos();

  void run();

 protected:

  void dump_short_description (std::ostream& out, bool show_hidden = false) const;
  void dump_full_description(std::ostream& out, bool show_hidden = false) const;
  void dump_examples(std::ostream& out, bool show_hidden = false) const;

  void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

  xmlDocPtr _xml_cfg;
  std::string _node_name;
  std::string _disks;
  std::string _datanetwork;
};



#endif /* __EXA_CLNODEADD_H__ */
