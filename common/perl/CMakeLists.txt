#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_custom_target(exa_names.pm ALL
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/c_define_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_names.h
            exa_names.pm
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/c_define_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_names.h
    COMMENT "Generate exa_names.pm")

add_custom_target(exa_error.pm ALL
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/c_error_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_error.h
            ${CMAKE_SOURCE_DIR}/common/lib/exa_error.c
            exa_error.pm
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/c_error_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_error.h
            ${CMAKE_SOURCE_DIR}/common/lib/exa_error.c
    COMMENT "Generate exa_error.pm")

add_custom_target(exa_constants.pm ALL
    COMMAND perl ${CMAKE_SOURCE_DIR}/tools/c_define_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_constants.h
            exa_constants.pm
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/c_define_2_perl.pl
            ${CMAKE_SOURCE_DIR}/common/include/exa_constants.h
    COMMENT "Generate exa_constants.pm")

configure_file(constantes2.pm.in "${CMAKE_CURRENT_BINARY_DIR}/constantes.pm" @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/exa_names.pm"
              "${CMAKE_CURRENT_BINARY_DIR}/exa_error.pm"
              "${CMAKE_CURRENT_BINARY_DIR}/exa_constants.pm"
              "${CMAKE_CURRENT_BINARY_DIR}/constantes.pm"
    DESTINATION "${PKG_DATA_DIR}/test")
