/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "target/iscsi/include/lun.h"
#include "target/iscsi/include/endianness.h"
#include "common/include/exa_conversion.h"
#include "common/include/exa_assert.h"
#include "os/include/os_stdio.h"

lun_t lun_from_str(const char *str)
{
    lun_t lun;

    if (str == NULL || to_uint64(str, &lun) < 0)
        return LUN_NONE;

    if (!LUN_IS_VALID(lun))
        return LUN_NONE;

    return lun;
}

const char *lun_to_str(lun_t lun)
{
    static __thread char str[LUN_STR_LEN + 1];

    if (!LUN_IS_VALID(lun))
        return NULL;

    if (os_snprintf(str, sizeof(str), "%"PRIu64, lun) >= sizeof(str))
        return NULL;

    return str;
}

/*
 * http://en.wikipedia.org/wiki/Logical_Unit_Number
 *
 * "In current SCSI, a LUN is a 64 bit identifier. (The name Logical Unit Number
 * is historical; it is not a number). It is divided into 4 16 bit pieces that
 * reflect a multilevel addressing scheme, and it is unusual to see any but the
 * first of these used.
 *
 * People usually represent a 16 bit single level LUN as a decimal number.
 *
 * In earlier versions of SCSI, and with some transport protocols, LUNs are 16
 * or 6 bits."
 */

void lun_set_bigendian(lun_t lun, unsigned char buffer[8])
{
    /* The last three LUN pieces (bytes 2-7) are forced to zero */
    set_bigendian16((uint16_t)lun, buffer);
    set_bigendian16((uint16_t)0, buffer + 2);
    set_bigendian16((uint16_t)0, buffer + 4);
    set_bigendian16((uint16_t)0, buffer + 6);
}

lun_t lun_get_bigendian(const unsigned char buffer[8])
{
    /* The last three LUN pieces (bytes 2-7) are ignored */
    return (lun_t)get_bigendian16(buffer);
}
