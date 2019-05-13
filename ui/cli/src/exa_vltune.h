/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __EXA_VLTUNE_H__
#define __EXA_VLTUNE_H__

#include "ui/cli/src/exa_vlcommand.h"

class exa_vltune : public exa_vlcommand
{
private:

    typedef enum
    {
	VLTUNE_NONE = 0,
	VLTUNE_LIST,        /* Print current values of tunable volume's parameters. */
	VLTUNE_SET_PARAM,   /* Set value of a parameter. */
	VLTUNE_GET_PARAM,   /* Get value of a parameter. */
	VLTUNE_ADD_PARAM,   /* Add a value to a list parameter. */
	VLTUNE_REMOVE_PARAM,/* Remove a value to a list parameter. */
	VLTUNE_RESET_PARAM  /* Reset value of a parameter. */
    } vltune_mode_t;


public:

    static const std::string ARG_PARAMETER_PARAMETER;
    static const std::string ARG_PARAMETER_VALUE;

    exa_vltune();

    static constexpr const char *name() { return "exa_vltune"; }

    void init_see_alsos(void);

    void run(void);

protected:

    void dump_short_description (std::ostream& out, bool show_hidden = false) const;
    void dump_full_description(std::ostream& out, bool show_hidden = false) const;
    void dump_examples(std::ostream& out, bool show_hidden = false) const;

    void parse_opt_args (const std::map<char, std::string>& opt_args);
    void parse_non_opt_args (const std::vector<std::string>& non_opt_args);

protected:
    void cmd_tune_param(void);
    void cmd_display_param_list(void);
    void display_value_from_xml(const std::string& raw_xml);
private:
#ifdef WITH_FS
    bool nofscheck;
#endif
    vltune_mode_t _vltune_mode;
    bool verbose;
    std::string _param_name;
    std::string _param_value;
};


#endif /* __EXA_CLTUNE_H__ */
