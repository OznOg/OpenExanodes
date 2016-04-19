/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/cli/src/exa_clstats.h"

#include <assert.h>
#include <algorithm>
#include <math.h>
#include <limits.h>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"
#include "ui/common/include/admindcommand.h"
#include "ui/common/include/admindmessage.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/common_utils.h"
#include "ui/common/include/exa_expand.h"

#include <vector>
#include <map>

#ifdef WIN32
#include <float.h>
#include <ymath.h>

#define isnan _isnan
#define NAN _Nan._Double
#endif

using std::shared_ptr;
using std::string;
using std::set;
using std::vector;
using std::map;
using std::list;

/** A bitmask representing each different types of output report
 *  we support
 */
typedef enum display_style_t
{
    STAT_GLOBAL = 1,
    STAT_VOLUME = 1 << 1,
    STAT_NDEV = 1 << 2,
    STAT_VOL_ERR = 1 << 3,
    STAT_NDEV_ERR = 1 << 4,
    STAT_SEQ_VOL = 1 << 5,
    STAT_SEQ_DISK = 1 << 6,
    STAT_MATRIX = 1 << 7,
    STAT_DISPLAY_ALL = 1 << 8,
    STAT_FORCE_KILO = 1 << 9,
} display_style_t;

const std::string exa_clstats::OPT_ARG_WRAPPING_N(Command::Boldify("N"));

exa_clstats::exa_clstats(int argc, char *argv[])
    : exa_clcommand(argc, argv)
    , reset(false)
    , display_style(STAT_GLOBAL | STAT_VOLUME | STAT_NDEV | STAT_VOL_ERR |
                    STAT_NDEV_ERR)
    , wrapping_in_nodes(EXA_CLI_MAX_NODE_PER_LINE)
{}


void exa_clstats::init_options()
{
    exa_clcommand::init_options();

    add_option('r', "reset", "Reset the statistics counters. The next call to "
               "exa_clstats will show statistics from that time.", 0, false,
               false);
    add_option('q', "seq", "Also display sequence measurements.", 0, false,
               false);
    add_option('s', "summarize", "Only display a summary of the measurements.",
               0, false, false);
    add_option('k', "kilo", "Display all statistics in KiB.", 0, false, false);
    add_option('m', "matrix", "Display a matrix with values for each pair of "
               "client/server.", 0, false, false);
    add_option('a', "display-all", "Display all data, including null values.",
               0, false, false);
    add_option('w', "wrapping", "Line wrapping. N is the max number of node "
               "to display per line.", 0, false, true, OPT_ARG_WRAPPING_N);
}


void exa_clstats::init_see_alsos()
{
    add_see_also("exa_clcreate");
    add_see_also("exa_cldelete");
    add_see_also("exa_clstart");
    add_see_also("exa_clstop");
    add_see_also("exa_clinfo");
    add_see_also("exa_cltune");
    add_see_also("exa_clreconnect");
}


#define H0 "%24s  "
#define H1 "%26s"
#define NODE_COLUMN_WIDTH 11

/** Some global functions for this file
 *
 */
static void xmlpropget(xmlNodePtr node, const char *prop,
                       uint64_t &num)
{
    xmlChar *str(xmlGetProp(node, BAD_CAST prop));

    if (str)
    {
        num = strtoull(reinterpret_cast<char *>(str), NULL, 0);
        xmlFree(str);
    }
    else
        num = 0;
}


static void xmlpropget(xmlNodePtr node, const char *prop,
                       string &qstr)
{
    xmlChar *str(xmlGetProp(node, BAD_CAST prop));

    if (str)
    {
        qstr = string(reinterpret_cast<char *>(str));
        xmlFree(str);
    }
}


/*--------------------------------------------------------------------------*/
/** \brief Helper function : Displaying a throughput humanly-readable
 *         if it is not null, and different from "-1".
 *
 * \param[in]   size: the throughput to display
 * \param[in]   force_kilo:
 * \return String representing the value if valid : value+unit or N/A
 *         if invalid.
 */
/*--------------------------------------------------------------------------*/
static string display_throughput(double throughput, bool force_kilo)
{
    string mant, unit;

    format_hum_friend_iB((uint64_t) throughput, mant, unit, force_kilo);

    /* Do not display empty value */
    if (mant == "0.00")
        return "";

    return mant + " " + unit + "/s";
}


static double tofreq(uint64_t samples, uint64_t msec)
{
    double rv = samples * 1000;

    /* admind may returns zeroed empty values, we want to consider them as
     * 0, not NAN values */
    if (msec == 0)
        return 0;

    rv = rv / msec;

    return rv;
}


/** GlobalStats handle a summary of the stats
 *
 */
class GlobalStats
{
public:

    struct exa_expand_stat_lt
    {
        bool operator ()(const string &c1, const string &c2) const
        {
            return os_strverscmp(c1.c_str(), c2.c_str()) < 0;
        }
    };

    uint64_t elapsed;

    double read_thr;
    double write_thr;
    double read_ios;
    double write_ios;

    uint wrapping_in_nodes;
    set<string, exa_expand_stat_lt> nodeList;

    typedef vector<double> StatList;
    typedef map<string, StatList> StatMapItem;
    typedef map<string,  StatMapItem> StatMap;
    StatMap volStatRWMap;
    StatMap volStatIOMap;
    StatMap volStatErrorMap;
    StatMap ndevStatRWMap;
    StatMap ndevStatIOMap;
    StatMap ndevStatErrorMap;

    int display_style;

    int output_warning;
    int output_summary;

    GlobalStats(void) :
        elapsed(0),
        read_thr(0),
        write_thr(0),
        read_ios(0),
        write_ios(0),
        wrapping_in_nodes(0),
        display_style(0),
        output_warning(0),
        output_summary(0)
    {}


