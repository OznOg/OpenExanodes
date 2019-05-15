/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/cli/src/command.h"
#include "ui/cli/src/command_version.h"

#include <iostream>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "common/include/exa_assert.h"
#include "common/include/exa_config.h"
#include "common/include/exa_mkstr.h"

#include "ui/common/include/common_utils.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/admindclient.h"
#include "os/include/os_getopt.h"
#include "os/include/os_time.h"

#include <functional>

#ifdef WIN32
#include <wincon.h>  /* For GetConsoleScreenBufferInfo() */
#else
#include <sys/ioctl.h>
#endif

using boost::lexical_cast;
using std::shared_ptr;
using std::string;
using std::map;
using std::placeholders::_1;
using std::placeholders::_2;
using std::set;

const std::string Command::TIMEOUT_ARG_NAME(Command::Boldify("TIMEOUT"));

/* Maximun number of retry when we get a busy from admind */
#define MAX_RETRY_ON_BUSY 10

Line::Line(Exabase &_exa,
           const AdmindMessage &message,
           bool _in_progress_hidden) :
    description(message.get_description()),
    exa(_exa),
    in_progress_hidden(_in_progress_hidden)
{
    if (!in_progress_hidden)
        exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", description.c_str());
}


Line::Line(Exabase &_exa) :
    description(),
    exa(_exa),
    in_progress_hidden(true)
{}


Line::~Line()
{
    if (!in_progress_hidden)
    {
        if (!errors.empty())
            exa_cli_error("%sERROR%s\n", COLOR_ERROR, COLOR_NORM);
        else if (!warnings.empty())
            exa_cli_warning("%sWARNING%s\n", COLOR_WARNING, COLOR_NORM);
        else
            exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
    }

    output_info(exa, false, warnings);
    output_info(exa, true, errors);
}


void Line::process(const AdmindMessage &message)
{
    string node = message.get_error_node();

    if (node.empty())
        node = message.get_connected_node();

    switch (get_error_type(message.get_error_code()))
    {
    case ERR_TYPE_ERROR:
        errors[message.get_error_msg()].insert(node);
        break;

    case ERR_TYPE_WARNING:
    case ERR_TYPE_INFO:
        warnings[message.get_error_msg()].insert(node);
        break;

    case ERR_TYPE_SUCCESS:
        break;
    }
}


void Line::output_info(Exabase &exa, bool error,
                       const map<string, set<string> > &infos)
{
    map<string, set<string> >::const_iterator it;

    for (it = infos.begin(); it != infos.end(); ++it)
    {
        string message;

        /* WARNING: Bug#2518 : Do not display the list of nodes in case the error
         *          is ADMIND_ERR_NODE_DOWN because it's confusing. The list of nodes
         *          we display after the error is the one that have answered us
         *          but the error message in case of ADMIND_ERR_NODE_DOWN let the user
         *          think these are the down nodes.
         *          Since we don't have the error code at that point, we test the error
         *          message itself.
         */
        if (it->first == exa_error_msg(ADMIND_ERR_NODE_DOWN))
            message = it->first;
        else
        {
            if (exa.get_hostnames() != it->second)
                message = it->first + " (" +
                          strjoin(" ",
                                  exa_unexpand(strjoin(" ", it->second))) + ")";
            else
                message = it->first;
        }

        if (error)
            exa_cli_error("%sERROR%s: %s\n",
                          COLOR_ERROR,
                          COLOR_NORM,
                          message.c_str());
        else
            exa_cli_warning("%sWARNING%s: %s\n",
                            COLOR_WARNING,
                            COLOR_NORM,
                            message.c_str());
    }
}



Command::Command()
        : _options()
          , _args()
          , _param_groups()
          , _see_also()
          , _show_hidden(true)
          , _timeout(0)
          , _in_progress_hidden(true)
{
    add_option('h', "help", "Display this help and exit.", 0, false, false);
    add_option('H', "HELP", "Display expert help", 0, true, false);
    add_option('v', "version", "Display version information and exit.",
               0, false, false);
}


void Command::add_option(const char short_opt,
                         const std::string &long_opt,
                         const std::string &description,
                         int param_group,
                         bool hidden,
                         bool arg_expected,
                         const std::string &arg_name,
                         const std::string &default_value)
{
    std::set<int> param_groups;
    /* 0 is a special value for "not mandatory" */
    if (param_group != 0)
        param_groups.insert(param_group);

    add_option(short_opt, long_opt, description, param_groups,
               hidden, arg_expected, arg_name, default_value);
}


