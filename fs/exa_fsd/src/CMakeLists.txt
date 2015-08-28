#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

add_executable(exa_fsd
	exa_fsd.c
	exa_fsd.h
	work_thread.c
	work_thread.h
	fscommands.c
	fscommands.h)

target_link_libraries(exa_fsd
	examsg
	exa_common_user
	daemon_request_queue
	daemon_server
	exalogclient
	exa_os
	os_daemon_child)

install(TARGETS exa_fsd DESTINATION ${SBIN_DIR})