    /** Call me for each node */
    void nodeAdd(string node)
    {
        nodeList.insert(node);
    }


    /** Add a throughput stat for the given volume */
    void volStatAdd(string group, string volume,
                    double throughput_read,
                    double throughput_write,
                    double ios_read,
                    double ios_write,
                    uint64_t req_error)
    {
        if (isnan(throughput_read))
            volStatRWMap[group][volume].push_back(throughput_read);
        else
            volStatRWMap[group][volume].push_back(
                throughput_read + throughput_write);
        volStatErrorMap[group][volume].push_back(req_error);
        volStatIOMap[group][volume].push_back(ios_read + ios_write);
    }


    /** Add a throughput stat for the given ndev */
    void ndevStatAdd(string node, string ndev,
                     double throughput_read,
                     double throughput_write,
                     double ios_read,
                     double ios_write,
                     uint64_t req_error)
    {
        if (isnan(throughput_read))
            ndevStatRWMap[node][ndev].push_back(throughput_read);
        else
            ndevStatRWMap[node][ndev].push_back(
                throughput_read + throughput_write);
        ndevStatIOMap[node][ndev].push_back(ios_read + ios_write);
        ndevStatErrorMap[node][ndev].push_back(req_error);
    }


    /** Call this once all data are gathered and before calling any
     * output*() methods
     *
     * \param[in]   display_style: carries all information regarding how to lay out data
     * \param[in]   wrapping_in_nodes: Max number of node per line.
     *              set to 0 to disable wrapping
     */
    void initOutput(int _display_style, uint _wrapping_in_nodes)
    {
        display_style = _display_style;
        wrapping_in_nodes = _wrapping_in_nodes;

        if (wrapping_in_nodes == 0)
            wrapping_in_nodes = nodeList.size();
    }


    /** Display elapsed time since last stats reset
     */
    void outputElapsed()
    {
        const uint64_t MSEC_PER_SEC  = 1000;
        const uint64_t MSEC_PER_MIN  = 60 * MSEC_PER_SEC;
        const uint64_t MSEC_PER_HOUR = 60 * MSEC_PER_MIN;
        const uint64_t MSEC_PER_DAY  = 24 * MSEC_PER_HOUR;

        if (elapsed == 0)
            printf(H1 "    N/A\n", "Elapsed time");
        else if (elapsed < MSEC_PER_SEC)
            printf(H1 "    %" PRIu64 " ms\n", "Elapsed time", elapsed);
        else if (elapsed < MSEC_PER_MIN)
            printf(H1 "    %" PRIu64 " s\n",
                   "Elapsed time",
                   elapsed / MSEC_PER_SEC);
        else if (elapsed < MSEC_PER_HOUR)
            printf(H1 "    %" PRIu64 " min %" PRIu64 " s\n",
                   "Elapsed time",
                   elapsed / MSEC_PER_MIN,
                   (elapsed % MSEC_PER_MIN) / MSEC_PER_SEC);
        else if (elapsed < MSEC_PER_DAY)
            printf(H1 "    %" PRIu64 " h %" PRIu64 " min\n",
                   "Elapsed time",
                   elapsed / MSEC_PER_HOUR,
                   (elapsed % MSEC_PER_HOUR) / MSEC_PER_MIN);
        else
            printf(H1 "    %" PRIu64 " days %" PRIu64 " h\n",
                   "Elapsed time",
                   elapsed / MSEC_PER_DAY,
                   (elapsed % MSEC_PER_DAY) / MSEC_PER_HOUR);

        printf("\n");
    }


    /** Display the statistic summary
     */
    void outputSummary()
    {
        printf(H1 "\n", "Volume summary");
        if (output_summary <= 0)
        {
            printf(H1 "    N/A\n", "Read/Write");
            printf(H1 "    N/A\n", "Read");
            printf(H1 "    N/A\n", "Write");

            output_warning = 1;
        }
        else
        {
            printf(H1 " %*s %*.0f IO/s\n", "Read/Write",
                   NODE_COLUMN_WIDTH,
                   display_throughput(write_thr + read_thr,
                                      (display_style & STAT_FORCE_KILO)).c_str(),
                   NODE_COLUMN_WIDTH - 2,
                   write_ios + read_ios);
            printf(H1 " %*s %*.0f IO/s\n", "Read",
                   NODE_COLUMN_WIDTH,
                   display_throughput(read_thr,
                                      (display_style & STAT_FORCE_KILO)).c_str(),
                   NODE_COLUMN_WIDTH - 2,
                   read_ios);
            printf(H1 " %*s %*.0f IO/s\n", "Write",
                   NODE_COLUMN_WIDTH,
                   display_throughput(write_thr,
                                      (display_style & STAT_FORCE_KILO)).c_str(),
                   NODE_COLUMN_WIDTH - 2,
                   write_ios);
        }

        printf("\n");
    }


    /** Display the list of nodes
     *
     * \return the next node to display or 0 if done.
     */
    uint outputNodes(uint current_node_index)
    {
        uint i = 0;

        if (current_node_index >= nodeList.size())
            return 0;

        if (current_node_index == 0)
            printf(" %*s", NODE_COLUMN_WIDTH, "Total");

        set<string, exa_expand_stat_lt>::iterator it = nodeList.begin();
        while (current_node_index > i)
        {
            ++it;
            i++;
        }

        for (;
             it != nodeList.end() && i < current_node_index + wrapping_in_nodes;
             ++it)
        {
            string node = *it;
            i++;
            if (display_style & STAT_MATRIX)
                printf(" %*s", NODE_COLUMN_WIDTH, node.c_str());
        }

        return i;
    }


