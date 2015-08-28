#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)
include(CompileCommandLine)

add_unit_test(ut_xml_parse
    ../xml_proto.c
    testxml_command_parsing.c)

target_link_libraries(ut_xml_parse
    exa_common_user
    exa_os
    exalogclientfake
    exa_config)

# XXX 'COMPILE' stuff copied from UnitTest.cmake. Should be refactored.
get_directory_property(INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES)
get_directory_property(COMPILE_DEFINITIONS COMPILE_DEFINITIONS)

get_compile_commandline(C COMPILE)

set(CMD_LIST_FILE "${CMAKE_CURRENT_BINARY_DIR}/__testcommand_list.txt")
file(REMOVE ${CMD_LIST_FILE})
file(APPEND ${CMD_LIST_FILE} "${CMAKE_SOURCE_DIR}/admind/src/xml_proto/test/ut_xml_parse.c\n")

add_custom_command(OUTPUT testxml_command_parsing.c
    COMMAND perl ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/adm_command_extraction.pl ${COMPILE} ${CMD_LIST_FILE}
            | perl ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/generate_xml_parsing.pm -
            > testxml_command_parsing.c
    DEPENDS ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/test/ut_xml_parse.c
            ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/adm_command_extraction.pl
            ${CMAKE_SOURCE_DIR}/admind/src/xml_proto/generate_xml_parsing.pm)