void Command::add_option(const char short_opt,
                         const std::string &long_opt,
                         const std::string &description,
                         const std::set<int> &param_groups,
                         bool hidden,
                         bool arg_expected,
                         const std::string &arg_name,
                         const std::string &default_value)
{
    if (_options.find(short_opt) != _options.end())
        throw CommandException(
            "Option config error, short option -" + std::string(1, short_opt)
            + " already reserved.");

    std::shared_ptr<CommandOption> sp(new CommandOption(short_opt,
                                                          long_opt,
                                                          description,
                                                          param_groups,
                                                          hidden,
                                                          arg_expected,
                                                          arg_name,
                                                          default_value));
    _options.insert(std::make_pair(short_opt, sp));
}


void Command::add_arg(const std::string &arg_name,
                      int param_group,
                      bool hidden,
                      const std::string &default_value,
                      bool multiple)
{
    std::set<int> param_groups;
    /* 0 is a special value for "not mandatory" */
    if (param_group != 0) param_groups.insert(param_group);
    add_arg(arg_name, param_groups, hidden, default_value, multiple);
}


void Command::add_arg(const std::string &arg_name,
                      const std::set<int> &param_groups,
                      bool hidden,
                      const std::string &default_value,
                      bool multiple)
{
    std::shared_ptr<CommandArg> sp(new CommandArg(_args.size(),
                                                    arg_name,
                                                    param_groups,
                                                    hidden,
                                                    default_value,
                                                    multiple));

    if ((_args.empty() == false) &&
        (_args.find(_args.size() - 1)->second->is_multiple()))
        throw CommandException(
            "Only one multiple arg allowed at the end of arg sequence");
    _args.insert(std::make_pair(sp->get_position(), sp));
}


void Command::add_see_also(const std::vector<std::string> &see_also)
{
    _see_also.insert(see_also.begin(), see_also.end());
}

void Command::add_see_also(const std::string &see_also)
{
    _see_also.insert(see_also);
}


void Command::add_to_param_groups(const std::shared_ptr<CommandParam> &param)
{
    if (!param->is_mandatory())
        return;

    for (std::set<int>::const_iterator git = param->get_param_groups().begin();
         git != param->get_param_groups().end(); ++git)
    {
        int group(*git);
        if (_param_groups.find(group) == _param_groups.end())
            _param_groups.insert(std::make_pair(group, CommandParams()));
        if ((_show_hidden == false) && (param->is_hidden()))
            continue;

        _param_groups[group].push_back(param);
    }
}


