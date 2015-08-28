#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_MONITORING)
    set(LIBMONITCLIENT md_client)
endif (WITH_MONITORING)

add_library(rain1 STATIC
    lay_rain1_metadata.c
    lay_rain1_check.c
    lay_rain1_desync_info.c
    lay_rain1_group.c
    lay_rain1_module.c
    lay_rain1_rdev.c
    lay_rain1_request.c
    lay_rain1_status.c
    lay_rain1_striping.c
    lay_rain1_superblock.c
    lay_rain1_sync.c
    lay_rain1_sync_job.c
    lay_rain1_sync_tag.c)

target_link_libraries(rain1
    vrt_common
    file_stream
    ${LIBMONITCLIENT})
