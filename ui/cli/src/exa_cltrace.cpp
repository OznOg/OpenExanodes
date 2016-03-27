/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_cltrace.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

#include "log/include/log.h"

#include "boost/lexical_cast.hpp"
using boost::lexical_cast;
using std::string;

/* FIXME This command shouldn't take a cluster on its commandline. It's of
 *       no use as this command should use hostnames only, never node names. */

const std::string exa_cltrace::OPT_ARG_LEVEL_LEVEL(Command::Boldify("LEVEL"));
const std::string exa_cltrace::OPT_ARG_COMPONENT_COMPONENT(Command::Boldify(
                                                               "COMPONENT"));
const std::string exa_cltrace::OPT_ARG_NODE_HOSTNAMES(Command::Boldify(
                                                          "HOSTNAMES"));

static void sort_components();

exa_cltrace::exa_cltrace(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , allnodes(false)
    , component_index(-1)
    , level_index(-1)
{
    sort_components();
}


exa_cltrace::~exa_cltrace()
{}

void exa_cltrace::init_options()
{
    std::set<int> groups;

    exa_clcommand::init_options();

    groups.insert(1);
    groups.insert(2);
    groups.insert(3);

    add_option('l', "list", "Display the list of components and levels.",
               groups, false, false);
    add_option('L', "LIST", "Expert mode, display the extended list of "
               "components and levels.", groups, true, false);

    add_option('c', "component", "Component to change the log level of.",
               1, false, true, OPT_ARG_COMPONENT_COMPONENT);

    add_option('e', "level", "New log level.", 2, false, true,
               OPT_ARG_LEVEL_LEVEL);

    add_option('n', "node", "Nodes on which to change the log level.",
               3, false, true, OPT_ARG_NODE_HOSTNAMES);

    add_option('a', "all", "Apply to all nodes of the cluster.",
               3, false, false);
}


void exa_cltrace::init_see_alsos()
{}


/* --- local data ---------------------------------------------------- */

/* components names */
struct idname
{
    int         id;
    const char *name;
    const char *descr;
    int         expert;
};

/* Not all component IDs appear here, but keep them ordered in the same order
 * they are defined.
 * XXX Shouldn't admin-{1,2,3} be expert only? */
static struct idname cnames[] =
{
    { EXAMSG_LOGD_ID,            "logd",                       "Logging daemon",
      0                         },
    { EXAMSG_CMSGD_ID,           "msgd",
      "Messaging daemon",             0                         },
    { EXAMSG_NBD_ID,             "nbd",
      "Network block device",         0                         },
    { EXAMSG_VRT_ID,             "vrt",
      "Storage virtualizer",          0                         },
    { EXAMSG_ADMIND_ID,          "admind",
      "Administration daemon",        0                         },
    { EXAMSG_ADMIND_EVMGR_ID,    "evmgr",                      "Event manager",
      0                         },
    { EXAMSG_ADMIND_INFO_ID,     "admin-1",
      "Administration thread one",    0                         },
    { EXAMSG_ADMIND_CMD_ID,      "admin-2",
      "Administration thread two",    0                         },
    { EXAMSG_ADMIND_RECOVERY_ID, "admin-3",
      "Administration thread three",  0                         },
    { EXAMSG_CSUPD_ID,           "csupd",
      "Cluster supervision daemon",   0                         },
    { EXAMSG_ISCSI_ID,           "iscsi",                      "iSCSI target",
      0                         },

#ifdef DEBUG
    { EXAMSG_TEST_ID,            "test",                       "Test component",
      1                         },
    { EXAMSG_TEST2_ID,           "test2",
      "Test component two",           1                         },
    { EXAMSG_TEST3_ID,           "test3",
      "Test component three",         1                         },
#endif

    { EXAMSG_ALL_COMPONENTS,     "all",                        "All components",
      0                         },
    { 0,                         NULL,                         NULL,
      0                         }
};

static struct idname lnames[] =
{
    { EXALOG_LEVEL_NONE,    "none",                  "Disable logs",
      1                                     },
    { EXALOG_LEVEL_ERROR,   "error",                 "Display error only",
      1                                     },
    { EXALOG_LEVEL_WARNING, "warn",                  "Warnings",
      1                                     },
    { EXALOG_LEVEL_INFO,    "info",                  "Information messages",
      0                                     },
    { EXALOG_LEVEL_DEBUG,   "debug",                 "Debugging messages",
      0                                     },
    { EXALOG_LEVEL_TRACE,   "trace",                 "Execution trace",
      1                                     },
    { 0,                    NULL,                    NULL,
      0                                     }
};

static int matchname(struct idname *in, const char *s);

static int compare_idnames(const struct idname *in1, const struct idname *in2)
{
    return strcmp(in1->name, in2->name);
}


static void sort_components()
{
    size_t n = 0;

    while (cnames[n].name != NULL)
        n++;

    qsort(cnames, n, sizeof(struct idname),
          (int(*) (const void *, const void *))compare_idnames);
}


struct cltrace_filter : private boost::noncopyable
{
    void operator ()(const std::string &node, exa_error_code err_code,
                     boost::shared_ptr<const AdmindMessage> message)
    {
        switch (err_code)
        {
        case EXA_SUCCESS:
            break;

        case EXA_ERR_CONNECT_SOCKET:
            exa_cli_warning(
                "\n%sWARNING%s: %s is unreachable. It won't be changed.\n",
                COLOR_WARNING,
                COLOR_NORM,
                node.c_str());
            break;

        default:
            if (get_error_type(err_code) == ERR_TYPE_WARNING)
                exa_cli_warning("\n%sWARNING%s: %s",
                                COLOR_WARNING,
                                COLOR_NORM,
                                exa_error_msg(err_code));
            else if (get_error_type(err_code) == ERR_TYPE_ERROR)
                exa_cli_error("\n%sERROR%s: %s",
                              COLOR_ERROR,
                              COLOR_NORM,
                              exa_error_msg(err_code));
            break;
        }
    }
};

void exa_cltrace::run()
{
    std::string error_msg;

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    exa_cli_info("Setting trace parameters:\n");
    exa_cli_info(" %-10s: %s\n", "Component", cnames[component_index].descr);
    exa_cli_info(" %-10s: %s\n", "Level", lnames[level_index].descr);

    if (allnodes)
    {
        nodelist = exa.get_hostnames();
        exa_cli_info(" %-10s: %s\n", "Nodes", "All");
    }
    else
        exa_cli_info(" %-10s: %s\n", "Nodes", strjoin(" ", nodelist).c_str());

    /* Create command */
    AdmindCommand command("cltrace", exa.get_cluster_uuid());
    command.add_param("component",
                      lexical_cast<std::string>(cnames[component_index].id));
    command.add_param("level", lexical_cast<std::string>(lnames[level_index].id));

    /* Send the command and receive the response */
    unsigned int nr_errors;

    cltrace_filter myfilter;
    nr_errors = send_admind_by_node(command, nodelist,
                                    std::ref(myfilter));

    if (nr_errors)
        exa_cli_error(
            "%sERROR%s: Failed to change the trace level on one or more "
            "nodes.\n",
            COLOR_ERROR,
            COLOR_NORM);

    if (nr_errors != 0)
        throw CommandException(EXA_ERR_DEFAULT);
}


static void _display_options(int expert)
{
    struct idname *in;

    exa_cli_info("Components:\n");
    for (in = cnames; in->name; in++)
        if (in->expert <= expert)
            exa_cli_info("  %-14s %s.\n", in->name, in->descr);

    exa_cli_info("\nLog levels:\n");
    for (in = lnames; in->name; in++)
        if (in->expert <= expert)
            exa_cli_info("  %-14s %s.\n", in->name, in->descr);
}


/** \brief Look for a string in a list of names
 *
 * \param[in] in        List of names.
 * \param[in] s         Name to look for.
 *
 * \return the index of matching entry, or -1.
 */

static int matchname(struct idname *in, const char *s)
{
    int i;

    for (i = 0; in->name; i++, in++)
        if (!strcmp(in->name, s))
            return i;

    return -1;
}


void exa_cltrace::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('c') != opt_args.end())
    {
        component_index = matchname(cnames, opt_args.find('c')->second.c_str());
        if (component_index == -1)
            exa_cli_error("%sERROR%s: Invalid component name '%s'.\n",
                          COLOR_ERROR, COLOR_NORM, opt_args.find(
                              'c')->second.c_str());
    }

    if (opt_args.find('e') != opt_args.end())
    {
        level_index = matchname(lnames, opt_args.find('e')->second.c_str());
        if (level_index == -1)
            exa_cli_error("%sERROR%s: Invalid level '%s'.\n",
                          COLOR_ERROR, COLOR_NORM, opt_args.find(
                              'e')->second.c_str());
    }

    if (opt_args.find('n') != opt_args.end())
        nodelist = exa_expand(opt_args.find('n')->second);

    if (opt_args.find('a') != opt_args.end())
        allnodes = true;

    if (opt_args.find('l') != opt_args.end())
    {
        _display_options(0);
        exa::Exception ex("", EXA_SUCCESS);
        throw ex;
    }
    if (opt_args.find('L') != opt_args.end())
    {
        _display_options(1);
        exa::Exception ex("", EXA_SUCCESS);
        throw ex;
    }
}