void Command::parse(int _argc, char *_argv[])
{
    /* FIXME _command_name is a hack to access the command name from here.
     * it should probably be given by the command itself, but:
     *   - it was done thru argv[0] in the legacy code
     *   - I plan to deeply rework hierarchy, thus I don't really want to go
     *   thru the 47 commands to add a function that will be removed after
     */
    _command_name = _argv[0];
    static struct option null_long_opt = { NULL, 0, NULL, 0};

    /* prepare parameters for getopt_long calls */
    std::stringstream ss;
    /* malloc is mandatory because ICC does not like non const size arrays. */
    struct option *long_opts = new struct option[_options.size() + 1];

    long_opts[_options.size()] = null_long_opt;

    int i(0);
    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
    {
        ss << it->first;
        if (it->second->is_arg_expected())
            ss << ':';

        struct option opt = { it->second->get_long_opt().c_str(),
                              (int) it->second->is_arg_expected(),
                              NULL,
                              it->first};
        long_opts[i++] = opt;
    }

    std::string short_opts = ss.str();
    std::map<char, std::string> opt_args;  /* fill without check first */

    /* Reinitialize getopt internal global variables to
     * enable multiple calls to parse */
    optind = 1;
    optarg = NULL;
    /* This prevents getopt from printing error messages by itself */
    opterr = 0;

    while (true)
    {
        int c = os_getopt_long(_argc, _argv,
                               short_opts.c_str(), long_opts, NULL);
        if (c == -1)
            break;

        if (c == '?')
        {
            delete[] long_opts;

            if (optopt != 0)
            {
                char short_opt_str[3];
                os_snprintf(short_opt_str, sizeof(short_opt_str), "-%c", optopt);
                throw CommandException("Unknown short option " +
                                       std::string(short_opt_str));
            }
            else
                throw CommandException("Unknown long option " +
                                       std::string(_argv[optind - 1]));
        }

        opt_args.insert(std::make_pair((char) c,
                                       (optarg !=
                                        NULL) ? std::string(optarg) : std::
                                       string()));
    }

    delete[] long_opts;

    if (opt_args.find('h') != opt_args.end())
        _show_hidden = false;

    /* fill param groups with options */
    for (CommandOptions::const_iterator it = _options.begin();
         it != _options.end(); ++it)
        add_to_param_groups(it->second);

    /* fill param groups with non-opt args */
    for (CommandArgs::const_iterator it = _args.begin();
         it != _args.end();
         ++it)
        add_to_param_groups(it->second);

    generate_valid_param_combinations();

    /* real opt values parsing starts here */

    /* special options, getting help and version */
    if (opt_args.find('H') != opt_args.end()
        || opt_args.find('h') != opt_args.end())
    {
        std::ostringstream output;
        dump_usage(output, _show_hidden);
        os_colorful_fprintf(stdout, "%s", output.str().c_str());

        exa::Exception ex("", EXA_SUCCESS);
        throw ex;
    }

    if (opt_args.find('v') != opt_args.end())
    {
        show_version();
        exa::Exception ex("", EXA_SUCCESS);
        throw ex;
    }

    std::vector<std::string> non_opt_args;
    for (int i = optind; i < _argc; ++i)
    {
        if (i + 1 >= (int) _args.size()
            && _args.empty() ==  false
            && _args.find(_args.size() - 1)->second->is_multiple())
        {
            if (non_opt_args.empty())
                non_opt_args.push_back("");

            /* handle multiple arg */
            non_opt_args[_args.size() - 1] = non_opt_args[_args.size() - 1]
                                             + " " + std::string(_argv[i]);
        }

        non_opt_args.push_back(std::string(_argv[i]));
    }

    check_provided_params(opt_args, non_opt_args);

    /* bulk parsing is done, call overriden methods */
    parse_opt_args(opt_args);
    parse_non_opt_args(non_opt_args);
}


struct CommandParamCmp
{
    bool operator ()(const std::shared_ptr<CommandParam> &op1,
                     const std::shared_ptr<CommandParam> &op2)
    {
        std::shared_ptr<CommandArg> arg1 =
            std::dynamic_pointer_cast<CommandArg>(op1);
        std::shared_ptr<CommandArg> arg2 =
            std::dynamic_pointer_cast<CommandArg>(op2);

        if (arg1.get() == NULL && arg2.get() != NULL)
            return true;
        if (arg1.get() != NULL && arg2.get() == NULL)
            return false;

        if (arg1.get() == NULL && arg2.get() == NULL)
        {
            std::shared_ptr<CommandOption> opt1 =
                std::dynamic_pointer_cast<CommandOption>(op1);
            std::shared_ptr<CommandOption> opt2 =
                std::dynamic_pointer_cast<CommandOption>(op2);

            char c1(opt1->get_short_opt());
            char c2(opt2->get_short_opt());

            if (tolower(c1) == tolower(c2))
                return c2 < c1;
            return tolower(c1) < tolower(c2);
        }

        return arg1->get_position() < arg2->get_position();
    }
};

