/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/*!\file
 * \brief somme common definitions for exanodes. It includes the error
 * message list (as text) and some utility functions.
 */

#include <stdio.h>
#include <stdlib.h>		/* For abort */

#include "common/include/exa_error.h"
#include "os/include/os_error.h"
#include "os/include/strlcpy.h"
#include "common/include/exa_assert.h"

/*
 * Global ERROR Message table (Local structure)
 *
 * Formatting rules. All error message must start with a Capital letter and finish with a dot
 *                   like in a regular sentence.
 *                   Try to be as helpful as you can for the user, for example, by providing guidance
 *                   on what may cause the error or what she could do to fix it.
 */
struct exa_error {
  exa_error_code   error_code;
  char            *error_msg;
};

static struct exa_error exa_error_msg_table[] = {
  {EXA_ERR_DEFAULT,			"Default error code."}, /* Must always be the first error */


					// THE FIRST SET OF ERROR ARE THE ONE RETURNED FROM CLI EXIT TO THE USER
  {VRT_ERR_NOT_ENOUGH_FREE_SC,		"Not enough storage capacity to create or resize the volume."},
  {EXA_ERR_ADM_BUSY,			"Admind is already performing a command."},
  {ADMIND_ERR_NODE_DOWN,		"One node failed during command processing."},
  {ADMIND_WARNING_FORCE_DISABLED,       "Option 'force' is disabled."},

  {EXA_ERR_DUMMY_END_OF_CLI_SECTION,	"========== Dumy error, mark the end of CLI exit error code =========="},

  {EXA_ERR_BAD_INSTALL,                 "Exanodes is not properly installed. Please reinstall."},
  {EXA_ERR_CMD_PARSING,			"Error while parsing a command."},
  {EXA_ERR_OPEN_FILE,			"Error on file open."},
  {EXA_ERR_CLOSE_FILE,			"Error on file close."},
  {EXA_ERR_WRITE_FILE,			"Error on file write."},
  {EXA_ERR_READ_FILE,			"Error on file read."},
  {EXA_ERR_EXEC_FILE,			"Cannot execute file."},
  {EXA_ERR_XML_INIT,			"Failed to initialize an XML tree."},
  {EXA_ERR_XML_GET,			"Failed to find an item in an XML tree."},
  {EXA_ERR_XML_ADD,			"Failed to add a node to an XML tree."},
  {EXA_ERR_XML_PARSE,			"Failed to parse XML."},
  {EXA_ERR_IOCTL,			"Ioctl error."},
  {EXA_ERR_CREATE_SOCKET,		"Socket creation error."},
  {EXA_ERR_CONNECT_SOCKET,              "Socket connection error."},
  {EXA_ERR_INVALID_PARAM,               "Invalid parameter."},
  {EXA_ERR_INVALID_VALUE,               "Invalid value."},
  {EXA_ERR_CANT_GET_MINOR,		"Cannot get a minor number for the volume."},
  {EXA_ERR_NOT_BLOCK_DEV,		"Not a disk (not a Unix block device)."},
  {EXA_ERR_ADMIND_NOCONFIG,             "There is no cluster created."},
  {EXA_ERR_ADMIND_STOPPED,              "Node is stopped."},
  {EXA_ERR_ADMIND_STARTING,             "Node is starting."},
  {EXA_ERR_ADMIND_STARTED,              "Node is started."},
  {EXA_ERR_ADMIND_STOPPING,             "Node is stopping."},
  {EXA_ERR_BAD_PROTOCOL,		"Protocol release does not match."},
  {EXA_ERR_MODULE_OPEN,                 "Failed to open the /dev file associated to module"},
  {EXA_ERR_SERVICE_PARAM_UNKNOWN,	"The service parameter is unknown."},
  {EXA_ERR_LOCK_CREATE,                 "Failed to create lock file"},
  {EXA_ERR_NOT_FOUND,                   "The requested data was not found"},
  {EXA_ERR_THREAD_CREATE,               "Thread creation failed"},
  {EXA_ERR_EXPORT_NOT_FOUND,            "Export not found"},
  {EXA_ERR_EXPORT_WRONG_METHOD,         "Action unavailable with this export method"},

