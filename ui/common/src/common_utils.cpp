/*
 * Copyright 2002, 2011 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "os/include/os_inttypes.h"
#include "ui/common/include/common_utils.h"
#include "os/include/os_stdio.h"

#include <errno.h>
#include <math.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <sstream>

#include "common/include/exa_error.h"
#include "os/include/os_mem.h"

using boost::lexical_cast;
using std::string;
using std::vector;

static const uint64_t KILO = 1000ULL;
static const uint64_t I_KILO = 1024ULL;

const char PRE_BYTE [] = "B";
const char PRE_I_BYTE [] = "iB";

static const char *tabUnitBytes[] = {"B", "K", "M", "G", "T", "P", "E", NULL};

/** \brief Get the kilo factors in the format given.
 *
 * \param[in]  u: the new unit format.
 */
uint64_t exa_get_kilo (t_unit un)
{
    switch (un)
    {
    case NO_UNIT:
        return I_KILO;
        break;

    case UNIT_B:
        return KILO;
        break;

    case UNIT_IB:
        return I_KILO;
        break;
    }
    return 0ULL;
}

/** \brief Get the units stringlist formated in the new format given.
 *
 * \param[in]  u: the new unit format.
 */
vector<string> exa_get_tab (t_unit un)
{
    vector<string> table;
    switch (un)
    {
    case  NO_UNIT:
        for (int i = 0; tabUnitBytes [i] != NULL; i ++)
	{
            table.push_back(tabUnitBytes[i] );
	}
        break;

    case UNIT_B:
        for (int i = 0; tabUnitBytes [i] != NULL; i ++)
	{
            if (i == 0)
	    {
                table.push_back(string (tabUnitBytes[i]) );
                continue;
	    }
            table.push_back(string(tabUnitBytes [i]) + PRE_BYTE );
	}
        break;

    case UNIT_IB:
        for (int i = 0; tabUnitBytes [i] != NULL; i ++)
	{
            if (i == 0)
	    {
                table.push_back(string (PRE_BYTE) );
                continue;
	    }
            table.push_back(string(tabUnitBytes[i]) + PRE_I_BYTE );
	}
        break;
    }
    return table;
}

/** \brief Convert Size given in string to ULL.
 *
 * \param[in]  sizestr: size in string to convert.
 * \param[in]  sizeT: size in ULL.
 * \param[in]  u_k : kilo format.
 * \param[in]  u_t : unitTab format.
 *
 * \return true/false: success/error.
 */
static bool conv_Qstr2u64 (string sizestr, uint64_t * sizeT, t_unit u_k, t_unit u_t)
{
    char unit = 'u';
    double sizeD;
    uint64_t kilo = exa_get_kilo (u_k);

    setlocale (LC_NUMERIC, "C");

    if (sizestr.empty ())
    {
        * sizeT = (uint64_t) 0;
        return false;
    }

    while(sizestr[sizestr.length()-1] == ' ')
        sizestr.erase (sizestr.length()-1);

    while (! isdigit(sizestr[sizestr.length () - 1]) )
    {
        unit = sizestr [sizestr.length() - 1];
        sizestr.erase (sizestr.length() - 1);

        while(sizestr[sizestr.length()-1] == ' ')
            sizestr.erase (sizestr.length()-1);
    }

    sizeD = lexical_cast<double>(sizestr);
    *sizeT = 1;
    switch (unit)
    {
    case 't':
    case 'T':
        *sizeT *= kilo;
    case 'g':
    case 'G':
        *sizeT *= kilo;
    case 'm':
    case 'M':
        *sizeT *= kilo;
    case 'k':
    case 'K':
        *sizeT *= kilo;
    case 'u':
    case 'U':
    case 'b':
    case 'B':
        sizeD *= (double) (*sizeT);
        *sizeT = (uint64_t) sizeD;
        break;

    default:
        *sizeT = (uint64_t) 0;
        return false;
    }
    return true;
}

#define TMPBUFSIZE 16

/** \brief Convert a size given in ULL to value in string and unit in string.
 *
 * \param[in]  size: size in ULL to convert.
 * \param[out]  mant: mantiss in string.
 * \param[out]  unit: unit in string.
 * \param[in]  u_k : kilo format.
 * \param[in]  u_t : unitTab format.
 * \param[in]  force_kilo: flag to force the unit to kilo.
 *
 */