void Command::generate_valid_param_combinations()
{
    /* generate valid combinations of command parameters */
    /* this is the cartesian product of all groups of params */

    /* example : */
    /* group 1 : a, b, c */
    /* group 2 : d, e */
    /* this function will then generate (a,d) (b,d) (c,d) (a,e) (b,e) (c,e) */

    std::deque<CommandParams> combinations;
    combinations.push_back(CommandParams());

    for (std::map<int, CommandParams>::const_iterator it = _param_groups.begin();
         it != _param_groups.end(); ++it)
    {
        CommandParams group = it->second;

        int nb(combinations.size());
        while (nb > 0)
        {
            CommandParams co = combinations.front();
            combinations.pop_front();
            for (size_t i = 0; i < group.size(); ++i)
            {
                CommandParams nco(co);
                std::shared_ptr<CommandParam> gco = group.at(i);

                /* check that this opt is not in a param group */
                /* from a previously chosen param */
                bool reject(false);
                for (CommandParams::const_iterator cit = co.begin();
                     cit != co.end(); ++cit)
                    for (std::set<int>::const_iterator itg =
                             gco->get_param_groups().begin();
                         itg != gco->get_param_groups().end(); ++itg)
                        if ((*cit)->get_param_groups().find(*itg) !=
                            (*cit)->get_param_groups().end())
                        {
                            reject = true;
                            break;
                        }

                if (!reject)
                    nco.push_back(gco);

                combinations.push_back(nco);
            }
            --nb;
        }
    }

    CommandParamCmp cmp;
    /* keep only combinations that cover all mandatory groups (hacky) */
    for (std::deque<CommandParams>::iterator it = combinations.begin();
         it != combinations.end(); ++it)
    {
        size_t nb_groups(0);
        for (CommandParams::const_iterator it2 = it->begin();
             it2 != it->end(); ++it2)
            nb_groups += (*it2)->get_param_groups().size();

        if (nb_groups == _param_groups.size())
        {
            CommandParams comb(*it);
            std::sort(comb.begin(), comb.end(), cmp);
            _valid_combinations.insert(comb);
        }
    }
}


void Command::check_provided_params(const std::map<char, std::string> &opt_args,
                                    const std::vector<std::string> &
                                    non_opt_args) const
{
    std::set<int> provided;
    std::map<int, std::string> group_desc;
    for (std::map<int, CommandParams>::const_iterator it = _param_groups.begin();
         it != _param_groups.end(); ++it)
    {
        std::stringstream group_str;
        if (it->second.size() > 1)
            group_str << "(";
        for (CommandParams::const_iterator itp = it->second.begin();
             itp != it->second.end(); ++itp)
        {
            if (itp != it->second.begin())
                group_str << ", ";
            group_str << *(*itp);
        }
        if (it->second.size() > 1)
            group_str << ")";
        group_desc.insert(std::make_pair(it->first, group_str.str()));
    }

    /* check of opt-args */
    for (std::map<char, std::string>::const_iterator it = opt_args.begin();
         it != opt_args.end(); ++it)
    {
        CommandOptions::const_iterator oit = _options.find(it->first);

        if (oit == _options.end())
            throw CommandException("Illegal option");

        std::shared_ptr<CommandOption> option(oit->second);
        if (option->is_arg_expected() && it->second.empty())
        {
            std::stringstream msg;
            msg << "Missing argument for option -" <<
            ((char) option->get_short_opt())
                << ".";
            throw CommandException(msg.str());
        }

        for (std::set<int>::const_iterator mit =
                 option->get_param_groups().begin();
             mit != option->get_param_groups().end(); ++mit)
        {
            if (provided.find(*mit) != provided.end())
            {
                std::stringstream msg;
                msg << "Only one of " << group_desc.find(*mit)->second <<
                " should be provided.";
                throw CommandException(msg.str());
            }
            provided.insert(*mit);
        }
    }

    /* check of non-opt args */
    int pos = 0;
    for (std::vector<std::string>::const_iterator it = non_opt_args.begin();
         it != non_opt_args.end(); ++it)
    {
        if (_args.empty() == false
            && _args.find(_args.size() - 1)->second->is_multiple() == true
            && pos + 1 > (int) _args.size())
            continue;
        CommandArgs::const_iterator ait = _args.find(pos);

        if (ait == _args.end())
            throw CommandException("Unexpected extra argument '" + (*it) + "'.");

        std::shared_ptr<CommandArg> arg(ait->second);
        for (std::set<int>::const_iterator mit = arg->get_param_groups().begin();
             mit != arg->get_param_groups().end(); ++mit)
        {
            if (provided.find(*mit) != provided.end())
            {
                std::stringstream msg;
                msg << "Only one of " << group_desc.find(*mit)->second <<
                " should be provided.";
                throw CommandException(msg.str());
            }
            provided.insert(*mit);
        }
        ++pos;
    }

    /* finally, check for non provided groups */
    for (std::map<int, CommandParams>::const_iterator it = _param_groups.begin();
         it != _param_groups.end(); ++it)
        if (provided.find(it->first) == provided.end())
        {
            std::stringstream msg;
            msg << ((it->second.size() > 1) ? "One of " : "") <<
            group_desc.find(it->first)->second << " must be provided.";
            throw CommandException(msg.str());
        }
}


