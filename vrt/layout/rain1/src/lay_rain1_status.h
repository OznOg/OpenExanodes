/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __LAY_RAIN1_STATUS_H__
#define __LAY_RAIN1_STATUS_H__

#include "vrt/virtualiseur/include/vrt_group.h"
#include "vrt/layout/rain1/src/lay_rain1_group.h"
#include "vrt/layout/rain1/src/lay_rain1_rdev.h"

/* --- Function prototypes ---------------------------------------- */

/**
 * Predicate -- True if the SPOF group has a defect (down or outdated)
 *
 * @param[in] rxg               rain1 group data
 * @param[in] spof_group        SPOF group
 *
 * @return true if the SPOF group is down, FALSE otherwise
 */
bool rain1_spof_group_has_defect(const rain1_group_t *rxg,
                           const spof_group_t *spof_group);


/**
 * Compose the 'old' real device status
 *
 * @param[in] layout_data   rain1 group data
 * @param[in] rdev	    the real device
 *
 * @return the compound status of the device
 */
exa_realdev_status_t rain1_rdev_get_compound_status(const void *layout_data,
                                                    const struct vrt_realdev *rdev);

/* change status */

/**
 * Compute and update the status of a group and its real devices.
 *
 * @param[in] group	the group to compute the status
 */
void rain1_compute_status(struct vrt_group *group);


/**
 * Mark replications and updates as finished.
 * Called in post_reintegrate after the rebuildings. Rebuildings are
 * stopped now.
 *
 * @param[in] lg        The layout data
 */
void rain1_rebuild_finish(rain1_group_t *lg);


#endif
