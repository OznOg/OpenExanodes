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

void Command::indented_dump(const std::string &s,
                            std::ostream &out,
                            size_t indent_level,
                            size_t width,
                            bool prepad,
                            bool postpad,
                            char pad_char)
{
    bool first = true;
    std::string working_string = s;

    while (working_string.size())
    {
        size_t first_character = working_string.find_first_not_of(" ", 0);

        working_string = working_string.substr(
            first_character,
            working_string.size() - first_character);

        size_t last_line_character = working_string.find_first_of("\n");
        if (last_line_character == std::string::npos)
            last_line_character = working_string.size();
        if (last_line_character == 0) /* skip */
        {
            working_string = working_string.substr(
                1,
                working_string.size() - 1);
            continue;
        }

        std::string line = working_string.substr(0, last_line_character);

        working_string = working_string.substr(
            last_line_character,
            working_string.size() - last_line_character);

        std::string first_characters = line.substr(0, 4);

        if (first_characters == "exa_")
            out << "<screen>" << line << "</screen>" << std::endl;
        else
        {
            if (!first)
                out << "<para>" << std::endl;
            out << line;
            if (!first)
                out << "</para>" << std::endl;
        }

        first = false;
    }

    return;
}


std::string Command::Boldify(const std::string &s)
{
    return "<emphasis>" + s + "</emphasis>";
}


void Command::Subtitle(std::ostream &out, const std::string &s) const
{
    out << "<para><emphasis>" << s << "</emphasis></para>" << std::endl;
}


void Command::ItemizeBegin(std::ostream &out) const
{
    out << "<variablelist>" << std::endl;
}


void Command::ItemizeEnd(std::ostream &out) const
{
    out << "</variablelist>" << std::endl;
}


void Command::Item(std::ostream &out,
                   const std::string &item,
                   const std::string &desc) const
{
    out << "<varlistentry><term><option>" << Boldify(item) <<
    "</option></term>";
    out << "<listitem><para>" << desc << "</para></listitem></varlistentry>" <<
    std::endl;
}


void Command::dump_section(std::ostream &out,
                           const std::string &section,
                           bool show_hidden) const
{
    static bool previous_section = false;

    if (previous_section)
        out << "</refsection>" << std::endl;

    previous_section = true;
    out << "<refsection>" << std::endl;
    out << "<title>" << section << "</title>" << std::endl;
}


void Command::dump_usage(std::ostream &out, bool show_hidden) const
{
    std::stringstream ss;

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    out <<
    "<!DOCTYPE refentry PUBLIC \"-//OASIS//DTD DocBook XML V4.3//EN\"" <<
    std::endl;
    out << "	\"http://www.oasis-open.org/docbook/xml/4.3/docbookx.dtd\" ["<<
    std::endl;
    out << "<!ENTITY % globalent SYSTEM \"../libs/guide/global.ent\">" <<
    std::endl;
    out << "%globalent;" << std::endl;
    out << "<!ENTITY % xinclude SYSTEM \"../libs/guide/xinclude.mod\">" <<
    std::endl;
    out << "%xinclude;" << std::endl;
    out << "<!ENTITY language \"&English;\">" << std::endl;
    out << "]>" << std::endl;

    out << "<refentry id=\"" + get_name() + "\">" << std::endl;

    out << "<refmeta><refentrytitle>"  << get_name() <<
    "</refentrytitle><manvolnum>1</manvolnum></refmeta>" << std::endl;

    out << "<refnamediv><refname>"  << get_name() <<
    "</refname><refpurpose>";
    dump_short_description(ss, show_hidden);
    indented_dump(ss.str(), out);
    out << "</refpurpose></refnamediv>" << std::endl;

    dump_section(out, "Synopsis", show_hidden);
    ss.str("");
    dump_synopsis(ss, show_hidden);
    out << ss.str() << std::endl;

    dump_section(out, "Description", show_hidden);
    ss.str("");
    dump_full_description(ss, show_hidden);
    indented_dump(ss.str(), out);

    dump_output_section(out, show_hidden);

    dump_section(out, "Options", show_hidden);
    std::vector<std::string> str_opts;

    out << "<variablelist>" << std::endl;

    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        if (show_hidden == false && it->second->is_hidden())
            continue;

        std::stringstream ss;
        if (it->second->is_arg_expected())
            ss << " " << it->second->get_arg_name();

        str_opts.push_back(ss.str());
    }

    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        if (show_hidden == false && it->second->is_hidden())
            continue;

        out << "<varlistentry>" << std::endl;
        out << "<term><option>-";
        out << it->second->get_short_opt() << ",--" << it->second->get_long_opt();
        if (it->second->is_arg_expected())
            out << " " << it->second->get_arg_name();
        out << "</option></term>" << std::endl;
        out << "<listitem><para>";
        out << it->second->get_description();
        out << "</para></listitem>" << std::endl;
        out << "</varlistentry>" << std::endl;
    }

    out << "</variablelist>" << std::endl;

    dump_section(out, "Examples", show_hidden);
    ss.str("");
    dump_examples(ss, show_hidden);
    indented_dump(ss.str(), out);

    if (_see_also.empty() == false)
    {
        dump_section(out, "See also", show_hidden);
        ss.str("");
        dump_see_also(ss, show_hidden);
        out << ss.str();
    }

    out << std::endl << "</refsection>" << std::endl;

    out << "</refentry>" << std::endl;
}


void Command::dump_synopsis(std::ostream &out, bool show_hidden) const
{
    for (std::set<CommandParams>::const_iterator it = _valid_combinations.begin();
         it != _valid_combinations.end(); ++it)
    {
        out << "<cmdsynopsis><command>" <<
        get_name() << "</command>" << std::endl;

        for (CommandParams::const_iterator it2 = it->begin();
             it2 != it->end(); ++it2)
        {
            boost::shared_ptr<CommandOption> opt =
                boost::dynamic_pointer_cast<CommandOption>(*it2);

            if (opt.get() != NULL)
            {
                std::string choice;
                choice = " choice=\"plain\" ";
                out << "<arg " << choice << "><option>--" <<
                opt->get_long_opt() <<
                "</option>" << std::endl;
                if (opt->is_arg_expected())
                    out << opt->get_arg_name();
                out << "</arg>" << std::endl;
                continue;
            }

            boost::shared_ptr<CommandArg> arg =
                boost::dynamic_pointer_cast<CommandArg>(*it2);

            if (arg.get() != NULL)
            {
                std::string choice, multiple;
                choice = arg->is_mandatory() ?
                         " choice=\"plain\" " : " choice=\"opt\" ";
                multiple = arg->is_multiple() ? " rep=\"repeat\"" : "";
                out << "<arg " <<
                choice << multiple <<
                ">" << arg->get_arg_name() <<
                "</arg>" << std::endl;
            }
        }
        out << "</cmdsynopsis>" << std::endl;
    }
}


void Command::dump_see_also(std::ostream &out, bool show_hidden) const
{
    out << "<para><simplelist type=\"inline\">" << std::endl;

    for (std::set<std::string>::const_iterator it = _see_also.begin();
         it != _see_also.end(); ++it)
        out << "<member><xref linkend='" << (*it) << "'>" << (*it)
            << "</xref></member>" << std::endl;

    out << "</simplelist></para>" << std::endl;
}


