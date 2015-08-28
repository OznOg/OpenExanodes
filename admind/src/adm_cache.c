/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_cache.h"
#include "admind/src/adm_globals.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_hostname.h"
#include "admind/src/adm_incarnation.h"
#include "admind/include/service_lum.h"
#include "admind/services/rdev/include/rdev.h"
#include "admind/services/vrt/sb_version.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_env.h"
#include "os/include/os_dir.h"

/* Remove everything from the cache directory. This is best-effort only. */
void adm_cache_cleanup(void)
{
    lum_exports_remove_exports_file();
    rdev_remove_broken_disks_file();
    adm_hostname_delete_file();
    adm_license_uninstall(exanodes_license);
    exanodes_license = NULL;
    adm_cluster_delete_goal();
    adm_delete_incarnation();

    sb_version_remove_directory();
    os_dir_remove(exa_env_cachedir());
}

int adm_cache_create(void)
{
    return os_dir_create_recursive(exa_env_cachedir());
}