void Command::show_version() const
{
    indented_dump(get_version(), std::cout);
}


void Command::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    if (opt_args.find('T') != opt_args.end())
    {
        int timeout(exa::to_uint32(opt_args.find('T')->second));
        _timeout = timeout;
    }

    if (opt_args.find('I') != opt_args.end())
        _in_progress_hidden = false;

    if (opt_args.find('C') != opt_args.end())
    {
        COLOR_USED = "";
        COLOR_INFO = "";
        COLOR_NORM = "";
        COLOR_ERROR = "";
        COLOR_WARNING = "";
        COLOR_BOLD = "";
        COLOR_FEATURE = "";
    }
}


void Command::parse_non_opt_args(const std::vector<std::string> &non_opt_args)
{
    /* dummy */
}


/**
 * @ Contact an admind and return the param tree.
 *
 * @param[out] cfg  The remote param tree or NULL (an error message is displayed).
 *
 * @return an error code
 */
exa_error_code Command::get_param(shared_ptr<xmlDoc> &cfg)
{
    exa_error_code error_code;
    string error_message;

    AdmindCommand command("getparam", exa.get_cluster_uuid());

    shared_ptr<AdmindMessage> message(send_command(command, "", error_code,
                                                   error_message));

    if (!message)
    {
        exa_cli_error("%sERROR%s: We failed to retrieve the list of nodes "
                      "of your cluster from the node '%s'.\n       Please "
                      "try again this command with another node.\n",
                      COLOR_ERROR, COLOR_NORM,
                      exa.get_hostnames().begin()->c_str());

        /* return an empty document as no message was received */
        cfg = shared_ptr<xmlDoc>();
        return error_code;
    }
    else if (get_error_type(error_code) != ERR_TYPE_SUCCESS)
        /* FIXME why exit here ? May just return error_code. */
        throw CommandException("Failed to receive a valid response from "
                               "admind", error_code);

    cfg =  shared_ptr<xmlDoc>(xmlReadMemory(
                                  (*message).get_payload().c_str(),
                                  (*message).get_payload().size(),
                                  NULL, NULL, XML_PARSE_NOBLANKS |
                                  XML_PARSE_NOERROR | XML_PARSE_NOWARNING),
                              xmlFreeDoc);

    if (!cfg || !cfg->children || !cfg->children->name
        || !xmlStrEqual(cfg->children->name, BAD_CAST("Exanodes")))
        throw CommandException(
            "Failed to parse admind returned initialization file");

    EXA_ASSERT_VERBOSE(cfg->children->children,
                       "No cluster element found in the tree");

    return error_code;
}


/**
 * Contact an admind and return the config tree.
 *
 * @param[out] cfg  The remote config tree or NULL (an error message is
 *                  displayed).
 *
 * @return an error code
 */
exa_error_code Command::get_configclustered(shared_ptr<xmlDoc> &cfg)
{
    exa_error_code error_code;
    string error_message;

    AdmindCommand command("getconfigcluster", exa.get_cluster_uuid());

    shared_ptr<AdmindMessage> message(send_command(command, "", error_code,
                                                   error_message));

    if (!message)
    {
        cfg = shared_ptr<xmlDoc>();
        return error_code;
    }
    else if (get_error_type(error_code) != ERR_TYPE_SUCCESS)
        throw CommandException("Failed to receive a valid response from admind",
                               error_code);

    cfg = shared_ptr<xmlDoc>(
        xmlReadMemory(
            (*message).get_payload().c_str(), (*message).get_payload().size(),
            NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOERROR |
            XML_PARSE_NOWARNING),
        xmlFreeDoc);

    if (!cfg || !cfg->children || !cfg->children->name
        || !xmlStrEqual(cfg->children->name, BAD_CAST("Exanodes")))
        throw CommandException(
            "Failed to parse admind returned initialization file");

    EXA_ASSERT_VERBOSE(cfg->children->children,
                       "No cluster element found in the tree");

    if (exa.update_cache_from_config(cfg.get(), error_message) != EXA_SUCCESS)
        exa_cli_warning("%sWARNING%s: %s\n", COLOR_WARNING, COLOR_NORM,
                        error_message.c_str());

    return error_code;
}