void exa_cltrace::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Configure the level of logging of Exanodes on nodes.";
}


void exa_cltrace::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Log messages up to level " << OPT_ARG_LEVEL_LEVEL <<
    " for component "
        << OPT_ARG_COMPONENT_COMPONENT << " on nodes " <<
    OPT_ARG_NODE_HOSTNAMES << " in cluster "
                           << ARG_CLUSTERNAME << "." << std::endl;

    out << OPT_ARG_NODE_HOSTNAMES <<
    " is a regular expansion (see exa_expand)." << std::endl;
    out << "Use --list option to get the list of components and levels." <<
    std::endl;
}


void exa_cltrace::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Log debug (and info) messages for the " << Boldify("csupd")
        << " component on all the nodes of the cluster " << Boldify("mycluster")
        << ":" << std::endl;
    out << "  " <<
    "exa_cltrace --all --component csupd --level debug mycluster" << std::endl;
    out << std::endl;

    out << "Log only informational messages for all components on nodes " <<
    Boldify("node10")
        << " and " << Boldify("node11") << " of the cluster " << Boldify(
        "mycluster")
        << ":" << std::endl;
    out << "  " <<
    "exa_cltrace --node node/10-11/ --component all --level info mycluster"
        << std::endl;
    out << std::endl;
}