  {NET_ERR_INVALID_IP,                  "Invalid IP address"},
  {NET_ERR_INVALID_HOST,                "Invalid hostname"},

					// ERREURS SPECIFIQUES AU MODULE : DFVIRTUAL

  {VRT_ERR_GROUPNAME_USED,		"The name of the disk group is already used."},
  {VRT_ERR_UNKNOWN_LAYOUT,		"Unknown layout."},
  {VRT_ERR_RDEV_ALREADY_USED,		"Disk already used."},
  {VRT_ERR_NB_RDEVS_IN_GROUP,		"Too many disks in this disk group."},
  {VRT_ERR_RDEV_TOO_SMALL,		"Disks too small to be virtualized."},
  {VRT_ERR_NO_SUCH_RDEV_IN_GROUP,       "The given disk is not part of the disk group."},
  {VRT_ERR_NO_RDEV_IN_GROUP,		"No disk in the disk group."},
  {VRT_ERR_NO_RDEV_TO_READ,		"No disk to read."},
  {VRT_ERR_NOT_ENOUGH_RDEVS,		"Not enough disks for the layout. The layout needs at least 3 disks."},
  {VRT_ERR_OLD_RDEVS_MISSING,           "Some disks needed to start the disk group are missing."},
  {VRT_ERR_GROUP_NOT_STARTED,           "The disk group is not started."},
  {VRT_ERR_GROUP_NOT_STOPPED,           "The disk group is not stopped."},
  {VRT_ERR_GROUP_NOT_ADMINISTRABLE,     "The disk group is not administrable."},
  {VRT_ERR_VOL_NOT_STOPPED_IN_GROUP,    "Impossible to stop a disk group because one of more volumes are not stopped."},
  {VRT_ERR_TOO_MANY_CHUNKS,             "Disk group contains too many chunks. Please increase the chunk_size."},
  {VRT_ERR_DIRTY_ZONE_TOO_SMALL,        "Dirty zone too small."},
  {VRT_ERR_SB_CORRUPTION,		"Superblock corruption."},
  {VRT_ERR_SB_UUID_MISMATCH,		"Superblock contains information for another group."},
  {VRT_ERR_SB_FORMAT,			"Bad superblock format."},
  {VRT_ERR_SB_MAGIC,			"Bad superblock magic."},
  {VRT_ERR_SB_NOT_FOUND,                "Superblock not found."},
  {VRT_ERR_SB_CHECKSUM,                 "Bad superblock checksum."},
  {EXA_ERR_UUID,			"Uuid mismatch."},
  {EXA_ERR_VERSION,			"Invalid version."},
  {VRT_ERR_VOLUMENAME_USED,		"A volume with this name is already used."},
  {VRT_ERR_UNKNOWN_VOLUME_UUID,         "Unknown volume."},
  {VRT_ERR_VOLUME_IS_PRIVATE,		"Trying to start a private volume on more than one node with one of them in read-write mode."},
  {VRT_ERR_VOLUME_NOT_STARTED,          "The volume is not started."},
  {VRT_ERR_VOLUME_NOT_STOPPED,          "The volume is not stopped."},
  {VRT_ERR_VOLUME_NOT_EXPORTED,         "The volume is not exported."},
  {VRT_ERR_RDEV_DOWN,			"Some devices are down."},
  {VRT_ERR_CANT_DGDISKRECOVER,		"The disk to be replaced is not BROKEN nor MISSING."},
  {VRT_ERR_GROUP_FRAGMENTATION,		"Disk group fragmentation."},
  {VRT_ERR_GROUP_TOO_SMALL,		"Disk group too small."},
  {VRT_ERR_DISK_REPLACEMENT_NOT_SUPPORTED, "The group does not support disk replacement."},
  {VRT_ERR_CANT_REBUILD,	        "Rebuild is not possible on an OFFLINE group."},
  {VRT_ERR_GROUP_OFFLINE,		"The group is offline."},
  {VRT_ERR_VOLUME_ALREADY_STARTED,	"The volume is already started."},
  {VRT_ERR_VOLUME_ALREADY_EXPORTED,     "The volume is already exported."},
  {VRT_ERR_UNKNOWN_GROUP_UUID,		"Unknown disk group UUID."},
  {VRT_ERR_NB_VOLUMES_CREATED,		"Too many volumes created."},
  {VRT_ERR_NB_VOLUMES_STARTED,          "Too many volumes started."},
  {VRT_ERR_VOLUME_IS_IN_USE,		"You are trying to stop a volume in use."},
  {VRT_WARN_GROUP_OFFLINE,		"The group is offline."},
  {VRT_ERR_PREVENT_GROUP_OFFLINE,       "Cannot stop so many nodes because some disk groups may go offline."},
  {VRT_ERR_CANNOT_DELETE_NODE,          "A volume is still started on this node."},
  {VRT_ERR_LAYOUT_CONSTRAINTS_INFRINGED,"Layout constraints infringed." },
  {VRT_ERR_REBUILD_INTERRUPTED,         "Rebuild interrupted." },
  {VRT_INFO_GROUP_ALREADY_STARTED,      "The disk group is already started."},
  {VRT_INFO_GROUP_ALREADY_STOPPED,      "The disk group is already stopped."},
  {VRT_INFO_VOLUME_ALREADY_STARTED,     "The volume is already started on some nodes."},
  {VRT_INFO_VOLUME_ALREADY_STOPPED,     "The volume is already stopped on some nodes."},
  {VRT_INFO_VOLUME_ALREADY_EXPORTED,    "The volume is already exported on some nodes."},
  {VRT_ERR_UNKNOWN_DISK_UUID,           "Unknown disk UUID."},
  {VRT_ERR_LAYOUT_UNKNOWN_OPERATION,    "The layout cannot treat this operation."},
  {VRT_ERR_DISK_NOT_LOCAL,              "The disk is not local to this instance of VRT."},