static void handle_progressive_payload(const AdmindMessage &message,
                                       string &payload)
{
    exa_cli_trace("Command::handle_progressive_payload: received: %s\n",
                  message.dump().c_str());

    xmlNodePtr value_node;
    shared_ptr<xmlDoc> doc = message.get_subtree();
    value_node = xml_conf_xpath_singleton(doc.get(), "/string");

    if (!value_node)
        throw "Exanodes protocol error: no string value in payload.";

    xmlChar *_str = xmlGetProp(value_node, BAD_CAST("value"));
    /* Concat the payload */
    payload += std::string((char *) _str);
    xmlFree(_str);

    exa_cli_trace("Command::handle_progressive_payload: received payload: %s\n",
                  payload.c_str());
}


static void leader_done(const AdmindMessage &message,
                        shared_ptr<AdmindMessage> *retval)
{
    exa_cli_trace("Command::send_command: received: %s\n",
                  message.dump().c_str());

    *retval = shared_ptr<AdmindMessage>(new AdmindMessage(message));
}


static void leader_warning(const string &hostname, const string &info,
                           map<string, set<string> > *warnings)
{
    (*warnings)[info].insert(hostname);
}


static void leader_error(const string &info, string *error_message)
{
    exa_cli_error("%sERROR%s: %s\n", COLOR_ERROR, COLOR_NORM, info.c_str());
    *error_message = info;
}


shared_ptr<AdmindMessage> Command::send_command(const AdmindCommand &command,
                                                const string &summary,
                                                exa_error_code &error_code,
                                                string &error_message)
{
    AdmindClient client(notifier);

    shared_ptr<AdmindMessage> message;
    string payload;
    map<string, set<string> > warnings;

    exa.log(string("Sending ") + command.get_summary() + string(" to leader"));
    exa_cli_trace("Command::send_command: %s\n",
                  command.get_xml_command(true).c_str());

    set<string> nodes = exa.get_hostnames();

    for (unsigned int attempt = 0; attempt < MAX_RETRY_ON_BUSY; ++attempt)
    {
        assert(!line);

        error_message = "";
        message = shared_ptr<AdmindMessage>();

        client.send_leader(command, nodes,
                           std::bind(&Command::handle_inprogress, this, _1),
                           std::bind(handle_progressive_payload, _1,
                                     std::ref(payload)),
                           std::bind(leader_done, _1, &message),
                           std::bind(leader_warning, _1, _2, &warnings),
                           std::bind(leader_error, _1, &error_message),
                           _timeout);

        notifier.run();

        line = shared_ptr<Line>();

        if (!message || message->get_error_code() != EXA_ERR_ADM_BUSY)
            break;

        exa_cli_log("Command::send_command: got EXA_ERR_ADM_BUSY, waiting and "
                    "retrying... (%i/%i)\n", attempt + 1, MAX_RETRY_ON_BUSY);

        os_sleep(1);
    }

    /* We don't want to annoy the user by telling him how difficult it
     * was to find the leader, if we managed to do it in the end. But if
     * we failed, then he might be interested in knowing why! */
    if (!message)
        Line::output_info(exa, false, warnings);

    error_code = message ? message->get_error_code() : EXA_ERR_CONNECT_SOCKET;

    if (!summary.empty())
    {
        exa_cli_info("%-" exa_mkstr(FMT_TYPE_H1) "s ", summary.c_str());

        switch (get_error_type(error_code))
        {
        case ERR_TYPE_SUCCESS:
            exa_cli_info("%sSUCCESS%s\n", COLOR_INFO, COLOR_NORM);
            break;

        case ERR_TYPE_INFO:
        case ERR_TYPE_WARNING:
            exa_cli_warning("%sWARNING%s\n", COLOR_WARNING, COLOR_NORM);
            break;

        case ERR_TYPE_ERROR:
            exa_cli_error("%sERROR%s\n", COLOR_ERROR, COLOR_NORM);
            break;
        }
    }

    /* We only give the defail of the error message if we have a
     * message, because otherwise we only have EXA_ERR_DEFAULT, which
     * isn't very helpful. */
    if (message)
    {
        exa.log(message->get_summary());

        switch (get_error_type(error_code))
        {
        case ERR_TYPE_SUCCESS:
            break;

        case ERR_TYPE_INFO:
        case ERR_TYPE_WARNING:
            exa_cli_warning("%sWARNING%s: %s\n", COLOR_WARNING, COLOR_NORM,
                            message->get_error_msg().c_str());
            break;

        case ERR_TYPE_ERROR:
            exa_cli_error("%sERROR%s: %s\n", COLOR_ERROR, COLOR_NORM,
                          message->get_error_msg().c_str());
            break;
        }
    }
    /* FIXME: Maybe there should be an "else" here to say something like
     * "protocol error"? */
    if (message && !payload.empty())
        message->set_payload(payload);

    return message;
}



