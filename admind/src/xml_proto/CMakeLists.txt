#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(CompileCommandLine)

file(GLOB CMD_SOURCES
     ${CMAKE_SOURCE_DIR}/admind/src/commands/exa_*.c)

if (NOT WITH_FS)
    file(GLOB FS_CMD_SOURCES
         ${CMAKE_SOURCE_DIR}/admind/src/commands/exa_fs*.c)
    list(REMOVE_ITEM CMD_SOURCES ${FS_CMD_SOURCES})
endif (NOT WITH_FS)

if (NOT WITH_MONITORING)
    file(GLOB MONIT_CMD_SOURCES
         ${CMAKE_SOURCE_DIR}/admind/src/commands/exa_clmonitor*.c)
    list(REMOVE_ITEM CMD_SOURCES ${MONIT_CMD_SOURCES})
endif (NOT WITH_MONITORING)

# The list is sorted in alphabetical order to keep the version number at the
# same value when building with automake and cmake.
# for an unknown reason, file taken with the 'file' command above are sorted
# by inodes...
list(SORT CMD_SOURCES)

set(CMD_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/__command_list.txt")
file(REMOVE ${CMD_LIST_FILE})
foreach (cmd_source ${CMD_SOURCES})
    file(APPEND ${CMD_LIST_FILE} "${cmd_source}\n")
endforeach (cmd_source)
file(APPEND ${CMD_LIST_FILE} "${CMAKE_SOURCE_DIR}/admind/src/instance.c")

get_compile_commandline(C COMPILE)

add_custom_command(OUTPUT command_hash.pl
    COMMAND echo COMPILE: ${COMPILE}
    COMMAND perl ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/adm_command_extraction.pl ${COMPILE} ${CMD_LIST_FILE}
            > command_hash.pl
    COMMAND perl -c command_hash.pl
    DEPENDS ${CMD_SOURCES}
            ${CMAKE_SOURCE_DIR}/admind/src/instance.c
            ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/adm_command_extraction.pl)

add_custom_command(OUTPUT xml_command_parsing.c
    COMMAND cat command_hash.pl
            | perl ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/generate_xml_parsing.pm -
            > xml_command_parsing.c
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/command_hash.pl)

set(XML_PROTOCOL_VERSION_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/command_hash.pl
    ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/xml_proto.c
    ${CMAKE_SOURCE_DIR}/common/include/exa_error.h)

add_custom_command(OUTPUT xml_protocol_version.h
    COMMAND cat ${XML_PROTOCOL_VERSION_SOURCES}
            | perl ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/generate_xml_protocol_version.pl -
            > xml_protocol_version.h
    DEPENDS ${XML_PROTOCOL_VERSION_SOURCES})

add_library(xml_proto STATIC
    xml_protocol_version.h # This header must be specified explicitely as it is generated
    xml_proto.c
    xml_command_parsing.c)

target_link_libraries(xml_proto ${LIBXML2_LIBRARIES})

if (WITH_NODES AND WITH_UT)
    add_subdirectory(test)
endif (WITH_NODES AND WITH_UT)