					// ERREURS SPECIFIQUES AU MODULE : NBD

  {NBD_ERR_SERVERD_INIT,                "Cannot initialize NBD server daemon."},
  {NBD_ERR_NB_SNODES_CREATED,		"Too many nodes imported."},
  {NBD_ERR_UNKNOWN_SNODENAME,		"Unknown remote node."},
  {NBD_ERR_NB_RDEVS_CREATED,		"Too many managed disks."},
  {NBD_ERR_CANT_GET_MINOR,              "Cannot get a major/minor number for the device."},
  {NBD_ERR_UNKNOWN_NET,                 "Unknown network type."},
  {NBD_ERR_EXISTING_SESSION,            "Network connection already established."},
  {NBD_ERR_CLOSED_SESSION,              "Network connection already closed."},
  {NBD_ERR_NO_SESSION,                  "No network connection established."},
  {NBD_ERR_SERVER_REFUSED_CONNECTION,   "Failed to connect to NBD server."},
  {NBD_ERR_SNODE_NET_CLEANUP,           "Cannot close network connection."},
  {NBD_ERR_THREAD_CREATION,             "Cannot create thread."},
  {NBD_ERR_MALLOC_FAILED,               "Memory allocation failed."},
  {NBD_ERR_BAD_STATE,                   "Can not set the state ."},
  {NBD_ERR_NO_CONNECTION,               "No connection with remote client."},
  {NBD_ERR_MOD_SESSION,                 "Cannot establish session with Bd module."},
  {NBD_ERR_SET_SIZE,                    "Cannot set the size of the network device."},
  {NBD_ERR_REMOVE_NDEV,                 "Cannot remove ndev from the nbd module."},
  {NBD_ERR_EXISTING_CLIENT,             "Client node already added."},
  {NBD_ERR_CLIENTS_NUMBER_EXCEEDED,     "Maximum number of clients exceeded."},
  {NBD_WARN_EXPORT_FAILED,              "Failed to export one or more disks."},
  {NBD_WARN_OPEN_SESSION_FAILED,        "Failed to connect to one or more NBD servers."},

