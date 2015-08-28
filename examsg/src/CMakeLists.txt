#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(examsg STATIC
    examsgapi.c
    objpoolapi.c
    mailbox.c
    ringbuf.c
    msg_type_names.c)

set_source_files_properties(msg_type_names.c GENERATED)

add_custom_command(
    OUTPUT msg_type_names.c
    COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/gen_examsg_name_tab.pl ${CMAKE_SOURCE_DIR}/examsg/include/examsg.h > msg_type_names.c
    DEPENDS ${CMAKE_SOURCE_DIR}/examsg/include/examsg.h)
