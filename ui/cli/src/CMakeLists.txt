#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

if (WITH_MONITORING)
    set(MONITORING_SOURCES
        exa_clmonitorstart.cpp
        exa_clmonitorstop.cpp)
endif (WITH_MONITORING)

if (WITH_FS)
    set(FSCOMMANDS_SOURCES
	exa_fscheck.cpp
	exa_fscreate.cpp
	exa_fsdelete.cpp
	exa_fsresize.cpp
	exa_fsstart.cpp
	exa_fsstop.cpp
	exa_fstune.cpp)
    set(FS_COMMAND_LIB_SOURCES
      	exa_fscommand.cpp)
endif (WITH_FS)

set(EXA_CLI_COMMON_SOURCES
    exa_clcreate.cpp
    exa_cldelete.cpp
    exa_cldiskadd.cpp
    exa_cldiskdel.cpp
    exa_clinfo.cpp
    exa_cllicense.cpp
    exa_clnodeadd.cpp
    exa_clnodedel.cpp
    exa_clnodestart.cpp
    exa_clnodestop.cpp
    exa_clreconnect.cpp
    exa_clnoderecover.cpp
    exa_clstart.cpp
    exa_clstats.cpp
    exa_cltrace.cpp
    exa_cltune.cpp
    exa_dgcreate.cpp
    exa_dgdelete.cpp
    exa_dgstart.cpp
    exa_dgstop.cpp
    exa_dgdiskrecover.cpp
    exa_dgdiskadd.cpp
    exa_expand.cpp
    exa_unexpand.cpp
    exa_vlcreate.cpp
    exa_vldelete.cpp
    exa_vlresize.cpp
    exa_vlstart.cpp
    exa_vlstop.cpp
    exa_vltune.cpp
    ${MONITORING_SOURCES}
    exa_makeconfig.cpp
    ${FSCOMMANDS_SOURCES})

set(EXA_CLI_COMMAND_LIB_SOURCES
    cli_log_impl.cpp
    command.cpp
    command_arg.cpp
    command_option.cpp
    command_param.cpp
    command_version.cpp
    exa_clcommand.cpp
    exa_dgcommand.cpp
    exa_vlcommand.cpp
    license.cpp
    ${FS_COMMAND_LIB_SOURCES}
    info_help.cpp)

add_library(exa_cli_command_lib
    ${EXA_CLI_COMMAND_LIB_SOURCES})

add_dependencies(exa_cli_command_lib git.h)

add_library(exa_cli_lib
    ${EXA_CLI_COMMON_SOURCES})

add_executable(exa_cli main.cpp)
    

target_link_libraries(exa_cli
    exa_cli_lib
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os
    ${LIBXML2_LIBRARIES})

target_compile_definitions(exa_cli PRIVATE WITH_COMMANDS)
install(TARGETS exa_cli DESTINATION ${BIN_DIR})

if (WITH_TOOLS)
add_executable(exa_clitools
    main.cpp
    exa_dgreset.cpp
    exa_dgcheck.cpp)
target_link_libraries(exa_clitools
    exa_cli_command_lib
    exa_ui_common
    exa_common_user
    exa_config
    exa_os
    ${LIBXML2_LIBRARIES})

target_compile_definitions(exa_clitools PRIVATE WITH_TOOLS)

install(TARGETS exa_clitools DESTINATION ${BIN_DIR})
endif (WITH_TOOLS)

if (NOT WIN32)
    add_executable(exa_pod
        main.cpp
        exa_dgreset.cpp
        exa_dgcheck.cpp
        info_pod.cpp)

    target_link_libraries(exa_pod
        exa_cli_lib
        exa_cli_command_lib
        exa_ui_common
        exa_common_user
        exa_config
        exa_os
        ${LIBXML2_LIBRARIES})
target_compile_definitions(exa_pod PRIVATE WITH_TOOLS WITH_COMMANDS)
endif (NOT WIN32)

if (WITH_DOCS)

    add_executable(exa_referenceguide
        info_referenceguide.cpp)

    target_link_libraries(exa_referenceguide
        exa_cli_lib
        exa_cli_command_lib
        exa_ui_common
        exa_common_user
        exa_config
        exa_os
        ${LIBXML2_LIBRARIES})

endif (WITH_DOCS)

if (WITH_FS)
    set(FSCOMMANDS_LIST
        exa_fscheck
        exa_fscreate
        exa_fsdelete
        exa_fsresize
        exa_fsstart
        exa_fsstop
        exa_fstune)
endif (WITH_FS)

