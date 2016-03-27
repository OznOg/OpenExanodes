/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/command.h"
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "common/include/exa_constants.h"

void Command::indented_dump(const std::string &s,
                            std::ostream &out,
                            size_t indent_level,
                            size_t width,
                            bool prepad,
                            bool postpad,
                            char pad_char)
{
    if (indent_level  == 2)
    {
        out << std::endl << "=over 12" << std::endl;
        out << std::endl;
        out << "=item B<" << s << ">" << std::endl;
        out << std::endl;
    }
    else if (indent_level  == 20)
        out << s << std::endl << std::endl << "=back" << std::endl << std::endl;
    else
        out << s << std::endl;
}


std::string Command::Boldify(const std::string &s)
{
    return "B<" + s + ">";
}


void Command::Subtitle(std::ostream &out, const std::string &s) const
{
    out << std::endl;
    out << "=head2 " << s << std::endl;
    out << std::endl;
}


void Command::ItemizeBegin(std::ostream &out) const
{
    out << "=over 12" << std::endl;
    out << std::endl;
}


void Command::ItemizeEnd(std::ostream &out) const
{
    out << "=back" << std::endl;
    out << std::endl;
}


void Command::Item(std::ostream &out,
                   const std::string &item,
                   const std::string &desc) const
{
    out << "=item " << Boldify(item) << std::endl << std::endl;
    out << desc << std::endl << std::endl;
}


void Command::dump_section(std::ostream &out,
                           const std::string &section,
                           bool show_hidden) const
{
    out << "=head1 " << boost::to_upper_copy(section) << std::endl << std::endl;
}


void Command::dump_usage(std::ostream &out, bool show_hidden) const
{
    out << "=pod" << std::endl;

    std::stringstream ss;
    out << std::endl;
    ss << get_name() << " - ";
    dump_short_description(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << std::endl << std::endl;

    ss.str("");
    dump_section(out, "Synopsis", show_hidden);
    out << "=over 2" << std::endl << std::endl;
    dump_synopsis(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << "=back" << std::endl << std::endl;
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
        if ((show_hidden == false) && it->second->is_hidden()) continue;
        std::stringstream ss;

        std::string short_opt("-");
        short_opt.append(1, it->second->get_short_opt());
        std::string long_opt("--");
        long_opt.append(it->second->get_long_opt());

        ss << Boldify(short_opt)  << ", " << Boldify(long_opt);

        if (it->second->is_arg_expected()) ss << " " <<
            it->second->get_arg_name();

        str_opts.push_back(ss.str());
        size_t real_size = Get_real_string_size(ss.str());
        if (real_size > max_width)
            max_width = real_size;
    }

    size_t i(0);
    out << "=over 2" << std::endl;
    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        if ((show_hidden == false) && it->second->is_hidden()) continue;
        out << std::endl << "=item ";
        indented_dump(str_opts.at(i), out, 0, 0, true, true);
        out << std::endl;
        std::stringstream ss;
        ss << it->second->get_description();
        if (it->second->is_arg_expected() &&
            (it->second->is_mandatory() == false) &&
            (it->second->get_default_value().empty() == false))
            ss << std::endl << "Default is '" <<
            it->second->get_default_value() << "'.";
        indented_dump(ss.str(), out, 0, Get_tty_cols(), false, false);
        out << std::endl;

        ++i;
    }
    out << "=back " << std::endl;
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
    out << "=cut" << std::endl;
}


void Command::dump_synopsis(std::ostream &out, bool show_hidden) const
{
    for (std::set<CommandParams>::const_iterator it = _valid_combinations.begin();
         it != _valid_combinations.end(); ++it)
    {
        out << "=item" << std::endl;
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
        out << std::endl << std::endl;
    }
}


void Command::dump_see_also(std::ostream &out, bool show_hidden) const
{
    for (std::set<std::string>::const_iterator it = _see_also.begin();
         it != _see_also.end(); ++it)
        out << "L<" << (*it) << ">" << std::endl;
}