    /* Utility function for outputStat. It will display each item of the given group
     *  if there are values to be displayed.
     */
    uint outputStatItems(string group,
                         StatMapItem mapitem,
                         uint current_node,
                         bool is_throughput)
    {
        uint number_of_lines = 0;

        /* Display values items */
        for (StatMapItem::iterator it = mapitem.begin();
             it != mapitem.end();
             ++it)
        {
            uint i;
            bool item_displayed = true;

            /* Strip empty lines */
            if (!(display_style & STAT_DISPLAY_ALL))
            {
                if (current_node == 0)
                {
                    item_displayed = false;
                    for (i = 0; i < nodeList.size(); i++)
                        if ((int) it->second[i])
                        {
                            item_displayed = true;
                            break;
                        }
                }
                else
                {
                    item_displayed = false;
                    for (i = current_node;
                         (i < current_node + wrapping_in_nodes &&
                          i < nodeList.size());
                         i++)
                        if (((int) it->second[i]))
                        {
                            item_displayed = true;
                            break;
                        }
                }
            }

            if (!item_displayed)
                continue;

            printf(H1, string(group + ":" + it->first).c_str());

            for (i = current_node;
                 (i < current_node + wrapping_in_nodes &&
                  i < nodeList.size());
                 i++)
            {
                if (i == 0)
                {
                    /* calculate the total and display it */
                    double total = 0.0;
                    uint n;
                    for (n = 0; n < nodeList.size(); n++)
                        total += it->second[n];

                    if (!(display_style & STAT_DISPLAY_ALL) && (int) total == 0)
                        printf(" %*s", NODE_COLUMN_WIDTH, "");
                    else
                    {
                        if (is_throughput)
                            printf(" %*s", NODE_COLUMN_WIDTH,
                                   display_throughput(total,
                                                      (display_style &
                                                       STAT_FORCE_KILO)).c_str());
                        else
                            printf(" %*d", NODE_COLUMN_WIDTH, (int) total);
                    }

                    number_of_lines++;
                }

                /* Do not display 0 values */
                if (!(display_style & STAT_DISPLAY_ALL) &&
                    (int) it->second[i] == 0)
                {
                    printf(" %*s", NODE_COLUMN_WIDTH, "");
                    continue;
                }

                if ((display_style & STAT_MATRIX))
                {
                    if (is_throughput)
                        printf(" %*s", NODE_COLUMN_WIDTH,
                               display_throughput(it->second[i],
                                                  (display_style &
                                                   STAT_FORCE_KILO)).c_str());
                    else
                        printf(" %*d", NODE_COLUMN_WIDTH, (int) it->second[i]);
                }

                number_of_lines++;
            }
            printf("\n");
        }

        return number_of_lines;
    }


    void outputStat(StatMap &statMap, string title, string unit,
                    bool is_throughput = true)
    {
        StatMap::iterator itg;
        StatMapItem::iterator it;
        uint current_node = 0;
        uint next_node = 0;
        uint number_of_lines;
        bool got_one = false;

        printf(H1, title.c_str());

        /* Skip if empty */
        if (!(display_style & STAT_DISPLAY_ALL))
        {
            for (itg = statMap.begin(); itg != statMap.end(); ++itg)
                for (it = itg->second.begin(); it != itg->second.end(); ++it)
                    /* Strip empty lines */
                    for (StatList::iterator it2 = it->second.begin();
                         it2 != it->second.end();
                         ++it2)
                        if (*it2 != 0)
                        {
                            got_one = true;
                            break;
                        }

            if (!got_one)
            {
                printf(" <None>\n\n");
                return;
            }
        }

        /* Loop over each nodes and display data */
        while ((next_node = outputNodes(current_node)))
        {
            number_of_lines = 0;

            printf(" %s\n", unit.c_str());

            for (itg = statMap.begin(); itg != statMap.end(); ++itg)
                number_of_lines += outputStatItems(itg->first, itg->second,
                                                   current_node, is_throughput);

            if (number_of_lines == 0)
                printf(H1 "\n", "<None>");

            printf("\n");
            printf(H1, "");

            current_node = next_node;

            if (!(display_style & STAT_MATRIX))
                break;
        }
        printf("\n");
    }


    /* Utility function for outputStat2 (2 data set in colum).
     * It will display each item of the given group
     * if there are values to be displayed.
     */
    uint outputStat2Items(string group,
                          StatMapItem mapitem1,
                          StatMapItem mapitem2,
                          bool is_throughput1,
                          bool is_throughput2)
    {
        uint number_of_lines = 0;

        /* Display values items */
        for (StatMapItem::iterator it = mapitem1.begin();
             it != mapitem1.end();
             ++it)
        {
            /* calculate the total and display it */
            uint n;
            double total1 = 0.0;
            for (n = 0; n < nodeList.size(); n++)
            {
                /* if volume started after stats reset, set the total to NAN */
                if (isnan(it->second[n]))
                {
                    total1 = NAN;
                    continue;
                }

                total1 += it->second[n];
            }

            /* if volume started after stats reset, display N/A */
            if (isnan(total1))
            {
                printf(H1, string(group + ":" + it->first).c_str());

                printf("    N/A \n");
                output_warning = 1;
                continue;
            }

            double total2 = 0.0;
            for (n = 0; n < nodeList.size(); n++)
                total2 += mapitem2[it->first][n];

            if (!(display_style & STAT_DISPLAY_ALL) && total1 == 0 && total2 ==
                0)
                continue;

            printf(H1, string(group + ":" + it->first).c_str());

            if (!(display_style & STAT_DISPLAY_ALL) && (int) total1 == 0)
                printf(" %*s", NODE_COLUMN_WIDTH, "");
            else
            {
                if (is_throughput1)
                    printf(" %*s", NODE_COLUMN_WIDTH,
                           display_throughput(total1,
                                              (display_style &
                                               STAT_FORCE_KILO)).c_str());
                else
                    printf(" %*d", NODE_COLUMN_WIDTH, (int) total1);
            }

            if (!(display_style & STAT_DISPLAY_ALL) && (int) total2 == 0)
                printf(" %*s", NODE_COLUMN_WIDTH, "");
            else
            {
                if (is_throughput2)
                    printf(" %*s", NODE_COLUMN_WIDTH,
                           display_throughput(total2,
                                              (display_style &
                                               STAT_FORCE_KILO)).c_str());
                else
                    printf(" %*d", NODE_COLUMN_WIDTH, (int) total2);
            }
            number_of_lines++;

            printf("\n");
        }

        return number_of_lines;
    }


