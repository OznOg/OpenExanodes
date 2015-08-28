#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(exalogclientfake STATIC
    fake_log.c)

add_library(exalogclient
    logapi.c
    logd_com_examsg.c)

add_library(exalogserver
    logd.c
    logd_com_examsg.c)
