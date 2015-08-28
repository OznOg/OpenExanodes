/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __COMMON_UTILS_H__
#define __COMMON_UTILS_H__

#include "os/include/os_inttypes.h"

#include <string>
#include <vector>

typedef enum { NO_UNIT, UNIT_B, UNIT_IB} t_unit;

extern "C"
{
#include "common/include/exa_conversion.h"
}

/*
 * FIXME: This has to be one of my favourite function name. Especially
 * when we could just override operator= to do it for us (if Qt
 * doesn't already have it, which would be quite sensible).
 */
// In iBytes format
void format_hum_friend_iB ( uint64_t size, std::string & mant, std::string & unit,
			    bool force_kilo = false);


// In no format
void format_hum_friend ( uint64_t size, std::string & mant, std::string & unit,
			   bool force_kilo = false);
bool conv_Qstr2u64 ( std::string sizeQstr, uint64_t * sizeT);
std::vector<std::string> get_unitTable ();

bool column_split(const std::string &sep,
		  const std::string &instr,
		  std::string &outstr1, std::string &outstr2);
bool column_split(const std::string &sep,
		  const std::string &instr,
		  std::string &outstr1, std::string &outstr2, std::string &outstr3);

template<class Container>
std::string strjoin(const std::string &separator, const Container &container)
{
  std::string retval;
  typename Container::const_iterator it(container.begin());

  while (it != container.end())
    {
      retval += *it;

      ++it;

      if (it != container.end())
	retval += separator;
    }

  return retval;
}


int set_parameter(const std::string& str, std::string& name, std::string& value);

#endif /* __COMMON_UTILS_H__ */
