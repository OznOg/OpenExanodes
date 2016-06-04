/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/rpc.h"
#include "admind/src/adm_workthread.h"
#include "common/include/exa_nodeset.h"
#include "common/include/exa_math.h"
#include "common/include/exa_error.h"
#include "log/include/log.h"
#include "vrt/virtualiseur/include/vrt_client.h"



int vrt_group_sync_sb_versions(admwrk_ctx_t *ctx, struct adm_group *group)
{
    exa_nodeid_t nid;
    sb_serialized_t sb_ser;
    int err;

    sb_version_local_recover(group->sb_version);

    sb_version_serialize(group->sb_version, &sb_ser);

    /* Exchange exports file version number */
    admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_VRT_SB_SYNC, &sb_ser, sizeof(sb_ser));
    while (admwrk_get_bcast(ctx, &nid, &sb_ser, sizeof(sb_ser), &err))
    {
        if (err == -ADMIND_ERR_NODE_DOWN)
            continue;

        sb_version_update_from(group->sb_version, &sb_ser);
    }

    /* After the synchronisation of the sb_versions, they can't be invalid
     * anymore.
     */
    EXA_ASSERT(sb_version_is_valid(group->sb_version));

    return err;
}

int adm_vrt_group_sync_sb(admwrk_ctx_t *ctx, struct adm_group *group)
{
  struct {
    bool group_is_started;
    bool can_write;
    bool have_disk_in_group;
  } info, reply;

  exa_nodeid_t nid;
  bool group_is_started_somewhere = false;
  int ret;
  int barrier_ret = EXA_SUCCESS;
  struct adm_disk *disk;
  int nb_nodes_with_writable_disks = 0;
  int nb_nodes_with_disks_in_group = 0;

  uint64_t old_sb_version, new_sb_version;

  COMPILE_TIME_ASSERT(sizeof(info) <= ADM_MAILBOX_PAYLOAD_PER_NODE);

  /* XXX maybe checking started is useless as administrable => started
   * and !administrable => return */
  info.group_is_started = group->started;
  info.can_write = false;
  info.have_disk_in_group = false;

  adm_group_for_each_disk(group, disk)
  {
      if (disk->node_id == adm_my_id)
      {
          info.have_disk_in_group = true;

          if (disk->up_in_vrt)
              info.can_write = true;
      }
  }

  admwrk_bcast(admwrk_ctx(), EXAMSG_SERVICE_VRT_SB_SYNC, &info, sizeof(info));
  while (admwrk_get_bcast(ctx, &nid, &reply, sizeof(reply), &ret))
  {
    if (ret == -ADMIND_ERR_NODE_DOWN)
    {
      barrier_ret = -ADMIND_ERR_NODE_DOWN;
      continue;
    }

    EXA_ASSERT(ret == EXA_SUCCESS);

    if (reply.can_write)
        nb_nodes_with_writable_disks++;

    if (reply.have_disk_in_group)
        nb_nodes_with_disks_in_group++;

    if (reply.group_is_started)
        group_is_started_somewhere = true;
  }

  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  /* do not write superblocks if the group is stopped on all nodes */
  if (!group_is_started_somewhere)
    return EXA_SUCCESS;

  if (nb_nodes_with_writable_disks < quotient_ceil64(nb_nodes_with_disks_in_group, 2))
      return -VRT_ERR_GROUP_NOT_ADMINISTRABLE;

  old_sb_version = sb_version_get_version(group->sb_version);
  new_sb_version = sb_version_new_version_prepare(group->sb_version);

  if (group->started)
  {
      ret = vrt_client_group_sync_sb(adm_wt_get_localmb(),
                                     &group->uuid, old_sb_version, new_sb_version);

      EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS || ret == -ADMIND_ERR_NODE_DOWN,
                         "Synchronization of superblocks failed for group '%s' "
                         "UUID=" UUID_FMT ": %s (%d)", group->name,
                         UUID_VAL(&group->uuid), exa_error_msg(ret), ret);
  }
  else
      ret = EXA_SUCCESS;

  barrier_ret = admwrk_barrier(ctx, ret, "VRT: Preparing superblocks version");
  if (barrier_ret != EXA_SUCCESS)
    return barrier_ret;

  sb_version_new_version_done(group->sb_version);

  barrier_ret = admwrk_barrier(ctx, EXA_SUCCESS, "VRT: Writing superblocks version");

  /* Commit anyway, If we are here, we are sure that other nodes have done the
   * job too even if they crashed meanwhile */
  sb_version_new_version_commit(group->sb_version);

  return barrier_ret;
}

int vrt_group_suspend_threads_barrier(admwrk_ctx_t *ctx, const exa_uuid_t *group_uuid)
{
    int ret, barrier_ret;

    ret = vrt_client_group_suspend_metadata_and_rebuild(adm_wt_get_localmb(), group_uuid);
    barrier_ret = admwrk_barrier(ctx, ret, "Suspending threads");

    return barrier_ret;
}

int vrt_group_resume_threads_barrier(admwrk_ctx_t *ctx, const exa_uuid_t *group_uuid)
{
    int ret, barrier_ret;

    ret = vrt_client_group_resume_metadata_and_rebuild(adm_wt_get_localmb(), group_uuid);
    barrier_ret = admwrk_barrier(ctx, ret, "Resuming threads");

    return barrier_ret;
}