  {FS_ERR_INVALID_FILESYSTEM,           "This file system is in an invalid state. Please delete and re-create it."},
  {FS_ERR_MOUNTPOINT_USED,              "Mountpoint already used."},
  {FS_ERR_MOUNT_ERROR,                  "Failed to mount the file system."},
  {FS_ERR_NB_LOGS_EXCEEDED,             "Not enough logs in the Seanodes FS file system. Number of nodes concurrently mounted exceeds the file system capacity"},
  {FS_ERR_INVALID_GULM_MASTERS_LIST,    "Invalid list of Gulm masters : it must contain 1, 3 or 5 nodes." },
  {FS_ERR_INVALID_MOUNTPOINT,           "Mount point exists and is not a directory."},
  {FS_ERR_IMPOSSIBLE_MOUNTPOINT,        "Impossible to access or create mount point."},
  {FS_WARN_MOUNTED_READONLY,            "Mounting succeeded, but the volume is DOWN. It is mounted in Read-Only mode."},
  {FS_ERR_LOAD_MODULE,                  "Problems loading kernel modules. Check your installation."},
  {FS_ERR_FSCK_ERROR,                   "Problem executing fsck program, it crashed."},
  {FS_ERR_EXECUTION_ERROR,              "Problems executing third-party program. Check your installation."},
  {FS_ERR_TIME_OUT,                     "Time-out problems while running the daemons. Check your installation, and name resolution."},
  {FS_ERR_MKFS_ERROR,                   "File system formatting failed."},
  {FS_ERR_MKFS_SIZE_ERROR,              "File system formatting failed: size too small."},
  {FS_ERR_UMOUNT_ERROR,                 "Failed to unmount the file system. Please check it is not in use."},
  {FS_ERR_RESIZE_NEED_MOUNT_RW,         "File system must be mounted read-write somewhere to resize it."},
  {FS_ERR_RESIZE_NEED_UMOUNT,           "File system must be unmounted to resize."},
  {FS_ERR_RESIZE_SHRINK_NOT_POSSIBLE,   "File system cannot be shrinked."},
  {FS_ERR_RESIZE_NOT_ENOUGH_SPACE,      "File system cannot be shrinked to this size."},
  {FS_ERR_STOP_WITH_VOLUME_DOWN,        "Trying to stop a volume which is DOWN."},
  {FS_ERR_CHECK_NODE_DOWN,              "The node on which check should run is down."},
  {FS_ERR_CHECK_ERRORS_CORRECTED,       "There were file system errors, they were corrected."},
  {FS_ERR_CHECK_ERRORS,                 "There were file system errors, they are left uncorrected."},
  {FS_ERR_CHECK_OPERATIONAL_ERROR,      "Operational errors while running checking program."},
  {FS_ERR_CHECK_USAGE_ERROR,            "Usage errors. You may have given an invalid parameters string."},
  {FS_INFO_ALREADY_STARTED,             "The file system is already started on some nodes." },
  {FS_INFO_ALREADY_STOPPED,             "The file system is already stopped." },
  {FS_ERR_HANDLE_GFS,                   "The group holding the Seanodes FS file system got offline. You need to reboot the cluster to manage the Seanodes FS file system again." },
  {FS_ERR_LOCAL_RW_ONE_NODE,            "A local file system in read/write mode cannot be started on several nodes at a time."},
  {FS_ERR_RW_TO_RO,                     "The file system is already mounted in read/write mode. It cannot be changed to read-only."},
  {FS_ERR_RO_TO_RW,                     "The file system is already mounted in read-only mode. It cannot be changed to read/write."},
  {FS_ERR_CHANGE_MOUNTPOINT,            "This file system is already started. Mount point cannot be changed on a started file system."},
  {FS_ERR_FSCK_STARTED,                 "Cannot run fsck on a started file system"},
  {FS_ERR_UNKNOWN_TUNE_OPTION,          "Unknown option name to tune"},
  {FS_ERR_LESS_LOGS,                    "Unable to reduce the number of logs of a file system"},
  {FS_ERR_INCREASE_LOGS,                "Unknown error while increasing the number of logs"},
  {FS_ERR_INCREASE_LOGS_NEED_MOUNT_RW,  "File system must be mounted RW somewhere to add logs."},
  {FS_ERR_FORMAT_MOUNT_OPTION,          "Mount option must respect the regular expression '[a-zA-Z0-9,=]*' "},
  {FS_ERR_MOUNT_OPTION_NEEDS_FS_STOPPED,"Mount option cannot be changed on a started file system."},
  {FS_ERR_GFS_CANNOT_DELETE_MASTER,     "Cannot delete a node with a running SeanodesFS."},
  {FS_ERR_GFS_MUST_CHANGE_MASTER,       "Change the list of Seanodes FS masters before deleting one of them."},