if (WITH_MONITORING)
    set(MONITORINGCOMMANDS_LIST
        exa_clmonitorstart
        exa_clmonitorstop)
endif (WITH_MONITORING)

if (WITH_TOOLS)
    set(EXA_CLITOOLS_LIST
        exa_dgreset
        exa_dgcheck)
endif (WITH_TOOLS)

set(COMMANDS_LIST
    exa_clcreate
    exa_cldelete
    exa_cldiskadd
    exa_cldiskdel
    exa_clinfo
    exa_cllicense
    exa_clnodeadd
    exa_clnodedel
    exa_clnodestart
    exa_clnodestop
    exa_clreconnect
    exa_clnoderecover
    exa_clstart
    exa_clstats
    exa_clstop
    exa_cltrace
    exa_cltune
    ${MONITORINGCOMMANDS_LIST}
    exa_dgcreate
    exa_dgdelete
    exa_dgstart
    exa_dgstop
    exa_dgdiskrecover
    exa_dgdiskadd
    exa_expand
    exa_makeconfig
    exa_unexpand
    exa_vlcreate
    exa_vldelete
    exa_vlresize
    exa_vlstart
    exa_vlstop
    exa_vltune
    ${FSCOMMANDS_LIST})

set(EXA_SCRIPTS
    exa_gatherinfo
    exa_install)

if (NOT WIN32)

    foreach(EXA_COMMAND ${COMMANDS_LIST} ${EXA_CLITOOLS_LIST})
        add_custom_command(TARGET exa_pod
            POST_BUILD
            COMMAND ./exa_pod ${EXA_COMMAND} -h > ${EXA_COMMAND}.pod
            COMMAND pod2man --section=1 -c "Exanodes Commands"
                            --name ${EXA_COMMAND}
                            --release "Exanodes ${EXA_VERSION}" ${EXA_COMMAND}.pod
                    > ${EXA_COMMAND}.1
#            COMMAND rm -f ${EXA_COMMAND}.pod
            COMMENT "Generate man page for ${EXA_COMMAND}")
    endforeach(EXA_COMMAND ${COMMANDS_LIST} ${EXA_CLITOOLS_LIST})

    foreach(EXA_COMMAND ${EXA_SCRIPTS})
        add_custom_command(TARGET exa_pod
            POST_BUILD
            COMMAND pod2man --section=1 -c "Exanodes Commands"
                            --name ${EXA_COMMAND}
                            --release "Exanodes ${EXA_VERSION}"
                            ${CMAKE_CURRENT_SOURCE_DIR}/${EXA_COMMAND}
                    > ${EXA_COMMAND}.1
            COMMENT "Generate man page for ${EXA_COMMAND}")
    endforeach(EXA_COMMAND ${EXA_SCRIPTS})

    if (WITH_DOCS)
        foreach(EXA_COMMAND ${COMMANDS_LIST})
            add_custom_command(TARGET exa_referenceguide
                POST_BUILD
                COMMAND ./exa_referenceguide ${EXA_COMMAND} -h | xmllint --nowarning --format - > ${EXA_COMMAND}.xml
                COMMENT "Generate reference guide for ${EXA_COMMAND}")
        endforeach(EXA_COMMAND ${COMMANDS_LIST})
    endif (WITH_DOCS)

endif (NOT WIN32)

# FIXME WIN32: port gatherinfo to Windows
if (NOT WIN32)

    foreach(EXA_COMMAND ${COMMANDS_LIST} ${EXA_SCRIPTS})
        install(CODE "execute_process(
            COMMAND /bin/ln -sf exa_cli ${EXA_COMMAND}
            WORKING_DIRECTORY \$ENV{DESTDIR}/${BIN_DIR})")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXA_COMMAND}.1
            DESTINATION ${MAN_DIR}/man1)
    endforeach(EXA_COMMAND ${COMMANDS_LIST})

    if (WITH_TOOLS)
        foreach(EXA_COMMAND ${EXA_CLITOOLS_LIST})
            install(CODE "execute_process(
                COMMAND /bin/ln -sf exa_clitools ${EXA_COMMAND}
                WORKING_DIRECTORY \$ENV{DESTDIR}/${BIN_DIR})")
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EXA_COMMAND}.1
                DESTINATION ${MAN_DIR}/man1)
        endforeach(EXA_COMMAND ${EXA_CLITOOLS_LIST})
    endif (WITH_TOOLS)

    install(PROGRAMS ${EXA_SCRIPTS}
        DESTINATION ${BIN_DIR})

endif (NOT WIN32)
