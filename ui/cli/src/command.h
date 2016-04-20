/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMAND_H__
#define __COMMAND_H__


#include "ui/cli/src/command_option.h"
#include "ui/cli/src/command_arg.h"
#include "ui/common/include/exabase.h"
#include "ui/common/include/notifier.h"
#include <functional>



class AdmindCommand;
class AdmindMessage;



#define EXA_CLI_MAX_NODE_PER_LINE 6 /* We do line wrapping after this number of nodes
                                       being displayed */

class Line : public boost::noncopyable
{
public:

    Line(Exabase &exa, const AdmindMessage &message, bool _in_progress_hidden);
    Line(Exabase &_exa);
    ~Line();

    void process(const AdmindMessage &message);
    static void output_info(Exabase &exa,
                            bool error,
                            const std::map<std::string,
                                           std::set<std::string> > &infos);

    const std::string description;

private:

    Exabase &exa;
    bool in_progress_hidden;
    std::map<std::string, std::set<std::string> >errors;
    std::map<std::string, std::set<std::string> >warnings;
};





/**
   Command class.

   This class provides facilities to add command line option and
   argument definition so as to automatically parse them and automate
   command synopsis/usage as much as possible.
*/
class Command
{
public:

    typedef std::shared_ptr<Command>(*factory_t)(int argc, char *argv[]);

    Command(int argc, char *argv[]);

    virtual ~Command();


    typedef std::function < void (const std::string & node,
                                    exa_error_code error_code,
                                    std::shared_ptr<const AdmindMessage>)
	> by_node_func;

    typedef std::function < void (const std::string & node,
                                    AdmindCommand & command) >
    per_node_modify_func;


    exa_error_code get_param(std::shared_ptr<xmlDoc> &cfg);
    exa_error_code get_configclustered(std::shared_ptr<xmlDoc> &cfg);

    void display_usage_from_pod(std::string &msg);

    std::shared_ptr<AdmindMessage> send_command(const AdmindCommand &command,
                                                  const std::string &summary,
                                                  exa_error_code &error_code,
                                                  std::string &error_message);

    std::shared_ptr<AdmindMessage> send_admind_to_node(const std::string &node,
                                                         AdmindCommand &command,
                                                         exa_error_code &error_code);

    unsigned int send_admind_by_node(AdmindCommand &command,
                                     std::set<std::string>nodes,
                                     by_node_func func = NULL,
                                     per_node_modify_func modify_func = NULL);

    void hide_in_progress();
    void show_in_progress();
    exa_error_code set_cluster_from_cache(const std::string &clustername,
                                          std::string &error_msg);

    /* FIXME These methods shouldn't be public, but it will do for now
     * (instead, the factory should be friend, etc)*/
    virtual void init_options() = 0;
    virtual void init_see_alsos() = 0;

    void parse ();

    virtual void run() = 0;

    void show_version() const;

protected:

    static const std::string TIMEOUT_ARG_NAME;

    std::string get_name () const { return _argv[0]; }

    /**
     * Add an option to the command.
     *
     * @param[in] short_opt     The character used for the short-format of the
     *                          option
     * @param[in] long_opt      The string used for the long-format of the
     *                          option
     * @param[in] description   The string displayed to descript the option
     * @param[in] param_group   The group ID of parameters of the option. Each
     *                          group of ID greater than 0 contains exclusive
     *                          options (one of them being mandatory), and the
     *                          group 0 contains optional, non-exclusive options
     * @param[in] hidden        Whether the option is displayed by --help and in
     *                          the documentation, or only by -H
     * @param[in] arg_expected  Whether this option takes an argument (or is just
     *                          a switch)
     * @param[in] arg_name      The name of the argument, if arg_expected is true
     * @param[in] default_value The default value (of the option ? or arg ?)
     */
    void add_option (const char short_opt,
		     const std::string &long_opt,
		     const std::string &description,
		     int param_group,
		     bool hidden = false,
		     bool arg_expected = false,
		     const std::string &arg_name = "ARG",
		     const std::string &default_value = "");

