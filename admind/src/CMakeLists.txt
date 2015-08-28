#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_subdirectory(xml_proto)

if (WITH_NODES)
add_subdirectory(commands)
add_subdirectory(evmgr)

# --- Source files ---------------------------------------------------

if (WITH_FS)
    set(fs_sources adm_fs.c)
    set(fs_lib exafsd_client)
endif (WITH_FS)

set(adm_monitor_sources adm_monitor.c)

add_executable(exa_admind
    adm_atomic_file.c
    adm_atomic_file.h
    adm_cache.c
    adm_cache.h
    adm_cluster.c
    adm_cluster.h
    adm_command.c
    adm_command.h
    adm_deserialize.c
    adm_deserialize.h
    adm_disk.c
    adm_disk.h
    adm_error.h
    adm_file_ops.c
    adm_file_ops.h
    adm_globals.c
    adm_globals.h
    adm_group.c
    adm_group.h
    adm_hostname.c
    adm_hostname.h
    adm_incarnation.c
    adm_incarnation.h
    admind.c
    admind.h
    admindstate.c
    admindstate.h
    adm_license.h
    adm_monitor.h
    ${adm_monitor_sources}
    adm_nic.c
    adm_nic.h
    adm_node.c
    adm_node.h
    adm_nodeset.c
    adm_nodeset.h
    adm_serialize.c
    adm_serialize.h
    adm_service.c
    adm_service.h
    adm_volume.c
    adm_volume.h
    adm_workthread.c
    adm_workthread.h
    cli_server.c
    cli_server.h
    daemon_api_client.c
    deviceblocks.h
    ${fs_sources}
    instance.c
    instance.h
    rpc.c
    rpc_command.c
    rpc_command.h
    rpc.h
    saveconf.c
    saveconf.h
    service_parameter.c
    service_parameter.h)

add_library(exa_license STATIC
    adm_license.c
    adm_license.h
    adm_file_ops.c
    adm_file_ops.h
    cacert.h)

target_link_libraries(exa_license
    exa_common_user
    exa_os
    -lcrypto
    ${OPENSSL_LIBEAY_LIBRARY}
    ${OPENSSL_LIBRARIES})

    set(lun_lib lun)

# Required no matter what
set(iqn_lib iqn)
set(iqn_filter_lib iqn_filter)

if (WITH_MONITORING)
    set(monitoring_lib md_client)
endif (WITH_MONITORING)

target_link_libraries(exa_admind
    commands
    exa_common_user
    exa_config
    exa_evmgr
    exa_export
    exa_license
    exalogclient
    exalogclientfake
    exalogserver
    examsg
    lum_client
    examsgd_client
    exanbdservice_client
    exa_os
    exavirtualizer_client
    vrt_common
    ${fs_lib}
    services
    sys_service
    xml_proto
    ${lun_lib}
    ${iqn_lib}
    ${iqn_filter_lib})

add_dependencies(exa_admind git.h)

add_definitions(${license_cflags} -DSSL_X509_CONST="$(SSL_X509_CONST)")

install(TARGETS exa_admind DESTINATION ${SBIN_DIR})

endif (WITH_NODES)
