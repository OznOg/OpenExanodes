/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clstop.h"

#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "boost/format.hpp"

using boost::format;

using std::string;

/* FIXME calling the constructor of exa_clnodestop means that we'll get */
/* duplicate options, and irrelevant "see also"... */
exa_clstop::exa_clstop() : exa_clnodestop()
{
    all_nodes = true;
}


void exa_clstop::init_options()
{
    exa_clnodestop::init_options();
}


void exa_clstop::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
    add_see_also("exa_clstart");
    add_see_also("exa_clinfo");
    add_see_also("exa_clstats");
    add_see_also("exa_cltune");
    add_see_also("exa_clreconnect");
}


void exa_clstop::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('r') != opt_args.end())
        recursive = true;
    if (opt_args.find('f') != opt_args.end())
        force = true;
}


void exa_clstop::dump_short_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Stop an Exanodes cluster.";
}


void exa_clstop::dump_full_description(std::ostream &out,
                                       bool show_hidden) const
{
    out << "Stop the Exanodes cluster " << ARG_CLUSTERNAME <<
    ". All the disk groups"
#ifdef WITH_FS
        << ", volumes and file systems"
#else
        << " and volumes"
#endif
        << " are also stopped but they keep their state"
        <<
    " property for the next exa_clstart. It means that if one or more disk "
#ifdef WITH_FS
        << "groups, volumes or file systems"
#else
        << "groups or volumes"
#endif
        << " are started, an exa_clstop followed "
        << "by an exa_clstart will bring them back in the started state." <<
    std::endl
        << std::endl;

    out <<
    "The --recursive option can also be used to set the stopped state on all disk groups"
#ifdef WITH_FS
        << ", volumes and file systems. "
#else
        << " and volumes. "
#endif
        << "In this case, the next exa_clstart will start the "
        << "Exanodes cluster and nothing more." << std::endl;
}


void exa_clstop::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "Stop the Exanodes cluster " << Boldify("mycluster") <<
    ". If they are started disk groups"
#ifdef WITH_FS
        << ", volumes or file systems"
#else
        << " or volumes"
#endif
        << ", they will be bring back to the same state at "
        << "the next exa_clstart:" << std::endl;
    out << "  " << "exa_clstop mycluster" << std::endl;
    out << std::endl;
}