    /**
     * Add an option to the command.
     *
     * @param[in] short_opt     The character used for the short-format of the
     *                          option
     * @param[in] long_opt      The string used for the long-format of the
     *                          option
     * @param[in] description   The string displayed to descript the option
     * @param[in] param_group   The groups IDs of parameters of the option. Each
     *                          group of ID greater than 0 contains exclusive
     *                          options (one of them being mandatory), and the
     *                          group 0 contains optional, non-exclusive options
     * @param[in] hidden        Whether the option is displayed by --help and in
     *                          the documentation, or only by -H
     * @param[in] arg_expected  Whether this option takes an argument (or is just
     *                          a switch)
     * @param[in] arg_name      The name of the argument, if arg_expected is true
     * @param[in] default_value The default value (of the option ? or arg ?)
     */
    void add_option (const char short_opt,
		     const std::string &long_opt,
		     const std::string &description,
		     const std::set<int>& param_groups/* = std::set<int>()*/,
		     bool hidden = false,
		     bool arg_expected = false,
		     const std::string &arg_name = "ARG",
		     const std::string &default_value = "");

    void add_arg (const std::string& arg_name,
		  int param_group,
		  bool hidden,
		  const std::string &default_value = "",
		  bool multiple = false);

    void add_arg (const std::string& arg_name,
		  const std::set<int>& param_groups,
		  bool hidden,
		  const std::string &default_value = "",
		  bool multiple = false);

    void add_see_also (const std::string& see_also);

    virtual void parse_opt_args (const std::map<char, std::string>& opt_args) = 0;
    virtual void parse_non_opt_args (const std::vector<std::string>& non_opt_args) = 0;

    void dump_usage (std::ostream& out, bool show_hidden = false) const;

    void dump_section (std::ostream& out,
		       const std::string& section,
		       bool show_hidden = false) const;

    virtual void dump_short_description (std::ostream& out, bool show_hidden = false) const = 0;

    virtual void dump_synopsis (std::ostream& out, bool show_hidden = false) const;
    virtual void dump_full_description (std::ostream& out, bool show_hidden = false) const = 0;
    virtual void dump_examples (std::ostream& out, bool show_hidden = false) const = 0;
    virtual void dump_see_also (std::ostream& out, bool show_hidden = false) const;
    virtual void dump_output_section (std::ostream& out, bool show_hidden = false) const;

    static std::string Boldify (const std::string& s);


    void Subtitle(std::ostream& out, const std::string& s) const;
    void ItemizeBegin(std::ostream& out) const;
    void ItemizeEnd(std::ostream& out) const;
    void Item(std::ostream& out, const std::string& item, const std::string& desc) const;

    static size_t Get_tty_cols();

    static void indented_dump (const std::string& s,
			       std::ostream &out,
			       size_t indent_level = 0,
			       size_t width = Get_tty_cols(),
			       bool prepad = true,
			       bool postpad = false,
			       char pad_char = ' ');

private:

    void add_to_param_groups(const std::shared_ptr<CommandParam>& param);
    void generate_valid_param_combinations();
    void check_provided_params(const std::map<char, std::string> &opt_args,
			       const std::vector<std::string> &non_opt_args) const;


    void handle_inprogress(const AdmindMessage &message);
    void by_node_done(const AdmindMessage &message,
                      const std::string &node,
                      std::string &payload,
                      unsigned int *errors,
                      Command::by_node_func &func);

    static size_t Get_real_string_size(const std::string& s);

protected:

    Exabase exa;

private:

    int _argc;
    char **_argv;
    CommandOptions _options;  //!< valid command options definition
    CommandArgs _args;  //!< valid command arguments definition
    std::map<int,CommandParams> _param_groups; //!< groups of exclusive parameters
    std::set<CommandParams> _valid_combinations;  //!< valid combinations of command parameters
    std::set<std::string> _see_also;

    bool _show_hidden;  //!< if is set, display help for experts
    unsigned int _timeout; //!< max time for cli command to finish
    bool _in_progress_hidden;  //!< whether or not to display in progress messages

    SelectNotifier notifier;
    std::shared_ptr<Line>line;
    std::string in_progress_source;

};



EXA_BASIC_EXCEPTION_DECL(CommandException, exa::Exception);


template<class T>std::shared_ptr<Command>
command_factory(int argc, char *argv[])
{
    std::shared_ptr<Command> inst(new T(argc, argv));
    inst->init_options();
    inst->init_see_alsos();
    inst->parse ();
    return inst;
}

#endif /* __COMMAND_H__ */
