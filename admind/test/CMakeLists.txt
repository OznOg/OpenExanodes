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
set(fs_sources ../src/adm_fs.c)
endif (WITH_FS)

add_unit_test(ut_adm_tunelist
	../src/commands/tunelist.c)

target_link_libraries(ut_adm_tunelist
	exa_os
	exa_license
	${LIBXML2_LIBRARIES})

add_unit_test(ut_adm_xml
	../src/adm_cluster.c
	../src/adm_atomic_file.c
	../src/rpc_command.c
	../src/adm_hostname.c
	../src/adm_deserialize.c
	../src/adm_disk.c
	${fs_sources}
	../src/adm_group.c
	../services/vrt/vrt_layout.c
	../services/vrt/sb_version.c
	../src/adm_nic.c
	../src/adm_node.c
	../src/adm_nodeset.c
	../src/adm_serialize.c
	../src/adm_volume.c
	../src/service_parameter.c
	../services/rdev/src/rdev_config.c)

target_link_libraries(ut_adm_xml
        exa_config
	exa_common_user
        exalogclientfake
	exa_os
	service_lum_exports
	exa_license
	vrt_common
	${LIBXML2_LIBRARIES}
        ${lun_lib})

# Generates a valid test config flag
execute_process(COMMAND uname -n
	OUTPUT_VARIABLE HOSTNAME
	OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/test.conf.tmpl
                COMMAND sed "s/EXA_VERSION/${EXA_VERSION}/"
                COMMAND sed "s/HOSTNAME/${HOSTNAME}/"
    OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/test.conf.in)

# The windows version of perl REQUIRES to backup the file on which the command is done
# That's why we need to remove the backup once the command is done...
if (NOT WITH_FS)
    execute_process(COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/test.conf.in.delme
	            COMMAND perl -pi.delme -e "s/^<fs.*$//" ${CMAKE_CURRENT_BINARY_DIR}/test.conf.in)
endif (NOT WITH_FS)

add_unit_test(ut_adm_nodeset
	../src/adm_nodeset.c
	../src/adm_cluster.c
	../src/adm_atomic_file.c
	../src/rpc_command.c
	../src/adm_hostname.c
	../src/adm_disk.c
	../src/adm_group.c
	../services/vrt/vrt_layout.c
	../services/vrt/sb_version.c
	../src/adm_nic.c
	../src/adm_node.c
	../src/adm_volume.c
	${fs_sources}
	../src/service_parameter.c)

target_link_libraries(ut_adm_nodeset
        exa_config
	exa_common_user
        exalogclientfake
	exa_license
	exa_os
        vrt_common)

add_unit_test(ut_adm_hostname
    ../src/adm_hostname.c)

target_link_libraries(ut_adm_hostname
    exa_common_user
    exalogclientfake
    exa_license
    exa_os)

add_unit_test(ut_adm_cluster
	../src/adm_cluster.c
	../src/adm_atomic_file.c
	../src/rpc_command.c
	../src/adm_hostname.c
	../src/adm_disk.c
	${fs_sources}
	../src/adm_group.c
	../services/vrt/vrt_layout.c
	../services/vrt/sb_version.c
	../src/adm_nic.c
	../src/adm_node.c
	../src/adm_nodeset.c
	../src/adm_volume.c
	../src/service_parameter.c)

target_link_libraries(ut_adm_cluster
        exa_config
	exa_common_user
        exalogclientfake
	exa_license
	exa_os
        vrt_common)

add_unit_test(ut_adm_deserialize
	../src/adm_deserialize.c
	../src/adm_cluster.c
	../src/adm_atomic_file.c
	../src/rpc_command.c
	../src/adm_hostname.c
	../src/adm_disk.c
	${fs_sources}
	../src/adm_group.c
	../services/vrt/vrt_layout.c
	../services/vrt/sb_version.c
	../src/adm_nic.c
	../src/adm_node.c
	../src/adm_nodeset.c
	../src/adm_serialize.c
	../src/adm_volume.c
	../src/service_parameter.c
	../services/rdev/src/rdev_config.c)

target_link_libraries(ut_adm_deserialize
	exa_common_user
        exalogclientfake
	exa_os
	service_lum_exports
	vrt_common
	${LIBXML2_LIBRARIES}
        ${lun_lib})

add_unit_test(ut_adm_license
	../src/adm_atomic_file.c
	../src/adm_hostname.c
	)

target_link_libraries(ut_adm_license
	exa_license
        exa_config
	exa_common_user
        exalogclientfake
	exa_os
	)
add_unit_test(ut_adm_file_ops
	../src/adm_file_ops.c
	)

target_link_libraries(ut_adm_file_ops
        exa_config
	exa_common_user
        exalogclientfake
	exa_os
	)

add_unit_test(ut_service_parameter
	../src/adm_nodeset.c
	../src/adm_cluster.c
	../src/adm_atomic_file.c
	../src/adm_hostname.c
	../src/adm_disk.c
	${fs_sources}
	../src/adm_group.c
	../services/vrt/vrt_layout.c
	../services/vrt/sb_version.c
	../src/adm_nic.c
	../src/adm_node.c
	../src/adm_volume.c
	../src/service_parameter.c)

target_link_libraries(ut_service_parameter
        exa_config
        exa_common_user
        exalogclientfake
	exa_license
        exa_os
        vrt_common)

add_unit_test(ut_adm_nic
    ../src/adm_nic.c)

target_link_libraries(ut_adm_nic
	exa_license
        exa_os)

add_unit_test(ut_vrt_layout
	../src/adm_atomic_file.c
	../src/adm_cluster.c
	../src/adm_volume.c
	../src/adm_disk.c
        ${fs_sources}
	../src/adm_group.c
	../src/adm_node.c
	../src/adm_hostname.c
	../src/service_parameter.c
	../services/vrt/sb_version.c
	../src/adm_nodeset.c
	../src/adm_nic.c
	../services/vrt/vrt_layout.c)

target_link_libraries(ut_vrt_layout
        exa_config
        exa_common_user
        exalogclientfake
	exa_license
        exa_os
        vrt_common)

