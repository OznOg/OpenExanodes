/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_DGDISKRECOVER_H__
#define  __EXA_DGDISKRECOVER_H__


#include "ui/cli/src/exa_dgcommand.h"

class exa_dgdiskrecover : public exa_dgcommand
{

 public:
    static const std::string OPT_ARG_OLD_DISK_UUID;
    static const std::string OPT_ARG_NEW_DISK_UUID;

    static constexpr const char *name() { return "exa_dgdiskrecover"; }

    void init_options();
    void init_see_alsos();

    void run();

protected:
    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

 private:
    std::string _node;
    std::string _old_disk;
    std::string _new_disk;
};


#endif  // __EXA_DGDISKRECOVER_H__