    /* same as outputStat but for 2 differents StatMap in two column.
     *  only the total is displayed for each column.
     */
    void output2Stat(StatMap &statMap1, StatMap &statMap2,
                     string title1, string title2,
                     bool is_throughput1, bool is_throughput2)
    {
        StatMap::iterator itg;
        StatMapItem::iterator it;
        bool got_one = false;

        printf(H1, "");

        /* Skip if empty */
        if (!(display_style & STAT_DISPLAY_ALL))
        {
            for (itg = statMap1.begin(); itg != statMap1.end(); ++itg)
                for (it = itg->second.begin(); it != itg->second.end(); ++it)
                    /* Strip empty lines */
                    for (StatList::iterator it2 = it->second.begin();
                         it2 != it->second.end();
                         ++it2)
                        if (*it2 != 0)
                        {
                            got_one = true;
                            break;
                        }

            if (!got_one)
            {
                printf(" <None>\n\n");
                return;
            }
        }

        printf(" %*s %*s\n",
               NODE_COLUMN_WIDTH, title1.c_str(),
               NODE_COLUMN_WIDTH, title2.c_str());

        for (itg = statMap1.begin(); itg != statMap1.end(); ++itg)
            outputStat2Items(itg->first, itg->second, statMap2[itg->first],
                             is_throughput1, is_throughput2);

        printf("\n");
    }


    void outputVolPerfStat()
    {
        printf(H0 "\n", "Volume Stats");

        if ((display_style & STAT_MATRIX))
        {
            outputStat(volStatRWMap, "Throughput", "",
                       true);

            outputStat(volStatIOMap, "IO/s", "",
                       /* Not throughput */ false);
        }
        else
            output2Stat(volStatRWMap, volStatIOMap,
                        "Throughput", "IO/s",
                        true, /* Not throughput */ false);
    }


    void outputNDevPerfStat()
    {
        printf(H0 "\n", "Disk Stats");

        if ((display_style & STAT_MATRIX))
        {
            outputStat(ndevStatRWMap, "Throughput", "",
                       true);

            outputStat(ndevStatIOMap, "IO/s", "",
                       /* Not throughput */ false);
        }
        else
            output2Stat(ndevStatRWMap, ndevStatIOMap,
                        "Throughput", "IO/s",
                        true, /* Not throughput */ false);
    }


    void outputVolErrorStat()
    {
        outputStat(volStatErrorMap, "Volume Errors", "(Number of errors)",
                   /* Not throughput */ false);
    }


    void outputNdevErrorStat()
    {
        outputStat(ndevStatErrorMap, "Disk Errors", "(Number of errors)",
                   /* Not throughput */ false);
    }
};

class Stats;

class VolumeStats : private boost::noncopyable
{
    string groupname;
    uint64_t msec;
    string name;
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_req_err;
    uint64_t seq_sect_read;
    uint64_t seq_sect_write;
    uint64_t seq_req_read;
    uint64_t seq_req_write;
    uint64_t seq_seeks_read;
    uint64_t seq_seeks_write;
    uint64_t seq_seek_dist_read;
    uint64_t seq_seek_dist_write;

public:

    const string &key;

    VolumeStats(xmlNodePtr node, string &_groupname, GlobalStats &global_stats);
    void parse(xmlNodePtr node, string &_groupname, GlobalStats &global_stats);
    void output(GlobalStats &global_stats);
    void output_seq(const Stats &stats);
};

class DiskGroupStats : private boost::noncopyable
{
public:

    struct exa_expand_stat_lt
    {
        bool operator ()(string const &c1, string const &c2) const
        {
            return os_strverscmp(c1.c_str(), c2.c_str()) < 0;
        }
    };

private:

    typedef map<string, VolumeStats *, exa_expand_stat_lt> VolumesMap;
    string name;
    VolumesMap volumes;

public:

    DiskGroupStats(xmlNodePtr node, GlobalStats &global_stats);
    ~DiskGroupStats()
    {
        while (!volumes.empty())
        {
            delete volumes.begin()->second;
            volumes.erase(volumes.begin());
        }
    }


    void parse(xmlNodePtr node, GlobalStats &global_stats);
    void output(GlobalStats &global_stats);
    void output_seq(const Stats &stats);
};

class VrtStats : private boost::noncopyable
{
    struct exa_expand_stat_lt
    {
        bool operator ()(string const &c1, string const &c2) const
        {
            return os_strverscmp(c1.c_str(), c2.c_str()) < 0;
        }
    };
    typedef map<string, DiskGroupStats *, exa_expand_stat_lt> DiskGroupsMap;
    DiskGroupsMap groups;

public:

    VrtStats(xmlNodePtr node, GlobalStats &global_stats);
    ~VrtStats()
    {
        while (!groups.empty())
        {
            delete groups.begin()->second;
            groups.erase(groups.begin());
        }
    }


    void parse(xmlNodePtr node, GlobalStats &global_stats);
    void output(GlobalStats &global_stats);
    void output_seq(const Stats &stats);
};

