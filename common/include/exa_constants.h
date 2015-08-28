/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_CONSTANTS_H
#define _EXA_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

#define EXA_COPYRIGHT "Copyright 2002, 2011 Seanodes Ltd. All rights reserved."

/*
 * general constants
 * -----------------
 */

#define EXA_MAX_NODES_NUMBER    128

#define EXA_MAXSIZE_BUFFER		1023
#define EXA_MAXSIZE_LINE		1023

/** Boolean */
#ifndef __cplusplus
/*
 * In adm_license.c we use openssl library that defines TRUE and FALSE as
 * macros so we have to use the same way to define them. The ifndef prevents
 * redefinition errors. Using an enum would lead to a compilation error since
 * enum {TRUE, FALSE} would be replaced by enum {0, 1} by the preprocessor.
 */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
typedef int exa_bool_t;
#endif

/* WARNING: constants below are used in many places in the code and
 * especially in superblocks. In order to have 64 bit aligned buffers,
 * please make sure the value you set is of the form: A = (8 * B) -1 where B
 * is an integer.
 */

/** Max node name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_NODENAME	63
#if (EXA_MAXSIZE_NODENAME+1) & 0x7
# error "EXA_MAXSIZE_NODENAME+1 is not a multiple of 8 bytes"
#endif

/** Max host name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_HOSTNAME	63
#if (EXA_MAXSIZE_HOSTNAME+1) & 0x7
# error "EXA_MAXSIZE_HOSTNAME+1 is not a multiple of 8 bytes"
#endif

/** Max host list length \b without the trailing '\\0' */
#define EXA_MAXSIZE_HOSTSLIST (EXA_MAX_NODES_NUMBER * (EXA_MAXSIZE_HOSTNAME + 1) - 1) /* NOT_IN_PERL */

/** Max cluster name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_CLUSTERNAME	15
#if (EXA_MAXSIZE_CLUSTERNAME+1) & 0x7
# error "EXA_MAXSIZE_CLUSTERNAME+1 is not a multiple of 8 bytes"
#endif

/** Max licensee name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_LICENSEE 63
#if (EXA_MAXSIZE_LICENSEE+1) & 0x7
# error "EXA_MAXSIZE_LICENSEE+1 is not a multiple of 8 bytes"
#endif

/** Max group name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_GROUPNAME	15
#if (EXA_MAXSIZE_GROUPNAME+1) & 0x7
# error "EXA_MAXSIZE_GROUPNAME+1 is not a multiple of 8 bytes"
#endif

/** Max network card ip adress length \b without the trailing '\\0' */
#define EXA_MAXSIZE_NICADDRESS	15
#if (EXA_MAXSIZE_NICADDRESS+1) & 0x7
# error "EXA_MAXSIZE_NICADDRESS+1 is not a multiple of 8 bytes"
#endif

/** Max volume name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_VOLUMENAME	15
#if (EXA_MAXSIZE_VOLUMENAME+1) & 0x7
# error "EXA_MAXSIZE_VOLUMENAME+1 is not a multiple of 8 bytes"
#endif

/** Max layout name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_LAYOUTNAME	15
#if (EXA_MAXSIZE_LAYOUTNAME+1) & 0x7
# error "EXA_MAXSIZE_LAYOUTNAME+1 is not a multiple of 8 bytes"
#endif

/** Max export method length \b without the trailing '\\0' */
#define EXA_MAXSIZE_EXPORT_METHOD 15
#if (EXA_MAXSIZE_EXPORT_METHOD+1) & 0x7
# error "EXA_MAXSIZE_EXPORT_METHOD+1 is not a multiple of 8 bytes"
#endif

/** Max export option length \b without the trailing '\\0' */
#define EXA_MAXSIZE_EXPORT_OPTION 127
#if (EXA_MAXSIZE_EXPORT_OPTION+1) & 0x7
# error "EXA_MAXSIZE_EXPORT_OPTION+1 is not a multiple of 8 bytes"
#endif

#ifdef WITH_FS

/** Max filesystem type length \b without the trailing '\\0' */
#define EXA_MAXSIZE_FSTYPE	15
#if (EXA_MAXSIZE_FSTYPE+1) & 0x7
# error "EXA_MAXSIZE_FSTYPE+1 is not a multiple of 8 bytes"
#endif


