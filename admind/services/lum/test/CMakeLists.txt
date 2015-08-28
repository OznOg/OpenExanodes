#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

set(lun_lib lun)
set(iqn_lib iqn)

if (WITH_FS)
set(fs_sources ${CMAKE_SOURCE_DIR}/admind/src/adm_fs.c)
endif (WITH_FS)

add_unit_test(ut_service_lum_exports
    ../src/service_lum_exports.c
    ${CMAKE_SOURCE_DIR}/admind/src/adm_group.c
    ${CMAKE_SOURCE_DIR}/admind/src/adm_volume.c
    ${fs_sources})

target_link_libraries(ut_service_lum_exports
        exa_common_user
	exa_license
        exa_os
        exa_config
	exa_export
        exalogclientfake
        ${LIBXML2_LIBRARIES}
        adm_export
        ${lun_lib}
        iqn
        iqn_filter)

add_unit_test(ut_adm_export
    ../src/adm_export.c)

target_link_libraries(ut_adm_export
     exa_common_user
     exa_os
     exa_export
     iqn
     iqn_filter)

add_unit_test(ut_target_config
    ../src/target_config.c)

target_link_libraries(ut_target_config
    exa_common_user
    exa_os)