class NdevStats : private boost::noncopyable
{
    uint64_t msec;
    string nodename;
    string disk;
    uint64_t nb_sect_read;
    uint64_t nb_sect_write;
    uint64_t nb_req_read;
    uint64_t nb_req_write;
    uint64_t nb_req_err;
    uint64_t seq_sect_read;
    uint64_t seq_sect_write;
    uint64_t seq_req_read;
    uint64_t seq_req_write;
    uint64_t seq_seeks_read;
    uint64_t seq_seeks_write;
    uint64_t seq_seek_dist_read;
    uint64_t seq_seek_dist_write;

public:

    NdevStats(xmlNodePtr node);
    void output(GlobalStats &global_stats);
    void output_seq(const Stats &stats);
};

class NbdStats : private boost::noncopyable
{
    list<NdevStats *> ndevs;

public:

    NbdStats(xmlNodePtr node);
    ~NbdStats()
    {
        while (!ndevs.empty())
        {
            delete *ndevs.begin();
            ndevs.erase(ndevs.begin());
        }
    }


    void parse(xmlNodePtr node);
    void output(GlobalStats &global_stats);
    void output_seq(const Stats &stats);
};

class NodeStats : private boost::noncopyable
{
    string name;
    VrtStats *vrt;
    NbdStats *nbd;

public:

    const string &key;

    NodeStats(xmlNodePtr node, GlobalStats &global_stats);
    void parse(xmlNodePtr node, GlobalStats &global_stats);
    void output(GlobalStats &global_stats);
    void output_seq_volume(const Stats &stats);
    void output_seq_ndev(const Stats &stats);
    ~NodeStats();
};

/*
 * I would prefer all of this to be part of the exa_clstats class
 * itself, but it would force me to expose the "nodes" member. I
 * should use the Pimpl idiom instead, it would be far cleaner. See
 * bug #1135.
 */
class Stats : private boost::noncopyable
{
    struct exa_expand_stat_lt
    {
        bool operator ()(string const &c1, string const &c2) const
        {
            return os_strverscmp(c1.c_str(), c2.c_str()) < 0;
        }
    };

    typedef map<string, NodeStats *, exa_expand_stat_lt> NodesMap;
    NodesMap nodes;
    GlobalStats global_stats;
    const int &display_style;
    const uint &wrapping_in_nodes;

public:

    Stats(const int &_display_style, uint &_wrapping_in_nodes);
    ~Stats()
    {
        while (!nodes.empty())
        {
            delete nodes.begin()->second;
            nodes.erase(nodes.begin());
        }
    }


    void parse_xml(xmlNodePtr node);
    void output(int display_style);
    void output_seq_volume();
    void output_seq_ndev();
    void display_seq(const string &label,
                     uint64_t seq_read,
                     uint64_t seq_write,
                     uint64_t req_read,
                     uint64_t req_write,
                     uint64_t sect_read,
                     uint64_t sect_write,
                     uint64_t seek_dist_read,
                     uint64_t seek_dist_write) const;
    void display_seq_detail(bool write, uint64_t reqs, uint64_t bytes,
                            uint64_t seqs, uint64_t seek_dist) const;
    const string display_amount(uint64_t bytes) const;

    bool reset;
};

VolumeStats::VolumeStats(xmlNodePtr node, string &_groupname,
                         GlobalStats &global_stats) :
    groupname(_groupname),
    key(name)
{
    parse(node, _groupname, global_stats);
}


void VolumeStats::parse(xmlNodePtr node, string &_groupname,
                        GlobalStats &global_stats)
{
    xmlpropget(node, "msec", msec);
    xmlpropget(node, "name", name);
    xmlpropget(node, "sect_read", nb_sect_read);
    xmlpropget(node, "sect_write", nb_sect_write);
    xmlpropget(node, "req_read", nb_req_read);
    xmlpropget(node, "req_write", nb_req_write);
    xmlpropget(node, "req_error", nb_req_err);
    xmlpropget(node, "seq_sect_read", seq_sect_read);
    xmlpropget(node, "seq_sect_write", seq_sect_write);
    xmlpropget(node, "seq_req_read", seq_req_read);
    xmlpropget(node, "seq_req_write", seq_req_write);
    xmlpropget(node, "seq_seeks_read", seq_seeks_read);
    xmlpropget(node, "seq_seeks_write", seq_seeks_write);
    xmlpropget(node, "seq_seek_dist_read", seq_seek_dist_read);
    xmlpropget(node, "seq_seek_dist_write", seq_seek_dist_write);

    if (msec == 0)
        global_stats.output_summary = -1;
    else
    {
        if (global_stats.output_summary == 0)
            global_stats.output_summary = 1;
        global_stats.read_thr += tofreq(nb_sect_read * SECTOR_SIZE, msec);
        global_stats.write_thr += tofreq(nb_sect_write * SECTOR_SIZE, msec);
        global_stats.write_ios += tofreq(nb_req_write, msec);
        global_stats.read_ios += tofreq(nb_req_read, msec);
    }
}


void VolumeStats::output(GlobalStats &global_stats)
{
    if (msec == 0)
        global_stats.volStatAdd(groupname, name,
                                NAN,
                                tofreq(nb_sect_write * SECTOR_SIZE, msec),
                                tofreq(nb_req_read, msec),
                                tofreq(nb_req_write, msec),
                                nb_req_err);
    else
        global_stats.volStatAdd(groupname, name,
                                tofreq(nb_sect_read * SECTOR_SIZE, msec),
                                tofreq(nb_sect_write * SECTOR_SIZE, msec),
                                tofreq(nb_req_read, msec),
                                tofreq(nb_req_write, msec),
                                nb_req_err);
}


