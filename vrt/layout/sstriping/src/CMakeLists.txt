#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_MONITORING)
    set(LIBMONITCLIENT md_client)
endif (WITH_MONITORING)

add_library(sstriping STATIC
    lay_sstriping_group.c
    lay_sstriping_module.c
    lay_sstriping_request.c
    lay_sstriping_striping.c
    lay_sstriping_superblock.c)

target_link_libraries(sstriping
    ${LIBMONITCLIENT})