shared_ptr<AdmindMessage> Command::send_admind_to_node(
    const string &node,
    AdmindCommand &command,
    exa_error_code &
    error_code)
{
    AdmindClient client(notifier);

    shared_ptr<AdmindMessage> answer;
    string payload;

    assert(!line);

    exa.log(string("Sending ") + command.get_summary() + string(" to ") + node);
    exa_cli_trace("Command::send_admind_to_node: sending to %s: %s\n",
                  node.c_str(), command.get_xml_command(true).c_str());

    auto to_node_done = [&answer](const AdmindMessage &message) {
        exa_cli_trace("Command::send_admind_to_node: received: %s\n",
                message.dump().c_str());

        answer.reset(new AdmindMessage(message));
    };

    auto to_node_error = [&node] (const std::string &message) {
        exa_cli_error("%sERROR%s, %s: %s\n", COLOR_ERROR, COLOR_NORM,
                      node.c_str(), message.c_str());
    };

    client.send_node(command, node,
                     [this] (const AdmindMessage &message) { this->handle_inprogress(message); },
                     [&payload] (const AdmindMessage &message) { handle_progressive_payload(message, payload); },
                     to_node_done,
                     to_node_error,
                     _timeout);

    notifier.run();

    line = shared_ptr<Line>();

    if (answer && !payload.empty())
        answer->set_payload(payload);

    /* Do you see the futility of it all, now? */
    if (answer)
        exa.log(answer->get_summary());
    error_code = answer ? answer->get_error_code() : EXA_ERR_CONNECT_SOCKET;

    return answer;
}


void Command::by_node_done(const AdmindMessage &message,
                           const std::string &node,
                           std::string &payload,
                           unsigned int *errors,
                           Command::by_node_func &func)
{
    if (message.get_error_code() != EXA_SUCCESS)
        ++(*errors);

    exa.log(message.get_summary());

    exa_cli_trace("Command::send_admind_by_node: received from %s:\n%s\n",
                  node.c_str(), message.dump().c_str());

    const_cast<AdmindMessage &>(message).set_payload(payload);

    /* See http://boost.org/libs/smart_ptr/shared_ptr.htm#BestPractices */
    shared_ptr<const AdmindMessage> msgptr(new AdmindMessage(message));

    func(node, message.get_error_code(), msgptr);
}


static void by_node_error(const std::string &info, const std::string &node,
                          unsigned int *errors, Command::by_node_func &func)
{
    ++(*errors);

    exa_cli_trace("Command::send_admind_by_node: error sending to %s: %s\n",
                  node.c_str(), info.c_str());

    func(node.c_str(), EXA_ERR_CONNECT_SOCKET,
         shared_ptr<const AdmindMessage>());
}


static void default_by_node_func(const string &node, exa_error_code error_code,
                                 shared_ptr<const AdmindMessage> message)
{
    if (message)
        switch (get_error_type(error_code))
        {
        case ERR_TYPE_ERROR:
            exa_cli_error("\n%sERROR%s, %s: %s", COLOR_ERROR, COLOR_NORM,
                          node.c_str(), message->get_error_msg().c_str());
            break;

        case ERR_TYPE_INFO:
        case ERR_TYPE_WARNING:
            exa_cli_warning("\n%sWARNING%s, %s: %s", COLOR_WARNING, COLOR_NORM,
                            node.c_str(), message->get_error_msg().c_str());
            break;

        case ERR_TYPE_SUCCESS:
            exa_cli_log("\n    %*s %sSUCCESS%s", EXA_MAXSIZE_HOSTNAME,
                        node.c_str(), COLOR_INFO, COLOR_NORM);
            break;
        }
}