  {CMD_EXP_ERR_OPEN_DEVICE,		"Cannot open the disk."},
  {CMD_EXP_ERR_CLOSE_DEVICE,		"Cannot close the disk."},
  {CMD_EXP_ERR_UNKNOWN_DEVICE,		"The disk is not managed by the serverd or clientd daemon."},

					// ERROR RELATED TO RESETTING THE SBG FROM ADMIND

  {CMD_STR_RESULT_IS_TOO_LARGE,         "String result of command is too big for the buffer size."},

					// ERROR RELATED TO ADMIND
  {ADMIND_ERR_ALREADY_RUNNING,          "Admind is already running."},
  {ADMIND_ERR_MODULESTART,		"Failed to start Exanodes' modules."},
  {ADMIND_ERR_MODULESTOP,		"Failed to stop Exanodes' modules."},
  {ADMIN_ERR_FSDSTART,		        "Failed to start fsd."},
  {ADMIN_ERR_FSDSTOP,		        "Failed to stop fsd."},
  {ADMIN_ERR_CSUPDSTOP,			"Failed to stop supervisor."},
  {ADMIN_ERR_CSUPDSTART,		"Failed to start supervisor."},
  {ADMIND_ERR_EXAMSGDSTART,		"Failed to start exa_msgd (check the network interface is valid)."},
  {ADMIND_ERR_EXAMSGDSTOP,		"Failed to stop exa_msgd."},
  {ADMIND_ERR_NOTHINGTODO,		"Nothing to do."},
  {ADMIND_ERR_NOTLEADER,		"We do not accept the command because we are not the leader."},
  {ADMIND_ERR_INRECOVERY,		"Cannot accept this command while a recovery is in progress."},
  {ADMIND_ERR_QUORUM_TIMEOUT,		"Timeout while trying to obtain a quorum."},
  {ADMIND_ERR_QUORUM_PRESERVE,		"You cannot stop so many nodes due to the quorum constraint."},
  {ADMIND_WARNING_NODE_IS_DOWN,         "Node is not part of the cluster."},
  {ADMIND_ERR_VOLUME_IN_FS,		"You cannot manage this volume because it is part of a file system. Please use an exa_fs* command instead."},
  {ADMIND_ERR_GROUP_NOT_EMPTY,		"The disk group still contains one or more volume."},
  {ADMIND_ERR_CLUSTER_NOT_EMPTY,	"The cluster still contains one or more disk group."},
  {ADMIND_ERR_CONFIG_LOAD,		"Failed to load configuration file."},
  {ADMIND_ERR_UNKNOWN_NODENAME,		"Unknown hostname."},
  {ADMIND_ERR_UNKNOWN_GROUPNAME,	"Unknown disk group name."},
  {ADMIND_ERR_UNKNOWN_DISK,		"Unknown disk."},
  {ADMIND_ERR_UNKNOWN_FSNAME,		"Unknown file system name."},
  {ADMIND_ERR_UNKNOWN_NICNAME,		"Unknown network interface name."},
  {ADMIND_ERR_TOO_MANY_DISKS,           "There are too many disks in the cluster"},
  {ADMIND_ERR_TOO_MANY_DISKS_IN_NODE,   "There are too many disks in the node"},
  {ADMIND_ERR_TOO_MANY_DISKS_IN_GROUP,  "There are too many disks in the group"},
  {ADMIND_ERR_DISK_ALREADY_ASSIGNED,    "Disk already assigned to a group"},
  {ADMIND_ERR_CLUSTER_ALREADY_CREATED,	"A cluster is already created."},
  {ADMIND_ERR_GROUP_ALREADY_CREATED,	"The disk group is already created."},
  {ADMIND_ERR_VOLUME_ALREADY_CREATED,	"The logical volume is already created."},
  {ADMIND_ERR_UNKNOWN_VOLUMENAME,	"Unknown logical volume name."},
  {ADMIND_ERR_METADATA_CORRUPTION,	"A serious failure occurred during command processing."},
  {ADMIND_ERR_RESOURCE_IS_INVALID, 	"At least one resource is invalid. You should delete it with the --metadata-recovery option."},
  {ADMIND_ERR_VOLUME_ACCESS_MODE,	"Cannot change volume's access mode."},
  {ADMIND_WARN_DISK_RECOVER,            "Timeout while waiting for the disk to be up."},
  {ADMIND_ERR_LICENSE,                  "A problem related to the license occurred."},
  {ADMIND_WARN_LICENSE,                 "A problem related to the license occurred."},
  {ADMIND_ERR_INIT_SSL,                 "Failed to initialize OpenSSL."},
  {ADMIND_ERR_DISK_ALIEN,               "The disk does not contain an Exanodes superblock."},
  {ADMIND_ERR_UNKNOWN_DISK_UUID,	"Unknown disk UUID."},
  {ADMIND_ERR_WRONG_DISK_STATUS,	"Wrong disk status."},
  {ADMIND_ERR_MOVED_DISK,		"The disk moved from one node to another."},
  {ADMIND_ERR_VOLUME_AS_DISK,           "Cannot use an Exanodes volume as an Exanodes disk."},
  {ADMIND_ERR_CREATE_DIR,               "Can not create directory"},
  {ADMIND_ERR_REMOVE_DIR,               "Can not remove directory"},
  {ADMIND_WARN_TOKEN_MANAGER_DISCONNECTED,"Connection to the token manager failed"},
  {ADMIND_ERR_INVALID_TOKEN_MANAGER,    "Invalid token manager"},
  {ADMIND_ERR_TM_TOO_MANY_NODES,        "Token manager configured, but cluster has more than two nodes."},
  {ADMIND_ERR_VLTUNE_NON_APPLICABLE,    "Command not applicable for this parameter"},

