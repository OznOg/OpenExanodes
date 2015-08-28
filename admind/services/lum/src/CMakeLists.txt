#
# Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(service_lum_exports STATIC
	service_lum_exports.c
	${CMAKE_SOURCE_DIR}/admind/src/adm_file_ops.c)

target_link_libraries(service_lum_exports
	exa_config
	exa_export
        lun
        iqn
        iqn_filter)

add_library(service_lum STATIC
	service_lum.c)

target_link_libraries(service_lum
	service_lum_exports
	exa_config
	exa_export
        adm_export
        lun
        iqn
        iqn_filter
        target_config)

add_library(adm_export STATIC
    adm_export.c)

target_link_libraries(adm_export
    iqn
    iqn_filter)

add_library(target_config STATIC
    target_config.c)

target_link_libraries(target_config
    exa_common_user
    exa_os)