static void format_hum_friend(uint64_t size, string &mant, string &unit,
                              t_unit u_k, t_unit u_t, bool force_kilo = false)
{
    char tmpbuf[TMPBUFSIZE];
    uint64_t kilo = exa_get_kilo(u_k);
    double val;
    unsigned int divis;

    if (force_kilo)
        divis = 1;
    else
        /* This is a pseudo log base N.
         * We don't use a proper log(size)/log(kilo) and we take 1000 instead of
         * kilo as threshold because we want to print 1 GB and not 10XX MB for
         * values between 1000 and 1023 */
        for (divis = 0; size >= 1000 * pow((double)kilo, (double)divis); divis++);

    val = size / pow((double)kilo, (double)divis);
    os_snprintf(tmpbuf, TMPBUFSIZE, "%.1f", val);
    mant = tmpbuf;
    unit = exa_get_tab(u_t).at(divis);
}


/** \brief Convert a size given in ULL to value in string and unit in string in
 *  KiB format.
 *
 * \param[in]  size: size in ULL to convert.
 * \param[in]  mant: mantiss in string.
 * \param[in]  unit: unit in string.
 * \param[in]  force_kilo: flag to force the unit to kilo.
 *
 */
void format_hum_friend_iB(uint64_t size, string &mant, string &unit,
                          bool force_kilo)
{
    format_hum_friend(size, mant, unit, UNIT_IB, UNIT_IB, force_kilo);
}

/** \brief Convert a size given in ULL to value in string and unit in string in
 *  KB format.
 *
 * \param[in]  size: size in ULL to convert.
 * \param[in]  mant: mantiss in string.
 * \param[in]  unit: unit in string.
 * \param[in]  force_kilo: flag to force the unit to kilo.
 *
 */
void format_hum_friend(uint64_t size, string &mant, string &unit,
                       bool force_kilo)
{
    format_hum_friend(size, mant, unit, NO_UNIT, NO_UNIT, force_kilo);
}


/** \brief Convert Size given in string to ULL in KB format.
 *
 * \param[in]  sizeQstr: size in string to convert.
 * \param[in]  sizeT: size in ULL.
 *
 * \return true/false: success/error.
 *
 */
bool conv_Qstr2u64 (string sizeQstr, uint64_t *sizeT)
{
    return conv_Qstr2u64 (sizeQstr, sizeT, NO_UNIT, NO_UNIT);
}

/** \brief Get the units stringlist formated in KB.
 *
 */
vector<string> get_unitTable ()
{
    return exa_get_tab (NO_UNIT);
}

/** split a string on 2 columns and return each value
 *
 * \param sep[in]: the string separator
 * \param instr[in]: the string to parse
 * \param outstr1[out]: the 1st part of the splitted string
 * \param outstr2[out]: the 2nd part of the splitted string
 *
 * \return true if there is 2 valid values in outstr1 and outstr2
 */
bool column_split(const string &sep,
		  const string &instr, string &outstr1, string &outstr2)
{
    vector<string> SplitVec;
    boost::split(SplitVec, instr, boost::algorithm::is_any_of(sep));

    if (SplitVec.size() != 2)
        return false;

    outstr1 = SplitVec[0];
    outstr2 = SplitVec[1];

    return true;
}

/** split a string on 3 columns and return each value
 *
 * \param sep[in]: the string separator
 * \param instr[in]: the string to parse
 * \param outstr1[out]: the 1st part of the splitted string
 * \param outstr2[out]: the 2nd part of the splitted string
 * \return true if there is 2 valid values in outstr1 and outstr2
 */
bool column_split(const string &sep,
		  const string &instr,
		  string &outstr1, string &outstr2, string &outstr3)
{
    vector<string> SplitVec;
    boost::split(SplitVec, instr, boost::algorithm::is_any_of(sep));

    if (SplitVec.size() != 3)
        return false;

    outstr1 = SplitVec[0];
    outstr2 = SplitVec[1];
    outstr3 = SplitVec[2];

    return true;
}

/**
 * Parse "str", looking for the first "=", and set both "name" and
 * "value" strings. "value" can be empty or can contain one or several
 * "=" signs.
 * @param[in]  str    string to parse, must contain a "=" character.
 * @param[out] name   parameter's name
 * @param[out] value  parameter's value
 * @return     EXA_SUCCESS if successful, -EXA_ERR_DEFAULT otherwise
 */
int set_parameter(const string &str, string &name, string &value)
{
    string::size_type index = str.find_first_of("=");

    if (index != string::npos)
    {
        name  = str.substr(0, index);
        value = str.substr(index + 1);
        return EXA_SUCCESS;
    }

    return -EXA_ERR_DEFAULT;
}