void VolumeStats::output_seq(const Stats &stats)
{
    /*
     * Why add one? Because the seeks counters keep track of the number
     * of seeks *between sequences*, while we want the number of
     * *sequences proper*. In other words, we know how many sections
     * there are to the fence, but we're interested in the number of
     * fenceposts.
     */
    stats.display_seq(groupname + ":" + name,
                      seq_seeks_read + 1, seq_seeks_write + 1,
                      seq_req_read, seq_req_write,
                      seq_sect_read, seq_sect_write,
                      seq_seek_dist_read, seq_seek_dist_write);
}


DiskGroupStats::DiskGroupStats(xmlNodePtr node, GlobalStats &global_stats)
{
    parse(node, global_stats);
}


void DiskGroupStats::parse(xmlNodePtr node, GlobalStats &global_stats)
{
    xmlNodePtr i;

    xmlpropget(node, "name", name);

    for (i = node->children; i; i = i->next)
        if (xmlStrcmp(i->name, BAD_CAST EXA_CONF_LOGICAL_VOLUME) == 0)
        {
            xmlChar *_name = xmlGetProp(i, BAD_CAST "name");
            string vol_name(reinterpret_cast<const char *>(_name));
            xmlFree(_name);

            if (volumes[vol_name])
                volumes[vol_name]->parse(i, name, global_stats);
            else
                volumes[vol_name] = new VolumeStats(i, name, global_stats);
        }
}


void DiskGroupStats::output(GlobalStats &global_stats)
{
    for (VolumesMap::iterator it = volumes.begin();
         it != volumes.end(); ++it)
    {
        VolumeStats &vol = *it->second;
        vol.output(global_stats);
    }
}


void DiskGroupStats::output_seq(const Stats &stats)
{
    for (VolumesMap::iterator it = volumes.begin();
         it != volumes.end(); ++it)
    {
        VolumeStats &vol = *it->second;
        vol.output_seq(stats);
    }
}


VrtStats::VrtStats(xmlNodePtr node, GlobalStats &global_stats)
{
    parse(node, global_stats);
}


void VrtStats::parse(xmlNodePtr node, GlobalStats &global_stats)
{
    xmlNodePtr i;

    for (i = node->children; i; i = i->next)
        if (xmlStrcmp(i->name, BAD_CAST EXA_CONF_GROUP) == 0)
        {
            xmlChar *_name = xmlGetProp(i, BAD_CAST "name");
            string group_name(reinterpret_cast<const char *>(_name));
            xmlFree(_name);

            if (groups[group_name])
                groups[group_name]->parse(i, global_stats);
            else
                groups[group_name] = new DiskGroupStats(i, global_stats);
        }

}


void VrtStats::output(GlobalStats &global_stats)
{
    for (DiskGroupsMap::iterator it = groups.begin();
         it != groups.end(); ++it)
    {
        DiskGroupStats &dgs = *it->second;
        dgs.output(global_stats);
    }
}


void VrtStats::output_seq(const Stats &stats)
{
    for (DiskGroupsMap::iterator it = groups.begin();
         it != groups.end(); ++it)
    {
        DiskGroupStats &dgs = *it->second;
        dgs.output_seq(stats);
    }
}


NdevStats::NdevStats(xmlNodePtr node)
{
    xmlpropget(node, "msec", msec);
    xmlpropget(node, "node", nodename);
    xmlpropget(node, "disk", disk);
    xmlpropget(node, "sect_read", nb_sect_read);
    xmlpropget(node, "sect_write", nb_sect_write);
    xmlpropget(node, "req_read", nb_req_read);
    xmlpropget(node, "req_write", nb_req_write);
    xmlpropget(node, "req_error", nb_req_err);
    xmlpropget(node, "seq_sect_read", seq_sect_read);
    xmlpropget(node, "seq_sect_write", seq_sect_write);
    xmlpropget(node, "seq_req_read", seq_req_read);
    xmlpropget(node, "seq_req_write", seq_req_write);
    xmlpropget(node, "seq_seeks_read", seq_seeks_read);
    xmlpropget(node, "seq_seeks_write", seq_seeks_write);
    xmlpropget(node, "seq_seek_dist_read", seq_seek_dist_read);
    xmlpropget(node, "seq_seek_dist_write", seq_seek_dist_write);
}


void NdevStats::output(GlobalStats &global_stats)
{
    if (msec == 0)
        global_stats.ndevStatAdd(nodename, disk,
                                 NAN,
                                 tofreq(nb_sect_write * SECTOR_SIZE, msec),
                                 tofreq(nb_req_read, msec),
                                 tofreq(nb_req_write, msec),
                                 nb_req_err);
    else
    {
        global_stats.ndevStatAdd(nodename, disk,
                                 tofreq(nb_sect_read * SECTOR_SIZE, msec),
                                 tofreq(nb_sect_write * SECTOR_SIZE, msec),
                                 tofreq(nb_req_read, msec),
                                 tofreq(nb_req_write, msec),
                                 nb_req_err);
        global_stats.elapsed = msec;
    }
}


void NdevStats::output_seq(const Stats &stats)
{
    /*
     * Why add one? Because the seeks counters keep track of the number
     * of seeks *between sequences*, while we want the number of
     * *sequences proper*. In other words, we know how many sections
     * there are to the fence, but we're interested in the number of
     * fenceposts.
     */
    stats.display_seq(nodename + ":" + disk,
                      seq_seeks_read + 1, seq_seeks_write + 1,
                      seq_req_read, seq_req_write,
                      seq_sect_read, seq_sect_write,
                      seq_seek_dist_read, seq_seek_dist_write);
}


NbdStats::NbdStats(xmlNodePtr node)
{
    parse(node);
}


