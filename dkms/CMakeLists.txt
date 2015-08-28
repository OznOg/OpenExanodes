#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dkms.conf.in
    ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf
    @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Makefile.dkms.in
    ${CMAKE_CURRENT_BINARY_DIR}/Makefile.dkms
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf
              ${CMAKE_CURRENT_BINARY_DIR}/Makefile.dkms
    DESTINATION ${DKMS_DIR})

install(PROGRAMS copy_symvers
    DESTINATION ${DKMS_DIR})
