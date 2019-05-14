/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef       __EXA_VLCREATE_H__
#define       __EXA_VLCREATE_H__

#include "ui/cli/src/exa_vlcommand.h"

class exa_vlcreate : public exa_vlcommand
{

public:

    static const std::string OPT_ARG_SIZE_SIZE;
    static const std::string OPT_ARG_EXPORT_METHOD;
    static const std::string OPT_ARG_READAHEAD_SIZE;
    static const std::string OPT_ARG_ACCESS_MODE;
    static const std::string OPT_ARG_LUN;

    exa_vlcreate();

    static constexpr const char *name() { return "exa_vlcreate"; }


    void run();

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

private:

    bool is_private;
    std::string force;
    std::string export_method;
    uint64_t sizeKB_uu64;
    bool size_max;
    int32_t lun; /* -1 means no lun specified */
    int64_t readahead; /* in KB (-1 means default readahead) */
};


#endif  // __EXA_VLCREATE_H__