unsigned int Command::send_admind_by_node(AdmindCommand &command,
                                          set<string> nodes, by_node_func func,
                                          per_node_modify_func modify_func)
{
    AdmindClient client(notifier);

    set<string>::iterator it;
    unsigned int errors(0);
    /* Each payload coming from each node must be processed separately */
    map<string, string> by_node_payload;

    assert(!line);

    if (!func)
        func = default_by_node_func;

    exa_cli_trace("Command::send_admind_by_node: command: %s\n",
                  command.get_xml_command(true).c_str());

    for (it = nodes.begin(); it != nodes.end(); ++it)
    {
        /* take a reference on string inside map; as the string does not yet exist
         * it is automatically created empty (default constructor for string)
         * CAREFUL: operations are really processed in notifier.run() thus strings
         * must remain valid outside of the next for loop */
        std::string &payload = by_node_payload[*it];

        if (modify_func)
            modify_func(*it, command);

        exa.log(string("Sending ") + command.get_summary() + string(
                    " to ") + *it);
        exa_cli_trace("Command::send_admind_by_node: sending to %s\n",
                      it->c_str());
        client.send_node(command, *it,
                         std::bind(&Command::handle_inprogress, this, _1),
                         /* Careful passing a ref thru bind needs use of std::ref() */
                         std::bind(handle_progressive_payload, _1,
                              std::ref(payload)),
                         std::bind(&Command::by_node_done, this, _1, *it,
                              std::ref(payload), &errors, func),
                         std::bind(by_node_error, _1, *it, &errors, func),
                         _timeout);
    }

    notifier.run();

    line = shared_ptr<Line>();

    return errors;
}


void Command::hide_in_progress()
{
    _in_progress_hidden = true;
}


void Command::show_in_progress()
{
    _in_progress_hidden = false;
}


/* FIXME Should be moved to OS lib */
size_t Command::Get_tty_cols()
{
#ifdef WIN32
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (GetConsoleScreenBufferInfo(hStdOut, &info))
        return info.dwSize.X;
    else
        return 80;
#else
    struct winsize size;

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &size) < 0)
        return 80;
    return size.ws_col;
#endif
}


size_t Command::Get_real_string_size(const std::string &s)
{
    std::string tmp(s);

    boost::erase_all(tmp, COLOR_USED);
    boost::erase_all(tmp, COLOR_INFO);
    boost::erase_all(tmp, COLOR_NORM);
    boost::erase_all(tmp, COLOR_ERROR);
    boost::erase_all(tmp, COLOR_WARNING);
    boost::erase_all(tmp, COLOR_BOLD);
    return tmp.size();
}


void Command::handle_inprogress(const AdmindMessage &message)
{
    exa_cli_trace("Command::handle_inprogress: received from %s:\n%s\n",
                  message.get_connected_node().c_str(), message.dump().c_str());

#ifdef DEBUG
    exa.log(message.get_summary());
#endif

    /* Define the source of in-progress messages. This source is set to
     * the first node that answers. We simply ignore in-progress
     * messages sent by other nodes. Allowing all nodes to send
     * their in-progress messages and filter them at the CLI level is of
     * particular interest for the exa_clnodestart command (see bug #2641).
     */
    if (in_progress_source.empty())
        in_progress_source = message.get_connected_node();

    if (message.get_connected_node() == in_progress_source &&
        line && line->description != message.get_description())
        line = shared_ptr<Line>();

    if (!line)
        line = shared_ptr<Line>(new Line(exa, message, _in_progress_hidden));

    line->process(message);
}


exa_error_code Command::set_cluster_from_cache(const string &clustername,
                                               string &error_msg)
{
    if (exa.set_cluster_from_cache(clustername, error_msg) != EXA_SUCCESS)
    {
        exa_cli_error("%s\n", error_msg.c_str());
        exa_cli_error("%sERROR%s: If the cluster '%s' exists, "
                      "you can reconnect to it using:\n"
                      "       exa_clreconnect %s --node <one hostname>.\n",
                      COLOR_ERROR, COLOR_NORM,
                      clustername.c_str(),
                      clustername.c_str());
        return EXA_ERR_DEFAULT;
    }
    return EXA_SUCCESS;
}


void Command::dump_output_section(std::ostream &out, bool show_hidden) const
{
    /* By default, no OUTPUT section */
}


