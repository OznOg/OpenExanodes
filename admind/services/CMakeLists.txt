#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_subdirectory(lum)
add_subdirectory(nbd)
add_subdirectory(rdev)
add_subdirectory(tools)
add_subdirectory(vrt)

add_library(services STATIC
	admind.c
	csupd.c)

target_link_libraries(services
    service_lum
    service_lum_exports
    service_nbd
    service_vrt
    service_rdev)

if (WITH_FS)
add_subdirectory(fs)
target_link_libraries(services service_fs)
endif (WITH_FS)

if (WITH_MONITORING)
add_subdirectory(monitor)
target_link_libraries(services service_monitor)
endif (WITH_MONITORING)
