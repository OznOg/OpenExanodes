/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __ADM_SERVICE_H
#define __ADM_SERVICE_H


#include "admind/src/rpc_command.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"

typedef enum adm_service_id {
#define ADM_SERVICE_FIRST ADM_SERVICE_ADMIN
    ADM_SERVICE_ADMIN = 0,
    ADM_SERVICE_RDEV,
    ADM_SERVICE_NBD,
    ADM_SERVICE_VRT,
    ADM_SERVICE_LUM,
#ifdef WITH_FS
    ADM_SERVICE_FS,
#endif
#ifdef WITH_MONITORING
    ADM_SERVICE_MONITOR
#endif
#ifdef WITH_MONITORING
#define ADM_SERVICE_LAST ADM_SERVICE_MONITOR
#elif defined(WITH_FS)
#define ADM_SERVICE_LAST ADM_SERVICE_FS
#else
#define ADM_SERVICE_LAST ADM_SERVICE_LUM
#endif
} adm_service_id_t;

#define ADM_SERVICE_ID_IS_VALID(id) \
    ((id) >= ADM_SERVICE_FIRST && (id) <= ADM_SERVICE_LAST)

struct adm_disk;
struct adm_node;
struct instance;

typedef struct stop_data
{
    exa_nodeset_t nodes_to_stop;
    bool    force;
    bool    goal_change;
} stop_data_t;

struct adm_service
{
  adm_service_id_t id;

  /* The following methods should be called on the leader only. */
  int (* init)(admwrk_ctx_t *ctx);
  int (* recover)(admwrk_ctx_t *ctx);
  int (* resume)(admwrk_ctx_t *ctx);
  int (* suspend)(admwrk_ctx_t *ctx);
  int (* stop)(admwrk_ctx_t *ctx, const stop_data_t *stop_data);
  int (* shutdown)(admwrk_ctx_t *ctx);
  int (* check_down)(admwrk_ctx_t *ctx);
  int (* check_up)(admwrk_ctx_t *ctx);

  /* The following methods should be called on all nodes. */
  int (* diskadd)(admwrk_ctx_t *ctx, struct adm_node *node, struct adm_disk *disk,
		  const char *path);
  /* The diskdel method cannot assume that the adm_disk is still part
   * of the containing adm_node. */
  void (* diskdel)(admwrk_ctx_t *ctx, struct adm_node *node, struct adm_disk *disk);
  int (* nodeadd)(admwrk_ctx_t *ctx, struct adm_node *node);
  void (* nodeadd_commit)(admwrk_ctx_t *ctx, struct adm_node *node);
  /* Before deleting, ask the service if its's allowed */
  int (* check_nodedel)(admwrk_ctx_t *ctx, struct adm_node *node);
  /* The nodedel method cannot assume that the adm_node is still part
   * of the containing adm_cluster. */
  void (* nodedel)(admwrk_ctx_t *ctx, struct adm_node *node);

  struct {
    rpc_command_t id;
    LocalCommand function;
  } local_commands[];
};

extern const struct adm_service adm_service_admin;
extern const struct adm_service adm_service_rdev;
extern const struct adm_service adm_service_nbd;
extern const struct adm_service adm_service_vrt;
extern const struct adm_service adm_service_lum;
#ifdef WITH_FS
extern const struct adm_service adm_service_fs;
#endif
#ifdef WITH_MONITORING
extern const struct adm_service adm_service_monitor;
#endif

extern const struct adm_service *const adm_services[ADM_SERVICE_LAST + 1];


void adm_service_init_command_processing(void);

const char *adm_service_name(adm_service_id_t id);

/* Macros */

#define adm_service_for_each_at(service, a_service) \
  for(service = a_service; \
      service; \
      service = service->id < ADM_SERVICE_LAST ? adm_services[service->id + 1] : NULL \
  )

#define adm_service_for_each(service) \
    adm_service_for_each_at(service, adm_services[ADM_SERVICE_FIRST])

#define adm_service_for_each_reverse(service) \
  for(service = adm_services[ADM_SERVICE_LAST]; \
      service; \
      service = service->id > ADM_SERVICE_FIRST ? adm_services[service->id - 1] : NULL \
  )

#endif /* __ADM_SERVICE_H */
