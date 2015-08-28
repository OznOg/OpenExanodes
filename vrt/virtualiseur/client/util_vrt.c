/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <string.h>
#include <errno.h>

#include "vrt/virtualiseur/include/vrt_common.h"
#include "common/include/exa_constants.h"


/**
 * The type used to define the group, volume and real device status
 * conversion tables.
 */
struct exa_status_table_entry
{
    int status;
    char *str;
};


/**
 * The group status conversion table.
 */
static struct exa_status_table_entry exa_group_status_table[] =
{
    { EXA_GROUP_OK,        ADMIND_PROP_OK        },
    { EXA_GROUP_DEGRADED,  ADMIND_PROP_DEGRADED  },
    { EXA_GROUP_OFFLINE,   ADMIND_PROP_OFFLINE   },
    { -1,                  NULL                  }
};


/**
 * Convert a group status to an human readable string
 *
 * @param[in] status The group status
 *
 * @return An human readable string describing the group status
 */
const char *
vrtd_group_status_str (exa_group_status_t status)
{
    int i;

    for (i = 0; exa_group_status_table[i].str != NULL; i++)
    {
	if (status == exa_group_status_table[i].status)
	    return exa_group_status_table[i].str;
    }

    return ADMIND_PROP_UNDEFINED;
}

/**
 * The realdev status conversion table
 */
static struct exa_status_table_entry exa_realdev_status_table[] =
{
    { EXA_REALDEV_DOWN,        ADMIND_PROP_DOWN        },
    { EXA_REALDEV_ALIEN,       ADMIND_PROP_ALIEN       },
    { EXA_REALDEV_OUTDATED,    ADMIND_PROP_OUTDATED    },
    { EXA_REALDEV_BLANK,       ADMIND_PROP_BLANK       },
    { EXA_REALDEV_UPDATING,    ADMIND_PROP_UPDATING    },
    { EXA_REALDEV_REPLICATING, ADMIND_PROP_REPLICATING },
    { EXA_REALDEV_OK,          ADMIND_PROP_OK          },
    { -1,                      NULL                    }
};


/**
 * Convert a realdev status to an human readable string
 *
 * @param[in] status A realdev status
 *
 * @return An human readable string describing the realdev status
 */
const char *
vrtd_realdev_status_str (exa_realdev_status_t status)
{
    int i;

    for (i = 0; exa_realdev_status_table[i].str != NULL; i++)
    {
	if (status == exa_realdev_status_table[i].status)
	    return exa_realdev_status_table[i].str;
    }

    return ADMIND_PROP_UNDEFINED;
}