/** Max GFS lock protocol name length (lock_gulm/lock_dlm) \b without the trailing '\\0' */
#define EXA_MAXSIZE_GFS_LOCK_PROTO	15
#if (EXA_MAXSIZE_GFS_LOCK_PROTO+1) & 0x7
# error "EXA_MAXSIZE_GFS_LOCK_PROTO+1 is not a multiple of 8 bytes"
#endif

/** Max option string length () \b without the trailing '\\0' */
#define EXA_MAXSIZE_MOUNT_OPTION	31
#if (EXA_MAXSIZE_GFS_LOCK_PROTO+1) & 0x7
# error "EXA_MAXSIZE_GFS_LOCK_PROTO+1 is not a multiple of 8 bytes"
#endif

/** Max filesystem check parameters length \b without the trailing '\\0' */
#define EXA_MAXSIZE_FSCHECK_PARAMETER	63
#if (EXA_MAXSIZE_FSCHECK_PARAMETER+1) & 0x7
# error "EXA_MAXSIZE_FSCHECK_PARAMETER+1 is not a multiple of 8 bytes"
#endif

#endif /* WITH_FS */

/** Max /dev path length (/dev/sda/) \b without the trailing '\\0' */
#define EXA_MAXSIZE_DEVPATH             127
#if (EXA_MAXSIZE_DEVPATH+1) & 0x7
# error "EXA_MAXSIZE_DEVPATH+1 is not a multiple of 8 bytes"
#endif

/** Max mountpoint path name length (/mnt/ext3) \b without the trailing '\\0' */
#define EXA_MAXSIZE_MOUNTPOINT		127
#if (EXA_MAXSIZE_MOUNTPOINT+1) & 0x7
# error "EXA_MAXSIZE_MOUNTPOINT+1 is not a multiple of 8 bytes"
#endif

/** Max service name length \b without the trailing '\\0' */
#define EXA_MAXSIZE_SERVICENAME		15
#if (EXA_MAXSIZE_SERVICENAME+1) & 0x7
# error "EXA_MAXSIZE_SERVICENAME+1 is not a multiple of 8 bytes"
#endif

/* Max regular expansion possible. if using many nodes with very different
 * names, it may be impossible to handle the list as a small regexp
 */
#define EXA_MAXSIZE_REGEXP	511
#if (EXA_MAXSIZE_REGEXP+1) & 0x7
# error "EXA_MAXSIZE_REGEXP+1 is not a multiple of 8 bytes"
#endif
#if (EXA_MAXSIZE_REGEXP) < (EXA_MAXSIZE_HOSTNAME)
# error "EXA_MAXSIZE_REGEXP is probably too small compared to EXA_MAXSIZE_HOSTNAME"
#endif

#define EXA_MAX_NUM_EXPORTS		256

#define EXA_MAXSIZE_IQN_FILTER_LIST	32

/* Alphabetical characters, for use in regexps */
#define ALPHA_RANGE_EXPANDED  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define ALPHA_RANGE           "A-Za-z"

/* Decimal digits, for use in regexps */
#define DEC_RANGE_EXPANDED  "0123456789"
#define DEC_RANGE           "0-9"

/* Hexadecimal digits, for use in regexps */
#define HEX_RANGE_EXPANDED  DEC_RANGE_EXPANDED "ABCDEFabcdef"
#define HEX_RANGE           DEC_RANGE "A-Fa-f"

/* Warning, when you want the '-', always put it at the end of the range */
#define EXACONFIG_EXANAME_REGEXP_EXPANDED     ALPHA_RANGE_EXPANDED DEC_RANGE_EXPANDED "_.-" /* NOT_IN_PERL */
#define EXACONFIG_EXANAME_REGEXP              "[" ALPHA_RANGE DEC_RANGE "_.-]" /* NOT_IN_PERL */

#define EXACONFIG_POSIXNAME_REGEXP_EXPANDED   EXACONFIG_EXANAME_REGEXP_EXPANDED /* NOT_IN_PERL */
#define EXACONFIG_POSIXNAME_REGEXP            EXACONFIG_EXANAME_REGEXP /* NOT_IN_PERL */

#define EXACONFIG_MOUNTPOINT_REGEXP_EXPANDED  EXACONFIG_POSIXNAME_REGEXP_EXPANDED "/" /* NOT_IN_PERL */
#define EXACONFIG_MOUNTPOINT_REGEXP           "[" ALPHA_RANGE DEC_RANGE "_./-]" /* NOT_IN_PERL */

#define EXACONFIG_GFS_UUID_REGEXP_EXPANDED    HEX_RANGE_EXPANDED ":" /* NOT_IN_PERL */
#define EXACONFIG_GFS_UUID_REGEXP             "[" HEX_RANGE ":]"     /* NOT_IN_PERL */

