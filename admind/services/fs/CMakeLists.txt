#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_library(service_fs STATIC
		service_fs.c
		service_fs.h
		generic_fs.c
		generic_fs.h
		type_gfs.c
		type_gfs.h
		type_clustered.c
		type_clustered.h
		type_local.c
		type_local.h)

target_link_libraries(service_fs os_daemon_parent)