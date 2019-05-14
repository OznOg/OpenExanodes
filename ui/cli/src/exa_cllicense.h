/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXA_CLLICENSE_H__
#define __EXA_CLLICENSE_H__

#include "ui/cli/src/exa_clcommand.h"

class exa_cllicense : public exa_clcommand
{
public:
    static const std::string OPT_ARG_FILENAME;
    static const std::string OPT_ARG_HOSTNAME;

    static constexpr const char *name() { return "exa_cllicense"; }

    exa_cllicense();

    void run();

protected:
    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);

private:
    std::string _license_file;
};

#endif /* __EXA_CLLICENSE_H__ */