#ifdef WIN32
#define EXACONFIG_DEVPATH_REGEXP_EXPANDED     EXACONFIG_MOUNTPOINT_REGEXP_EXPANDED "\\:?{}" /* NOT_IN_PERL */
#define EXACONFIG_DEVPATH_REGEXP              "[" ALPHA_RANGE DEC_RANGE "_./\\:?{}-]" /* NOT_IN_PERL */
#else  /* WIN32 */
#define EXACONFIG_DEVPATH_REGEXP_EXPANDED     EXACONFIG_MOUNTPOINT_REGEXP_EXPANDED":" /* NOT_IN_PERL */
#define EXACONFIG_DEVPATH_REGEXP              "[" ALPHA_RANGE DEC_RANGE "_./:-]" /* NOT_IN_PERL */
#endif /* WIN32 */

/** Sector size in bytes. Do not change this. */
#define SECTOR_SIZE		512
#define SECTORS_TO_BYTES(nb_sectors) ((nb_sectors) * SECTOR_SIZE)
#define BYTES_TO_SECTORS(nb_bytes) ((nb_bytes) / SECTOR_SIZE)

#define KBYTES_TO_BYTES(n_kbytes)  ((n_kbytes) * 1024)
#define BYTES_TO_KBYTES(n_bytes)   ((n_bytes) / 1024)

/* --- Admind constants ------------------------------------------- */

/** Maximum global number of disks in a cluster */
#define NBMAX_DISKS  512

/** Maximum number of network cards handled for each node */
#define NBMAX_NICS_PER_NODE      4

/** Maximum number of parameters */
#define NBMAX_PARAMS 32

#define RUNNING_DIR		"/tmp"

#define ADMIND_THREAD_STACK_SIZE	300000				//!< The size of the stack to pre-initialize.
									//!< To be use with exa_init_stack()

#define ADMIND_PROCESS_NICE	-11					//!< Admind will set its nice value to this.
									//!< Since all daemon are started from admind,
									//!< they will also inherit this value.

#define ADMIND_SOCKET_PORT		30797				//!< The socket on which admind listen


#define ADMIND_PROP_OK		"OK"
#define ADMIND_PROP_NOK		"NOK"
#define ADMIND_PROP_STARTED	"STARTED"
#define ADMIND_PROP_STOPPED	"STOPPED"
#define ADMIND_PROP_UP		"UP"
#define ADMIND_PROP_DOWN	"DOWN"
#define ADMIND_PROP_BROKEN	"BROKEN"
#define ADMIND_PROP_DEGRADED	"DEGRADED"
#define ADMIND_PROP_OFFLINE	"OFFLINE"
#define ADMIND_PROP_REBUILDING	"REBUILDING"
#define ADMIND_PROP_UPDATING	"UPDATING"
#define ADMIND_PROP_REPLICATING	"REPLICATING"
#define ADMIND_PROP_USINGSPARE	"USING SPARE"
#define ADMIND_PROP_OUTDATED	"OUT-DATED"
#define ADMIND_PROP_BLANK	"BLANK"
#define ADMIND_PROP_ALIEN       "ALIEN"
#define ADMIND_PROP_MISSING     "MISSING"
#define ADMIND_PROP_UNDEFINED	"UNDEFINED"
#define ADMIND_PROP_PRIVATE	"PRIVATE"
#define ADMIND_PROP_SHARED	"SHARED"
#define ADMIND_PROP_TRUE	"TRUE"
#define ADMIND_PROP_FALSE	"FALSE"

#define EXA_MAXSIZE_ADMIND_PROP 31

/* These constants are related to metadata-recovery */
#define ADMIND_PROP_INPROGRESS  "INPROGRESS" /**< Mark a transaction as "inprogress" */
#define ADMIND_PROP_COMMITTED   "COMMITTED"  /**< Mark a transaction as "committed" */

#define EXA_PARAM_DEFAULT       "default"
#define EXA_PARAM               "param"
#define EXA_PARAM_DESCRIPTION   "description"
#define EXA_PARAM_NAME          "name"
#define EXA_PARAM_VALUE		"value"
#define EXA_PARAM_VALUE_ITEM	"value_item"
#define EXA_PARAM_TYPE_INFO 	"type_info"
#define EXA_PARAM_CATEGORY	"category"
#define EXA_PARAM_CHOICES	"choices"

