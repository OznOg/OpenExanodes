#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(CompileCommandLine)

# --- function to declare a unit test --------------------------------

# usage :
# include(${CMAKE_SOURCE_DIR}/UnitTest.cmake)
# add_unit_test(ut_foo bar.c)
# target_link_libraries(ut_foo baz)
#
# ut_foo is the test name
# bar.c are additional source files
# baz are the libaries to link with

function(add_unit_test NAME)

# C or C++ ?
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cpp)
    set(UT_LANGUAGE CXX)
    set(UT_EXT cpp)
else (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cpp)
    set(UT_LANGUAGE C)
    set(UT_EXT c)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cpp)

get_directory_property(INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES)
get_directory_property(COMPILE_DEFINITIONS COMPILE_DEFINITIONS)

get_compile_commandline(${UT_LANGUAGE} COMPILE)

add_custom_command(
    OUTPUT tmp__${NAME}.${UT_EXT}
    COMMAND perl ${UT_BUILD} ${COMPILE} -o tmp__${NAME}.${UT_EXT} ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.${UT_EXT}
    DEPENDS ${UT_BUILD}
    IMPLICIT_DEPENDS ${UT_LANGUAGE} ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.${UT_EXT}
)

add_executable(${NAME} tmp__${NAME}.${UT_EXT} ${ARGN})

file(TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR} NATIVE_CURRENT_SOURCE_DIR)
string(REPLACE "\\" "\\\\" NATIVE_CURRENT_SOURCE_DIR ${NATIVE_CURRENT_SOURCE_DIR})
add_test(${NAME} ${NAME} -S "${NATIVE_CURRENT_SOURCE_DIR}")

if (WIN32)
    # Suppress warnings
    # - #1572 "floating-point (in)equality comparisons are unreliable"
    # - #111  "statement is unreachable"
    set_target_properties(${NAME} PROPERTIES COMPILE_FLAGS "/Qwd1572,111")
endif (WIN32)

endfunction(add_unit_test)
