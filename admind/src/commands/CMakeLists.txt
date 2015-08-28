#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#


if (WITH_FS)
    set(FS_SOURCES
        exa_fscheck.c
        exa_fscreate.c
        exa_fsdelete.c
        exa_fsresize.c
        exa_fsstart.c
        exa_fsstop.c
        exa_fstune.c
        exa_fsgettune.c
        exa_fscommon.c
    )
endif (WITH_FS)


if (WITH_MONITORING)
    set(MONITORING_SOURCES
        exa_clmonitorstart.c
        exa_clmonitorstop.c
    )
endif (WITH_MONITORING)


add_library(commands STATIC
    command_api.h
    exa_cldelete.c
    exa_clcreate.c
    exa_clinfo.c
    exa_clinfo_components.c
    exa_clinfo_export.c
    exa_clinfo_filesystem.c
    exa_clinfo_group.c
    exa_clinfo_monitoring.c
    exa_clinfo_node.c
    exa_clinfo_volume.c
    exa_clnodeadd.c
    exa_clnodedel.c
    exa_cldiskadd.c
    exa_cldiskdel.c
    exa_clshutdown.c
    exa_clnodestop.c
    exa_clstats.c
    exa_cltrace.c
    exa_cltune.c
    ${MONITORING_SOURCES}
    exa_dgcreate.c
    exa_dgdelete.c
    exa_dgdiskadd.c
    exa_dgdiskrecover.c
    exa_dgstart.c
    exa_dgstop.c
    exa_dgreset.c
    exa_dgcheck.c
    ${FS_SOURCES}
    exa_getconfig.c
    exa_getparam.c
    exa_getlicense.c
    exa_vlcreate.c
    exa_vldelete.c
    exa_vlresize.c
    exa_vlstart.c
    exa_vlstop.c
    exa_vltune.c
    exa_vlgettune.c
    tunelist.c
    tunelist.h
    exa_getclustername.c
    exa_getnodedisks.c
    exa_setlicense.c
)