void NbdStats::parse(xmlNodePtr node)
{
    xmlNodePtr i;

    for (i = node->children; i; i = i->next)
        /*
         * The ndevs are already in order of minor number on their local
         * node, which at the moment seems to make sense and is
         * consistent on every nodes. If those assumptions should prove
         * to be incorrect, "ndevs" should be use an os_strverscmp() sorted
         * set, much like NodeStatsList does.
         */
        if (xmlStrcmp(i->name, BAD_CAST "ndev") == 0)
            ndevs.push_back(new NdevStats(i));
}


void NbdStats::output(GlobalStats &global_stats)
{
    for (list<NdevStats *>::iterator it = ndevs.begin();
         it != ndevs.end(); ++it)
    {
        NdevStats *ndev = *it;
        ndev->output(global_stats);
    }
}


void NbdStats::output_seq(const Stats &stats)
{
    for (list<NdevStats *>::iterator it = ndevs.begin();
         it != ndevs.end(); ++it)
    {
        NdevStats *ndev = *it;
        ndev->output_seq(stats);
    }
}


NodeStats::NodeStats(xmlNodePtr node, GlobalStats &global_stats) :
    vrt(0),
    nbd(0),
    key(name)
{
    parse(node, global_stats);
}


void NodeStats::parse(xmlNodePtr node, GlobalStats &global_stats)
{
    xmlNodePtr i;

    xmlpropget(node, "name", name);

    global_stats.nodeAdd(name);

    for (i = node->children; i; i = i->next)
    {
        if (xmlStrcmp(i->name, BAD_CAST "vrt") == 0)
        {
            if (!vrt)
                vrt = new VrtStats(i, global_stats);
            else
                vrt->parse(i, global_stats);
        }

        if (xmlStrcmp(i->name, BAD_CAST "nbd") == 0)
        {
            if (!nbd)
                nbd = new NbdStats(i);
            else
                nbd->parse(i);
        }

        /*
         * FIXME: We should complain about unknown nodes? Or is silence
         * golden?
         *
         * "Be conservative in what you do, be liberal in what you
         * accept from others." -- Jon Postel, "Robustness Principle"
         * from RFC 793, September 1981.
         */
    }
}


void NodeStats::output(GlobalStats &global_stats)
{
    if (vrt)
        vrt->output(global_stats);

    if (nbd)
        nbd->output(global_stats);
}


void NodeStats::output_seq_volume(const Stats &stats)
{
    if (!vrt)
        return;

    printf("    %s\n", name.c_str());
    vrt->output_seq(stats);

    printf("\n");
}


void NodeStats::output_seq_ndev(const Stats &stats)
{
    if (!nbd)
        return;

    printf("    %s\n", name.c_str());
    nbd->output_seq(stats);

    printf("\n");
}


NodeStats::~NodeStats()
{
    delete nbd;
    delete vrt;
}


Stats::Stats(const int &_display_style, uint &_wrapping_in_nodes) :
    display_style(_display_style),
    wrapping_in_nodes(_wrapping_in_nodes)
{}


void Stats::parse_xml(xmlNodePtr node)
{
    for (; node; node = node->next)
    {
        NodeStats *nodstat;
        string name;
        xmlpropget(node, "name", name);
        NodesMap::iterator it = nodes.find(name);

        if (it != nodes.end())
        {
            nodstat = it->second;
            nodstat->parse(node, global_stats);
        }
        else
        {
            nodstat = new NodeStats(node, global_stats);
            nodes[nodstat->key] = nodstat;
        }
    }
}


void Stats::output(int display_style)
{
    printf("\n");

    for (NodesMap::iterator it = nodes.begin();
         it != nodes.end(); ++it)
    {
        NodeStats *node = it->second;
        node->output(global_stats);
    }

    global_stats.initOutput(display_style,
                            wrapping_in_nodes);

    global_stats.outputElapsed();

    if (display_style & STAT_GLOBAL)
        global_stats.outputSummary();

    if (display_style & STAT_VOLUME)
        global_stats.outputVolPerfStat();

    if (display_style & STAT_NDEV)
        global_stats.outputNDevPerfStat();

    if (display_style & STAT_VOL_ERR)
        global_stats.outputVolErrorStat();

    if (display_style & STAT_NDEV_ERR)
        global_stats.outputNdevErrorStat();

    if (display_style & STAT_SEQ_VOL)
        output_seq_volume();

    if (display_style & STAT_SEQ_DISK)
        output_seq_ndev();

    if (global_stats.output_warning && !reset)
    {
        printf(
            "\n Warning: Some devices have been started after the last stats reset, hence N/A was displayed. \n");
        printf("          Reset the stats to get the correct stats. \n");
    }
}


void Stats::output_seq_volume()
{
    printf("  Volumes Stats:\n");

    /* Use a hacked-up distorsion of the visitor pattern, so that the
     * nodes know how to display values properly. */
    for (NodesMap::iterator it = nodes.begin();
         it != nodes.end(); ++it)
    {
        NodeStats *node = it->second;
        node->output_seq_volume(*this);
    }

    printf("\n");
}


void Stats::output_seq_ndev()
{
    printf("  Disks Stats:\n");

    /* Use a hacked-up distorsion of the visitor pattern (again), so
     * that the nodes know how to display values properly. */
    for (NodesMap::iterator it = nodes.begin();
         it != nodes.end(); ++it)
    {
        NodeStats *node = it->second;
        node->output_seq_ndev(*this);
    }

    printf("\n");
}


