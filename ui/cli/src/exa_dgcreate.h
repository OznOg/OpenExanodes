/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_DGCREATE_H__
#define __EXA_DGCREATE_H__

#include "ui/cli/src/exa_dgcommand.h"

class exa_dgcreate : public exa_dgcommand
{

public:

    static const std::string OPT_ARG_DISK_HOSTNAMES;
    static const std::string OPT_ARG_DISK_PATH;
    static const std::string OPT_ARG_EXTRA_DGOPTION;
    static const std::string OPT_ARG_LAYOUT_LAYOUT;
    static const std::string OPT_ARG_GROUP_FILE;
    static const std::string OPT_ARG_NBSPARE_N;
    static const std::string OPT_ARG_LAYOUT_SSTRIPING;
    static const std::string OPT_ARG_LAYOUT_RAINX;

    exa_dgcreate();

    static constexpr const char *name() { return "exa_dgcreate"; }

    void init_see_alsos();

    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

    std::string groupconfig;
    bool startgroup;
    bool alldisks;
    std::string disks;
    std::string layout_type;
    std::vector<std::string> extra_option_list;
    int nb_spare;

    xmlDocPtr create_config_from_file(std::string groupconfig);
    xmlDocPtr create_config_from_param(const std::string &clustername,
                                       const std::string &diskgroup,
                                       std::vector<std::string> extra_option_list,
                                       std::string layout_type);
};


#endif /* __EXA_DGCREATE_H__ */
