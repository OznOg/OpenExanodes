/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __GENERIC_FS__H
#define __GENERIC_FS__H

#include <libxml/tree.h>

#include "common/include/exa_error.h"
#include "admind/src/admind.h"
#include "fs/include/fs_data.h"
#include "admind/src/commands/tunelist.h"

struct adm_fs;
struct adm_volume;
struct adm_node;

/** FS infos request */
typedef struct {
  char mountpoint [EXA_MAXSIZE_MOUNTPOINT + 1];          /**< /mnt/gfs */
  char devpath [EXA_MAXSIZE_DEVPATH + 1];             /**< /dev/exa/group/gfs */
  exa_nodeset_t nodeliststatfs;                   /**< Do we get all sizes on file system (statfs call) ? */
} fs_request_t;

/**
 * Definition of a filesystem type (GFS or localfilesystem)
 *
 * Each funtion pointer can have theses parameters:
 * - int thr_nb: numero of the thread
 * - const char* node_set: set of nodes (regexp)
 * - fs_data_t fs: pointer to the filesystem definition structure
 * - const char *config_path: directory where config files are (*.ccs for GFS)
 * */
typedef struct fs_definition
{
  /** List of supported filesystems (example: ext3, reiserfs, NULL) */
  const char** type_name_list;

  /** Check before start */
  exa_error_code  (*check_before_start)(int thr_nb, fs_data_t* fs);

  /** Check the filesystem can be stopped on the 'node_set' nodes */
  exa_error_code  (*check_before_stop)(int thr_nb, const exa_nodeset_t* node_set,
				       fs_data_t* fs, bool force);

  /** Parse the filesystem-specific creation parameters, and fill fs
      fs_data_t structure accordingly */
  int (*parse_fscreate_parameters)(fs_data_t *fs, int data, uint64_t rg_size);

    /** Perform clustered preparation before really creating the filesystem */
  exa_error_code (*pre_create_fs)(int thr_nb, fs_data_t* fs);

  /** Really create the filesystem (mkfs) and associated volumes */
  exa_error_code  (*create_fs)(int thr_nb, fs_data_t* fs);

  /** Mount the filesystem on the specified nodes.
   - node_set = list of nodes mounted RW
   - node_set_read_only = list of nodes mounted RO.
   These 2 lists can be unvoid at the same time during recovery, or exclusively unvoid
   at command time. The underlying volume will be RO/RW and it mounts RO if necessary.
   start_succeeded is filled with the full list of nodes that succeeded to mount (RO or RW) */
  exa_error_code  (*start_fs)(int thr_nb, const exa_nodeset_t* node_set,
			      const exa_nodeset_t* node_set_read_only,
			      fs_data_t* fs, exa_nodeset_t* start_succeeded,
			      int recovery);

  /** Called by a clustered FS, after preparing succeeded. */
  void (*post_prepare)();

  /** Called by a clustered FS, before unloading, always. */
  void (*pre_unload)();

  /** When node is stopped. Do specific necessary clean-up (unload daemons, modules) */
  exa_error_code (*node_stop)(int thr_nb, fs_data_t* fs);

  /** When node is added, check it is allowed. */
  exa_error_code (*node_add)(int thr_nb, struct adm_node *node);

  /** When node is added. (update /etc/cluster/cluster.conf, for example) */
  void (*node_add_commit)(int thr_nb, struct adm_node *node);

  /** Umount the filesystem on the specified nodes */
  exa_error_code  (*stop_fs)(int thr_nb, const exa_nodeset_t* node_set,
                             fs_data_t* fs, bool force,
                             adm_goal_change_t goal_change,
                             exa_nodeset_t* stop_succeeded);

  /** Resize the filesystem */
  exa_error_code  (*resize_fs)(int thr_nb, fs_data_t* fs, int64_t sizeKB);

  /** Check filesystem */
  exa_error_code  (*check_fs)(int thr_nb, fs_data_t* fs, const char* optional_parameter,
			      exa_nodeid_t node_where_to_check, bool repair);

  /** Tune filesystem */
  exa_error_code  (*tune)(int thr_nb, fs_data_t* fs,
			  const char* parameter, const char* value);

  /** Get one tuning parameter for a filesystem. Fill name with next, unless none is found.
   If none is found (it was the last one), return false, unless return true. If an error
   occurred, return also false and fill the error code */
  bool (*gettune)(int thr_nb, fs_data_t* fs,
			struct tune_t* tune, int* error);

  /** Fill from config to a structure */
  void (*fill_data_from_config)(fs_data_t* struct_fs, struct adm_fs *fs);

  /** Recovery function : This is called for every running file system instance,
      i.e, goal_started!="".  */
  exa_error_code (*specific_fs_recovery)(int thr_nb, fs_data_t* fs);

  /* Create with a private or shared volume ? */
  bool (*is_volume_private)();

} fs_definition_t;