void Stats::display_seq(const string &label,
                        uint64_t seq_read,
                        uint64_t seq_write,
                        uint64_t req_read,
                        uint64_t req_write,
                        uint64_t sect_read,
                        uint64_t sect_write,
                        uint64_t seek_dist_read,
                        uint64_t seek_dist_write) const
{
    printf("        %s", label.c_str());

    if (!req_read && !req_write)
        printf(": --\n");
    else
    {
        uint64_t bytes_read(sect_read * SECTOR_SIZE);
        uint64_t bytes_write(sect_write * SECTOR_SIZE);

        printf("\n");

        /* This used to say "Data transferred" in Exanodes 1.7, but it
         * was a gross lie, and my mother always told me that good boys
         * do not lie. */
        printf("            Data requested: %s (read=%s written=%s)\n",
               display_amount(bytes_read + bytes_write).c_str(),
               display_amount(bytes_read).c_str(),
               display_amount(bytes_write).c_str());

        if (req_read)
            display_seq_detail(false, req_read, bytes_read,
                               seq_read, seek_dist_read);

        if (req_write)
            display_seq_detail(true, req_write, bytes_write,
                               seq_write, seek_dist_write);
    }
}


void Stats::display_seq_detail(bool write, uint64_t reqs, uint64_t bytes,
                               uint64_t seqs, uint64_t seek_dist) const
{
    /* In a similar spirit of "not lying" as that exhibited in
     * Stats::display_seq, added a few "mean"s that had been forgotten,
     * as well. */
    printf("            %-6s %" PRIu64 " seq mean sz=%s mean dist=%s "
           "(%" PRIu64 " %ss mean sz=%s)\n",
           write ? "WRITE:" : "READ:",
           seqs,
           display_amount(bytes / seqs).c_str(),
           display_amount(seek_dist / seqs).c_str(),
           reqs,
           write ? "write" : "read",
           display_amount(bytes / reqs).c_str());
}


const string Stats::display_amount(uint64_t bytes) const
{
    string mantissa, unit;

    format_hum_friend_iB(bytes, mantissa, unit,
                         (display_style & STAT_FORCE_KILO));

    return mantissa + " " + unit;
}


void exa_clstats::run()
{
    string error_msg;
    exa_error_code error_code;
    string msg_str;
    Stats stats(display_style, wrapping_in_nodes);

    if (set_cluster_from_cache(_cluster_name, error_msg) != EXA_SUCCESS)
        throw CommandException(EXA_ERR_DEFAULT);

    msg_str = "Getting statistics for cluster '" + exa.get_cluster() + "'";

    AdmindCommand command("clstats", exa.get_cluster_uuid());
    command.add_param("reset", reset ? "TRUE" : "FALSE");

    std::shared_ptr<AdmindMessage> message(
        send_command(command, msg_str, error_code, error_msg));

    if (!message)
        throw CommandException("Failed to receive the response from admind.");

    shared_ptr<xmlDoc> subtree(
        xmlReadMemory(message->get_payload().c_str(),
                      message->get_payload().size(),
                      NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOERROR |
                      XML_PARSE_NOWARNING),
        xmlFreeDoc);
    xmlNodePtr node(NULL);

    if (subtree)
        node = xmlDocGetRootElement(subtree.get());

    if (!node || !xmlStrEqual(node->name, BAD_CAST "stats"))
        throw CommandException("Failed to parse admind response");

    stats.parse_xml(node->children);

    stats.reset = reset;
    stats.output(display_style);
}


void exa_clstats::parse_opt_args(const std::map<char, std::string> &opt_args)
{
    exa_clcommand::parse_opt_args(opt_args);

    if (opt_args.find('a') != opt_args.end())
        display_style |= STAT_DISPLAY_ALL;
    if (opt_args.find('r') != opt_args.end())
        reset = true;
    if (opt_args.find('s') != opt_args.end())
    {
        display_style &=
            ~(STAT_NDEV | STAT_VOL_ERR | STAT_NDEV_ERR | STAT_VOLUME);
        display_style |= STAT_GLOBAL;
    }
    if (opt_args.find('k') != opt_args.end())
        display_style |= STAT_FORCE_KILO;
    if (opt_args.find('q') != opt_args.end())
        display_style |= STAT_SEQ_VOL | STAT_SEQ_DISK;
    if (opt_args.find('m') != opt_args.end())
        display_style |= STAT_MATRIX;
    if (opt_args.find('w') != opt_args.end())
    {
        if (to_uint(opt_args.find('w')->second.c_str(), &wrapping_in_nodes)
            != EXA_SUCCESS)
            throw CommandException("Invalid wrapping number of nodes");
        if (wrapping_in_nodes == 0)
            /* Disabling node wrapping by setting a large enough number */
            wrapping_in_nodes = UINT_MAX / 2;
    }
}


void exa_clstats::dump_short_description(std::ostream &out,
                                         bool show_hidden) const
{
    out << "Display I/O statistics.";
}


void exa_clstats::dump_full_description(std::ostream &out,
                                        bool show_hidden) const
{
    out << "Display I/O statistics for the cluster " << ARG_CLUSTERNAME
        <<
    ". These statistics are average bandwidths since the last statistics reset."
        << std::endl;
}


void exa_clstats::dump_examples(std::ostream &out, bool show_hidden) const
{
    out << "A typical usage would be:" << std::endl;
    out << "  " << "- launch a workload," << std::endl;
    out << "  " << "- reset the stats with 'exa_clstats --reset " <<
    ARG_CLUSTERNAME
        << " (at this time displayed numbers are meaningless)," << std::endl;
    out << "  " << "- wait some time," << std::endl;
    out << "  " << "- display the stats with 'exa_clstats " << ARG_CLUSTERNAME
        << "'" << std::endl;
    out <<
    "To track bandwidth variations, you could replace the last step by:" <<
    std::endl;
    out << "  " << "- 'while true; do exa_clstats --reset " << ARG_CLUSTERNAME
        << "; sleep 5; done'" << std::endl << std::endl;
    out << "Because statistics are reset at each loop, this will display the "
        << "current bandwidth (actually the average of the 5 last seconds)." <<
    std::endl;
}