/* --- NBD constants ---------------------------------------------- */

/** The TCP port used by the Ethernet plugin of the NBD */
#define SERVERD_DATA_PORT	30796

/** Maximum number of locked zone at same time on one disk by serverd */
#define NBMAX_DISK_LOCKED_ZONES 32

/** 4 KB area reserved at the beginning of each device, in sectors */
#define RDEV_RESERVED_AREA_IN_SECTORS  BYTES_TO_SECTORS(4096)

/* Serverd constants */

#define NBD_THREAD_STACK_SIZE	40960

/* --- ISCSI constants -------------------------------------------- */

/** Vendor ID: must be 8 char long. */
#define SCSI_VENDOR_ID   "Seanodes"

/* Product ID: must be 16 char long. */
#define SCSI_PRODUCT_ID  "Exanodes        "

/* --- VRT constants ---------------------------------------------- */

/** Maximum number of real devices that the virtualizer can handle. */
#define NBMAX_DISKS_PER_GROUP	NBMAX_DISKS

/** Maximum number of physical disks handled for each node */
#define NBMAX_DISKS_PER_NODE    64

/** Maximum number of disks per SPOF group. */
#define NBMAX_DISKS_PER_SPOF_GROUP NBMAX_DISKS_PER_NODE

/** Maximum number of volumes per group that the virtualizer can
    handle. */
#define NBMAX_VOLUMES_PER_GROUP 256

/** Maximum number of started volumes. With 2.4 kernels, it cannot
    easily be increased above 256. */
#define NBMAX_STARTED_VOLUMES 256

/** Maximum number of spare stripes */
#define NBMAX_SPARES_PER_GROUP 16

#define SSTRIPING_NAME	"sstriping"	//!< name of the layout SSTRIPING
#define RAINX_NAME	"rainX"		//!< name for layout RAINS
#define RAIN1_NAME	"rain1"		


/** A comma separated list of Layouts supported by Exanodes
 *  XXX Get rid of it: only used by CLI command exa_makeconfig.
 */
#define EXA_LAYOUT_TYPES	SSTRIPING_NAME "," RAINX_NAME "," RAIN1_NAME /* NOT_IN_PERL */

/** A comma separated list of Access modes supported by Exanodes
 *  It's used by admin command to validate the user configuration file.
 */
#define EXA_PRIVATE_MODE        "private"
#define EXA_SHARED_MODE         "shared"

/** Size constants are in KB
 * */

#define VRT_DEFAULT_NB_SPARES                0  /*          NOT_IN_PERL */
#define VRT_MIN_DIRTY_ZONE_SIZE     (  1 << 10) /*   1 MB   NOT_IN_PERL */

/** Beware, the following parameters have an impact on the size of the
 *  chunk array.
 */
#define VRT_DEFAULT_CHUNK_SIZE     262144 /**< Default chunk size in KB (256 MB) */
#define VRT_MIN_CHUNK_SIZE          32768 /**< Min chunk size in KB (32 MB) */

/** Maximum number of chunks per disk group */
#define VRT_NBMAX_CHUNKS_PER_GROUP   500000   /* NOT_IN_PERL */

/* --- FS constants -------------------------------------------------- */

#ifdef WITH_FS

#define FS_MINIMUM_EXT3_SIZE (8*1024)
#define GFS_SIZE_STRING_CLUSTER_FS_ID (8)
#define GFS_MIN_RG_SIZE (256)
#define GFS_MAX_RG_SIZE (2048)
#define GFS_OPTIMAL_RG_PER_NODE (32) /* This is pure heuristic : consider that
					having 32 RG per node is a good option.
					Now, this can be changed at will, or
					forced with a case-specific value
					with exa_fscreate. */

  /* mount status constants. Unmounted needs be 0 */
typedef enum exa_fs_mounted_status
  {
    EXA_FS_UNMOUNTED=0,
    EXA_FS_MOUNTED_RW,
    EXA_FS_MOUNTED_RO
  } exa_fs_mounted_status_t;

  /* Compatibility number with GFS. */
  /* It must be the same in GFS, in cluster/gulm/src/config_gulm.h */
#define EXANODES_GFS_COMPATIBILITY_NUMBER 5
#define EXA_SHARED_MEM_FILE_PATH	"/var/cache/exanodes/fs_shm_key"
#define EXA_FS_MOUNT_OPTION_ACCEPTED_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789,="

#endif /* WITH_FS */

#ifdef __cplusplus
}
#endif

#endif