  					// ERROR RELATED TO EXA_RDEV
  {RDEV_ERR_INVALID_DEVICE,		"Invalid real device"},
  {RDEV_ERR_INVALID_BUFFER,		"Invalid real device buffer"},
  {RDEV_ERR_INVALID_OFFSET,		"Invalid real device offset"},
  {RDEV_ERR_INVALID_SIZE,		"Invalid real device size"},
  {RDEV_ERR_BIG_ERROR,			"Big error in exa_rdev"},
  {RDEV_ERR_NOT_SOCKET,			"Cannot select on a non-socket file descriptor"},
  {RDEV_ERR_NOT_ENOUGH_RESOURCES,       "Not enough resources to process this request"},
  {RDEV_ERR_UNKNOWN,			"Real device: error"},
  {RDEV_ERR_TOO_SMALL,			"Real device is too small"},
  {RDEV_ERR_NOT_INITED,			"Real device service is not initialized"},
  {RDEV_ERR_INVALID_IOCTL,		"Real device: invalid ioctl"},
  {RDEV_ERR_NOT_OPEN,			"File is not opened"},
  {RDEV_ERR_REQUEST_TOO_BIG,		"Request too big"},
  {RDEV_ERR_NOT_ENOUGH_MEMORY,		"Not enough memory"},
  {RDEV_ERR_CANT_OPEN_DEVICE,           "Cannot open device"},
  {RDEV_ERR_DISK_NOT_AVAILABLE,         "Disk is not in the list of available disks"},
  {RDEV_ERR_DUPLICATE_ID,               "Cannot create an exa_rdev with this ID, it's already used"},
  {RDEV_WARN_IO_BARRIERS,               "Some disks do not handle IO barriers. IO barriers should be disabled with exa_cltune"},
  {RDEV_ERR_INVALID_BROKEN_DISK_TABLE,  "Invalid broken disk table"},
  {MD_ERR_START,                        "Failed to start monitoring service."},
  {MD_ERR_STOP,                         "Failed to stop monitoring service."},
  {MD_ERR_STATUS,                       "Failed to get monitoring status."},
  {MD_ERR_AGENTX_NOT_ALIVE,             "Agentx has been detected down"},
  {LUN_ERR_INVALID_VALUE,               "Invalid LUN. The valid range is 0..255"},
  {LUN_ERR_ALREADY_ASSIGNED,            "LUN already assigned to another volume"},
  {LUN_ERR_NO_LUN_AVAILABLE,            "There is no more LUN available"},
  {LUN_ERR_LUN_BUSY,                    "The requested LUN is busy"},
  {VRT_WARN_DEV_ENTRY_REMOVAL,          "Failed to remove /dev entry"},

