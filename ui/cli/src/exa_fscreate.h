/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_FSCREATE_H__
#define  __EXA_FSCREATE_H__

#include "ui/cli/src/exa_fscommand.h"
#include "ui/cli/src/cli.h"


class exa_fscreate : public exa_fscommand
{
public:

    static const std::string OPT_ARG_MOUNTPOINT_PATH;
    static const std::string OPT_ARG_SIZE_SIZE;
    static const std::string OPT_ARG_TYPE_FSTYPE;
    static const std::string OPT_ARG_NBJOURNALS_NB;
    static const std::string OPT_ARG_RGSIZE_SIZE;

    exa_fscreate(int argc, char *argv[]);

    static constexpr const char *name() { return "exa_fscreate"; }

    void init_options();
    void init_see_alsos();

    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

private:
    std::shared_ptr<AdmindCommand> generate_config_command();

    exa_error_code create_and_send_xml_command();

    uint64_t sizeKB_uu64;
    uint64_t rg_sizeM;
    int nb_logs;
    bool size_max;            /* If true, the file system will fill the group */
    std::string fs_type;
    std::string mount_point;
};


#endif  // __EXA_FSCREATE_H__