const fs_definition_t* fs_get_definition(const char* name);
const fs_definition_t* fs_iterate_over_fs_definition(const fs_definition_t* current);

void             fs_fill_data_from_xml(fs_data_t* generic_fs, xmlNodePtr fs);
void             fs_fill_data_from_config(fs_data_t* generic_fs, struct adm_fs *fs);

exa_error_code     fs_data_device_create(int thr_nb, fs_data_t* fs,
					 uint64_t log_sizeKB, int private);
exa_error_code     fs_data_device_start(int thr_nb, fs_data_t* fs,
					const exa_nodeset_t* list,
					uint32_t readonly);
exa_error_code     fs_data_device_stop(int thr_nb, fs_data_t* fs,
				       const exa_nodeset_t* list,
                                       bool force,
                                       adm_goal_change_t goal_change);
exa_error_code     fs_data_device_delete(int thr_nb, fs_data_t* fs,
					 bool metadata_recovery);
int                fs_get_data_device_offline(int thr_nb, fs_data_t* fs);
int                fs_get_data_device_status_changed(int thr_nb, fs_data_t* fs);
struct adm_volume* fs_get_volume(fs_data_t* fs);
const char*        fs_get_name(fs_data_t* fs);

/* Some commands are common to all FS types, (ie fsck) */
exa_error_code   generic_fs_get_started_nodes(int thr_nb, fs_data_t* fs_to_test,
					      exa_nodeset_t* nodes_list,
					      exa_nodeset_t* nodes_list_ro);
exa_error_code   generic_fs_check(int thr_nb, fs_data_t* fs_to_test,
				  const char* optional_parameters,
				  exa_nodeid_t node_where_to_check,
				  bool repair);
void             generic_fs_check_local(int thr_nb, void *msg);
exa_error_code   generic_fs_check_before_stop(int thr_nb, const exa_nodeset_t* nodes,
					      fs_data_t* fs,
					      bool force);
exa_error_code   generic_fs_tune(int thr_nb, fs_data_t* fs,
				 const char* parameter, const char* value);
bool       generic_fs_gettune(int thr_nb, fs_data_t* fs,
				    struct tune_t* tune, int* error);
exa_error_code   generic_fs_mounted_grow(int thr_nb, fs_data_t* fs_to_grow,
					 int64_t new_size);

/** getting info of a mountpoint */
typedef struct mountpointused_info {
  char mountpoint[EXA_MAXSIZE_MOUNTPOINT+1];     /**< /mnt/ext */
  exa_nodeset_t nodes;                        /**< Which node to test */
} mountpointused_info_t;

/** Running a local action on a FS : type of action */
typedef enum startstop_action_t {
  startstop_action_mount,
  startstop_action_umount,
  startstop_action_prepare,
  startstop_action_unload
} startstop_action_t;

/** Running a local action on a FS : action message */
typedef struct startstop_info_t {
  int64_t action;         /**< Cast to startstop_action_t */
  int64_t read_only;      /**< should be mounted read-only ? */
  int64_t recovery;       /**< Is this a recovery ? This changes the flags given to fsd **/
  fs_data_t fs;           /**< The definition of the FS on which the action applies */
  exa_nodeset_t nodes;    /**< List on which the action happens */
} startstop_info_t;

#endif /* __GENERIC_FS__H */