					// LUM ERRORS

  {LUM_ERR_INVALID_EXPORT,                  "Invalid export"},
  {LUM_ERR_INVALID_LUN,                     "Invalid LUN"},
  {LUM_ERR_INVALID_IQN,                     "Invalid IQN"},
  {LUM_ERR_INVALID_IQN_FILTER,              "Invalid IQN filter"},
  {LUM_ERR_INVALID_IQN_FILTER_POLICY,       "Invalid IQN filter policy"},
  {LUM_ERR_DUPLICATE_IQN_FILTER,            "Duplicate IQN filter"},
  {LUM_ERR_IQN_FILTER_NOT_FOUND,            "IQN filter not found"},
  {LUM_ERR_TOO_MANY_IQN_FILTERS,            "Too many IQN filters"},
  {LUM_ERR_INVALID_TARGET_CONFIG_FILE,      "Invalid target configuration file"},
  {LUM_ERR_INVALID_TARGET_CONFIG_PARAM,     "Invalid parameter in target configuration file"},
  {LUM_ERR_UNKNOWN_TARGET_CONFIG_PARAM,     "Unknown parameter in target configuration file"},

					// ALWAYS LAST MESSAGE

  {EXA_LAST_ERROR,			"*ALWAYS LAST MESSAGE IN THIS LIST*"}

};

/* Given an error code, return the error message
 * Return a default message if the message in not in the
 * exa_error_msg_table
 */
const char *exa_error_msg(exa_error_code error_code)
{
  int i = 0;

  if (error_code <= 0)
    error_code = -error_code;

  /* If the error code is bellow our first error, then return the perror */
  if(error_code < EXA_ERR_DEFAULT)
    return os_strerror(error_code);

  while (exa_error_msg_table[i].error_code != EXA_LAST_ERROR)
  {
    if(exa_error_msg_table[i].error_code == error_code)
      return exa_error_msg_table[i].error_msg;
    i++;
  }

  /* Warning, do not return a message that we have in the error table
   * or exa_errorconv.pm will get back the wrong error code enum */
  return "There is no string definition for this error code";
}

/** Given an error code (must be positive), return the error type
 *
 * \param[in] err: the error code to analyse.
 *
 * \return an exa_error_type which gives an indication on the error code criticity.
 * The returned value is one of EXA_SUCCESS ERR_TYPE_INFO ERR_TYPE_ERROR.
 *
 */
exa_error_type get_error_type(exa_error_code err)
{

  EXA_ASSERT_VERBOSE(err>=0, "Error code must be positive or 0 (received %d)", err);

  switch (err)
    {
      case EXA_SUCCESS:
      case ADMIND_ERR_NOTHINGTODO:
        return ERR_TYPE_SUCCESS;

      case ADMIND_WARNING_FORCE_DISABLED:
      case ADMIND_WARNING_NODE_IS_DOWN:
      case FS_WARN_MOUNTED_READONLY:
      case VRT_WARN_GROUP_OFFLINE:
      case NBD_WARN_EXPORT_FAILED:
      case ADMIND_WARN_DISK_RECOVER:
      case RDEV_WARN_IO_BARRIERS:
      case VRT_WARN_DEV_ENTRY_REMOVAL:
      case ADMIND_WARN_LICENSE:
      case ADMIND_WARN_TOKEN_MANAGER_DISCONNECTED:
	return ERR_TYPE_WARNING;

      case FS_INFO_ALREADY_STOPPED:
      case FS_INFO_ALREADY_STARTED:
      case VRT_INFO_GROUP_ALREADY_STARTED:
      case VRT_INFO_GROUP_ALREADY_STOPPED:
      case VRT_INFO_VOLUME_ALREADY_STARTED:
      case VRT_INFO_VOLUME_ALREADY_STOPPED:
        return ERR_TYPE_INFO;

      default:
        return ERR_TYPE_ERROR;
    }
}
