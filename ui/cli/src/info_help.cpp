/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/command.h"
#include <iostream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "common/include/exa_constants.h"
#include "ui/common/include/cli_log.h"

void Command::indented_dump(const std::string &s,
                            std::ostream &out,
                            size_t indent_level,
                            size_t width,
                            bool prepad,
                            bool postpad,
                            char pad_char)
{
    std::string prep(indent_level, pad_char);

    if (s.size() > 0 && prepad)
        out << prep;

    std::vector<std::string> desc_lines;
    bool endl(true);
    boost::split(desc_lines, s, boost::algorithm::is_any_of(std::string("\n")));

    for (size_t l = 0; l < desc_lines.size(); ++l)
    {
        if (l > 0 && !endl)
            out << std::endl << prep;

        /* dump description taking care that it does not exceed Get_tty_cols() */
        std::vector<std::string> desc_tokens;
        boost::split(desc_tokens, desc_lines.at(l),
                     boost::algorithm::is_any_of(std::string(" \t")));

        size_t len(indent_level);
        for (size_t j = 0; j < desc_tokens.size(); ++j)
        {
            std::string token(desc_tokens.at(j));
            if (len + Get_real_string_size(token) + 1 >= width)
            {
                out << std::endl << prep;
                len = indent_level;
                endl = true;
            }
            if (token.empty() && (j == desc_tokens.size() - 1))
                continue;
            endl = false;

            out << token << " ";
            /* do not take into account special style characters */
            len += Get_real_string_size(token) + 1;
        }

        if (postpad && l == desc_lines.size() - 1)
            out << std::string(width - len, pad_char);
    }
}


std::string Command::Boldify(const std::string &s)
{
    return COLOR_BOLD + s + COLOR_NORM;
}


void Command::Subtitle(std::ostream &out, const std::string &s) const
{
    out << " ";  /* Ugly hack introduced because the endl were ignored... I don't know why */
    out << std::endl;
    out << Boldify(s) << std::endl;
}


void Command::ItemizeBegin(std::ostream &out) const {}

void Command::ItemizeEnd(std::ostream &out) const {}

void Command::Item(std::ostream &out,
                   const std::string &item,
                   const std::string &desc) const
{
    out.setf(std::ios_base::left, std::ios_base::adjustfield);

    out << std::string(2, ' ') << COLOR_BOLD;
    out.width(20);
    out << item << COLOR_NORM;

    indented_dump(desc, out, 22, Get_tty_cols(), false, false);
    out << std::endl;
}


void Command::dump_section(std::ostream &out,
                           const std::string &section,
                           bool show_hidden) const
{
    out << std::endl << Boldify(section) << std::endl;
    out << std::endl;
}


void Command::dump_usage(std::ostream &out, bool show_hidden) const
{
    std::stringstream ss;

    out << std::endl;
    ss << get_name() << " - ";
    dump_short_description(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl << std::endl;

    ss.str("");
    dump_section(out, "Synopsis", show_hidden);
    dump_synopsis(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl;

    ss.str("");
    dump_section(out, "Description", show_hidden);
    dump_full_description(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl;

    ss.str("");
    dump_output_section(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl;

    dump_section(out, "Options", show_hidden);

    std::vector<std::string> str_opts;
    size_t max_width(0);

    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        if (show_hidden == false && it->second->is_hidden())
            continue;
        std::stringstream ss;

        std::string short_opt("-");
        short_opt.append(1, it->second->get_short_opt());
        std::string long_opt("--");
        long_opt.append(it->second->get_long_opt());

        ss << "  ";
        ss << Boldify(short_opt)  << ", " << Boldify(long_opt);

        if (it->second->is_arg_expected())
            ss << " " << it->second->get_arg_name();

        str_opts.push_back(ss.str());
        size_t real_size = Get_real_string_size(ss.str());
        if (real_size > max_width)
            max_width = real_size;
    }

    size_t i(0);
    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        if (show_hidden == false && it->second->is_hidden())
            continue;

        indented_dump(str_opts.at(i), out, 0, max_width + 2, true, true);

        std::stringstream ss;

        ss << it->second->get_description();
        if (it->second->is_arg_expected() &&
            it->second->is_mandatory() == false &&
            it->second->get_default_value().empty() == false)
            ss << std::endl << "Default is '" <<
            it->second->get_default_value() << "'.";

        indented_dump(ss.str(), out, max_width + 2,
                      Get_tty_cols(), false, false);
        out << std::endl;

        ++i;
    }
    out << std::endl;

    dump_section(out, "Examples", show_hidden);
    ss.str("");
    dump_examples(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl;

    if (_see_also.empty() == false)
    {
        dump_section(out, "See also", show_hidden);
        ss.str("");
        dump_see_also(ss, show_hidden);
        indented_dump(ss.str(), out);
        out << std::endl;
        out << std::endl;
    }

    dump_section(out, "Author", show_hidden);
    ss.str("");
    ss << EXA_COPYRIGHT << std::endl;
    indented_dump(ss.str(), out);
    out << std::endl;
}


void Command::dump_synopsis(std::ostream &out, bool show_hidden) const
{
    for (std::set<CommandParams>::const_iterator it = _valid_combinations.begin();
         it != _valid_combinations.end(); ++it)
    {
        out << get_name() << " [OPTIONS]";
        for (CommandParams::const_iterator it2 = it->begin();
             it2 != it->end(); ++it2)
        {
            boost::shared_ptr<CommandOption> opt =
                boost::dynamic_pointer_cast<CommandOption>(*it2);

            if (opt.get() != NULL)
            {
                out << " --" << opt->get_long_opt();
                if (opt->is_arg_expected())
                    out << " " << opt->get_arg_name();
                continue;
            }

            boost::shared_ptr<CommandArg> arg =
                boost::dynamic_pointer_cast<CommandArg>(*it2);

            if (arg.get() != NULL)
            {
                if (!arg->is_mandatory())
                    out << " [" << arg->get_arg_name() <<
                    (arg->is_multiple() ? " ..." : "") << "]";
                else
                    out << " " << arg->get_arg_name()
                        << (arg->is_multiple() ? " ..." : "");
            }
        }
        out << " ";
        out << std::endl;
    }
}


void Command::dump_see_also(std::ostream &out, bool show_hidden) const
{
    for (std::set<std::string>::const_iterator it = _see_also.begin();
         it != _see_also.end(); ++it)
        out << (*it) << " ";
}


