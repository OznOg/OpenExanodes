/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef __ADM_COMMAND_H__
#define __ADM_COMMAND_H__

/**
 * @file
 *
 * Structures needed to define a XML command and its parameters. This
 * file is the complete version, used by admind.
 */
#include "admind/src/admindstate.h"
#include "admind/src/rpc_command.h"
#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"

typedef void (*ClusterCommand) (admwrk_ctx_t *ctx, void *data, cl_error_desc_t *err_desc);

/** Describe all the command code for the XML Admind protocol
 */
typedef enum {
  EXA_ADM_INVALID = 0,
  EXA_ADM_GETCONFIG,
  EXA_ADM_GETCONFIGCLUSTER,
  EXA_ADM_GETPARAM,
  EXA_ADM_GET_NODEDISKS,
  EXA_ADM_CLINFO,
  EXA_ADM_CLCREATE,
  EXA_ADM_CLDISKADD,
  EXA_ADM_CLDISKDEL,
  EXA_ADM_CLNODEADD,
  EXA_ADM_CLNODEDEL,
  EXA_ADM_CLINIT,
  EXA_ADM_CLSHUTDOWN,
  EXA_ADM_CLNODESTOP,
  EXA_ADM_CLDELETE,
  EXA_ADM_CLSTATS,
  EXA_ADM_CLTUNE,
#ifdef WITH_MONITORING
  EXA_ADM_CLMONITORSTART,
  EXA_ADM_CLMONITORSTOP,
#endif
  EXA_ADM_DGCREATE,
  EXA_ADM_DGSTART,
  EXA_ADM_DGSTOP,
  EXA_ADM_DGDELETE,
  EXA_ADM_DGRESET,
  EXA_ADM_DGCHECK,
  EXA_ADM_DGDISKRECOVER,
  EXA_ADM_DGDISKADD,
  EXA_ADM_VLCREATE,
  EXA_ADM_VLDELETE,
  EXA_ADM_VLRESIZE,
  EXA_ADM_VLSTART,
  EXA_ADM_VLSTOP,
  EXA_ADM_VLTUNE,
  EXA_ADM_VLGETTUNE,
#ifdef WITH_FS
  EXA_ADM_FSCREATE,
  EXA_ADM_FSSTART,
  EXA_ADM_FSSTOP,
  EXA_ADM_FSDELETE,
  EXA_ADM_FSRESIZE,
  EXA_ADM_FSCHECK,
  EXA_ADM_FSTUNE,
  EXA_ADM_FSGETTUNE,
#endif
  EXA_ADM_CLTRACE,
  EXA_ADM_GET_CLUSTER_NAME,
  EXA_ADM_GETLICENSE,
  EXA_ADM_SETLICENSE,
  EXA_ADM_RUN_SHUTDOWN,
  EXA_ADM_RUN_RECOVERY,
  EXA_ADM_LAST_COMMAND
} adm_command_code_t;

/**
 * Definition of an XML command
 */
typedef struct AdmCommand {

  /** Code */
  adm_command_code_t	 code;

  /** Short name */
  const char            *msg;

  /** Is this command allowed in a recovery ? */
  int allowed_in_recovery;

  /** the received uuid must match the one of the cluster the node is part of */
  bool match_cl_uuid;

  /** Status of admind for which the command is accepted */
  AdmindStatus		 accepted_status;

  /** The function implementing the cluster command */
  ClusterCommand	 cluster_command;

  /** List of local commands */
  struct {
    rpc_command_t id;
    LocalCommand fct;
  } local_commands[];

} AdmCommand;

const AdmCommand *adm_command_find(adm_command_code_t code);
const char *adm_command_name(adm_command_code_t code);
void adm_command_init_processing(void);

#endif /* __ADM_COMMAND_H__ */
