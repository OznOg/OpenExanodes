/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_error.h"

#include "os/include/os_file.h"

#include "ui/common/include/common_utils.h"
#include "ui/common/include/cli_log.h"

#include "os/include/os_string.h"

struct log_lvl_str_entry {
  exa_cli_log_level_t lvl;
  const char *str;
};

static const struct log_lvl_str_entry log_lvl_str_table[] = {
  { EXA_CLI_ERROR, "ERROR" },
  { EXA_CLI_INFO,  "INFO"  },
  { EXA_CLI_LOG,   "LOG"   },
  { EXA_CLI_TRACE, "TRACE" },
  { EXA_CLI_NOLOG, NULL      }
};

/* Global Color definition */
const char *COLOR_USED    = OS_COLOR_BLUE;
const char *COLOR_INFO    = OS_COLOR_GREEN;
const char *COLOR_NORM    = OS_COLOR_DEFAULT;
const char *COLOR_ERROR   = OS_COLOR_RED;
const char *COLOR_WARNING = OS_COLOR_PURPLE;
const char *COLOR_NOTICE  = OS_COLOR_PURPLE;
const char *COLOR_FEATURE = OS_COLOR_CYAN;
const char *COLOR_BOLD    = OS_COLOR_LIGHT;

static exa_cli_log_level_t exa_verb = EXA_CLI_INFO;

/**
 * Convert a string to a verbosity level.
 *
 * NOTE: The parsing is case insensitive.
 *
 * @param[out] level  Verbosity level parsed
 * @param[in]  str    String to parse
 *
 * @return true if successful, false otherwise
 */
bool exa_verb_from_str(exa_cli_log_level_t *level, const char *str)
{
    int i;

    if (level == NULL || str == NULL)
        return false;

    for (i = 0; log_lvl_str_table[i].str != NULL; i++)
        if (os_strcasecmp(str, log_lvl_str_table[i].str) == 0)
        {
            *level = log_lvl_str_table[i].lvl;
            return true;
        }

    return false;
}

/**
 * Set the verbosity.
 *
 * @param[in] verb  New verbosity
 */
void set_exa_verb(exa_cli_log_level_t verb)
{
  exa_verb = verb;
}


/**
 * Get the verbosity.
 *
 * @return the verbosity level
 */
exa_cli_log_level_t get_exa_verb ( void)
{
  return exa_verb;
}

