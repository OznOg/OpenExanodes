/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_EXAMSG
#define H_EXAMSG

/** \file examsg.h
 * \brief Cluster messaging framework.
 *
 * Public headers for cluster messaging functionality.
 *
 * This file can be included in C++ programs, but in this case only the
 * \a ExamsgID enum is available.
 */

/** Static data operation */
typedef enum
{
    EXAMSG_STATIC_CREATE,   /**< Create examsg static data */
    EXAMSG_STATIC_GET,      /**< Use an existing examsg data */
    EXAMSG_STATIC_RELEASE,  /**< Release own static data */
    EXAMSG_STATIC_DELETE    /**< Delete examsg static data */
} examsg_static_op_t;


/** Components ids - 0 is a reserved id, it cannot be used. */
typedef enum ExamsgID {
#define EXAMSG_FIRST_ID EXAMSG_NETMBOX_ID
  EXAMSG_NETMBOX_ID = 0,	      /**< do not use or change */

  EXAMSG_LOGD_ID,		      /**< logging daemon */
  EXAMSG_CMSGD_ID,		      /**< cluster messaging daemon */
  EXAMSG_CMSGD_RECV_ID,		      /**< internal to examsgd */
  EXAMSG_NBD_ID,		      /**< nbd service */
  EXAMSG_NBD_SERVER_ID,		      /**< nbd server */
  EXAMSG_NBD_CLIENT_ID,		      /**< nbd client */
  EXAMSG_NBD_LOCKING_ID,	      /**< nbd locking service */
  EXAMSG_VRT_LOCKING_ID,	      /**< virtualizer module (locking) */
  EXAMSG_VRT_ID,                      /**< virtualizer module events id */
  EXAMSG_ADMIND_ID,		      /**< admind daemon */
  EXAMSG_ADMIND_EVMGR_ID,	      /**< admind events manager */

  EXAMSG_ADMIND_CLISERVER_ID,	      /**< CLI server thread */

  EXAMSG_ADMIND_INFO_ID,	      /**< admind thread 1: CLI (info) */
  EXAMSG_ADMIND_INFO_LOCAL,	      /**< admind thread 1: CLI (info) */
  EXAMSG_ADMIND_INFO_BARRIER_ODD,     /**< admind thread 1: CLI (info) */
  EXAMSG_ADMIND_INFO_BARRIER_EVEN,    /**< admind thread 1: CLI (info) */

  EXAMSG_ADMIND_CMD_ID,		      /**< admind thread 2: CLI (commands) */
  EXAMSG_ADMIND_CMD_LOCAL,	      /**< admind thread 2: CLI (commands) */
  EXAMSG_ADMIND_CMD_BARRIER_ODD,      /**< admind thread 2: CLI (commands) */
  EXAMSG_ADMIND_CMD_BARRIER_EVEN,     /**< admind thread 2: CLI (commands) */

  EXAMSG_ADMIND_RECOVERY_ID,	      /**< admind thread 3: event manager */
  EXAMSG_ADMIND_RECOVERY_LOCAL,     /**< admind thread 3: event manager */
  EXAMSG_ADMIND_RECOVERY_BARRIER_ODD, /**< admind thread 3: event manager */
  EXAMSG_ADMIND_RECOVERY_BARRIER_EVEN,/**< admind thread 3: event manager */

  EXAMSG_CSUPD_ID,		      /**< supervisor daemon */
  EXAMSG_FSD_ID,		      /**< filesystem daemon */
  EXAMSG_RDEV_ID,		      /**< physical disk management module */

  EXAMSG_MONITORD_CONTROL_ID,         /**< monitoring daemon control */
  EXAMSG_MONITORD_EVENT_ID,           /**< monitoring daemon events */

  EXAMSG_ISCSI_ID,                    /**< ISCSI target */
  EXAMSG_LUM_ID,                      /**< LUM daemon */

  EXAMSG_TEST_ID,		      /**< test tools id */
  EXAMSG_TEST2_ID,
  EXAMSG_TEST3_ID

#define EXAMSG_LAST_ID EXAMSG_TEST3_ID
} ExamsgID;

#define EXAMSG_ID_VALID(cid) \
  ((cid) >= EXAMSG_FIRST_ID && (cid) <= EXAMSG_LAST_ID)

#ifndef __cplusplus

#include "common/include/exa_constants.h"
#include "common/include/uuid.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"

#include <stdlib.h>

/* --- examsgIdToName ------------------------------------------- */
/** \brief Return the name given an id.
 *
 * \param[in] id	ExamsgID of a examessage user.
 * \return name describing \a id
 */
static inline const char * examsgIdToName(ExamsgID id)
{
  static const struct idnames {
    ExamsgID id;
    const char *name;
  } list[] = {
    { EXAMSG_NETMBOX_ID,		    "Network Communication"			        },
    { EXAMSG_LOGD_ID,			    "Logging daemon"			                },
    { EXAMSG_CMSGD_ID,			    "Messaging daemon"			                },
    { EXAMSG_CMSGD_RECV_ID,                 "Messaging daemon internal"				},
    { EXAMSG_NBD_ID,			    "NBD"					        },
    { EXAMSG_NBD_CLIENT_ID,	            "NBD Client"				        },
    { EXAMSG_NBD_SERVER_ID,	            "NBD Server"				        },
    { EXAMSG_NBD_LOCKING_ID,		    "NBD (un)locking"                                   },
    { EXAMSG_VRT_LOCKING_ID,		    "Virtualizer module (locking)"		        },
    { EXAMSG_VRT_ID,                        "Virtualizer module events"                         },
    { EXAMSG_ADMIND_ID,			    "Admind"				                },
    { EXAMSG_ADMIND_EVMGR_ID,		    "Admind Event Manager"			        },
    { EXAMSG_ADMIND_CLISERVER_ID,           "Admind Cli server"                                 },
    { EXAMSG_ADMIND_INFO_ID,		    "Admind info thread"			        },
    { EXAMSG_ADMIND_INFO_LOCAL,		    "Admind info thread (local)"		        },
    { EXAMSG_ADMIND_INFO_BARRIER_ODD,	    "Admind info thread (odd barrier)"		        },
    { EXAMSG_ADMIND_INFO_BARRIER_EVEN,	    "Admind info thread (even barrier)"		        },
    { EXAMSG_ADMIND_CMD_ID,		    "Admind Command Thread"			        },
    { EXAMSG_ADMIND_CMD_LOCAL,		    "Admind Command Thread (local)"		        },
    { EXAMSG_ADMIND_CMD_BARRIER_ODD,	    "Admind Command Thread (odd barrier)"	        },
    { EXAMSG_ADMIND_CMD_BARRIER_EVEN,	    "Admind Command Thread (even barrier)"	        },
    { EXAMSG_ADMIND_RECOVERY_ID,	    "Admind Recovery Thread"			        },
    { EXAMSG_ADMIND_RECOVERY_LOCAL,	    "Admind Recovery Thread (local)"		        },
    { EXAMSG_ADMIND_RECOVERY_BARRIER_ODD,   "Admind Recovery Thread (odd barrier)"	        },
    { EXAMSG_ADMIND_RECOVERY_BARRIER_EVEN,  "Admind Recovery Thread (even barrier)"	        },
    { EXAMSG_CSUPD_ID,			    "Supervision"				        },
    { EXAMSG_FSD_ID,			    "File System Daemon"			        },
    { EXAMSG_MONITORD_EVENT_ID,	            "Events list of monitoring daemon"			},
    { EXAMSG_MONITORD_CONTROL_ID,           "Events list of monitoring daemon"			},
    { EXAMSG_RDEV_ID,			    "Physical disks management module"		        },
    { EXAMSG_ISCSI_ID,                      "ISCSI target"                                      },
    { EXAMSG_LUM_ID,                        "LUM Daemon"                                        },
    { EXAMSG_TEST_ID,			    "Test program"				        },
    { EXAMSG_TEST2_ID,			    "Test program 2"			                },
    { EXAMSG_TEST3_ID,			    "Test program 3"			                },
    { (ExamsgID)-1, NULL }
  };
  const struct idnames *p;

  for (p = list; p->name; p++)
    if (p->id == id)
      return p->name;

  return "unknown";
}

/** Maximum message size in bytes */
#define EXAMSG_MSG_MAX		((size_t)10240)

/** Message types */
typedef enum ExamsgType {
  /* generic */
  EXAMSG_INVALID,       /**< (public) the message content is not valid */
  EXAMSG_PING,		/**< (public) ping message */
  EXAMSG_ACK,		/**< (public) message acknowledgement */
  EXAMSG_START,		/**< (public) start a service */
  EXAMSG_ADDNODE,	/**< (public) add a node to exa_msgd */
  EXAMSG_DELNODE,       /**< (public) delete a node from exa_msgd */
  EXAMSG_RETRANSMIT_REQ,/**< (private) request a new transmit */
  EXAMSG_RETRANSMIT,    /**< (private) retransmit a message */
  EXAMSG_FENCE,         /**< (private) asks for fencing/unfencing  */
  EXAMSG_NEW_COMER,     /**< (private) A new node has arrived */
  EXAMSG_EXIT,          /**< (private) Ask examsgd to terminate */

  /* supervision */
  EXAMSG_SUP_MSHIP_CHANGE,  /**< (public) mship change sent by supervision */

  /* supervision, private */
  EXAMSG_SUP_PING,	/**< (private) csupd inter node ping message */

  /* nbd service  */
  EXAMSG_NBD_LOCK,    /**< (public) ask to (un)lock some I/O */

  /* admind */
  EXAMSG_ADM_CLUSTER_CMD,      /**< Used to call the master command from the CLI/GUI command */
  EXAMSG_ADM_CLUSTER_CMD_END,  /**< Reply message when cluster comand is finished */
  EXAMSG_ADM_CLSHUTDOWN,   /**< Used to call the local commands from the master command */
  EXAMSG_ADM_CLDELETE,     /**< Tell the evmgr to delete the config */
  /* admind event manager */

  /* admind service manager */
  EXAMSG_SERVICE_LOCALCMD,
  EXAMSG_SERVICE_REPLY,
  EXAMSG_SERVICE_BARRIER,
  EXAMSG_SERVICE_ADMIND,
  EXAMSG_SERVICE_ADMIND_VERSION_ID,
  EXAMSG_SERVICE_ADMIND_CONFIG_CHUNK,
  EXAMSG_SERVICE_RDEV_VERSION,
  EXAMSG_SERVICE_RDEV_DEAD_INFO,
  EXAMSG_SERVICE_RDEV_BROKEN_DISKS_EXCHANGE,
  EXAMSG_SERVICE_NBD,
  EXAMSG_SERVICE_NBD_DISKS_INFO,
  EXAMSG_SERVICE_NBD_CLIQUE,
  EXAMSG_SERVICE_VRT,
  EXAMSG_SERVICE_VRT_SB_SYNC,
  EXAMSG_SERVICE_VRT_REINTEGRATE_INFO,
  EXAMSG_SERVICE_VRT_RESYNC,

  /* internal event manager events */
  EXAMSG_EVMGR_MSHIP_READY,
  EXAMSG_EVMGR_MSHIP_YES,
  EXAMSG_EVMGR_MSHIP_PREPARE,
  EXAMSG_EVMGR_MSHIP_ACK,
  EXAMSG_EVMGR_MSHIP_COMMIT,
  EXAMSG_EVMGR_MSHIP_ABORT,
  EXAMSG_EVMGR_INST_EVENT,
  EXAMSG_EVMGR_RECOVERY_REQUEST,
  EXAMSG_EVMGR_RECOVERY_END,

  /* service LUM data */
  EXAMSG_SERVICE_LUM_EXPORTS_VERSION,
  EXAMSG_SERVICE_LUM_EXPORTS_NUMBER,
  EXAMSG_SERVICE_LUM_EXPORTS_EXPORT,
  EXAMSG_SERVICE_LUM_TARGET_LISTEN_ADDRESSES,

  EXAMSG_DAEMON_REPLY,
  EXAMSG_DAEMON_RQST,
  EXAMSG_DAEMON_INTERRUPT,
  EXAMSG_DAEMON_INTERRUPT_ACK,

  EXAMSG_MD_EVENT,          /**< Use to send events to MD */
  EXAMSG_MD_CONTROL,        /**< Use to send control message to MD */
  EXAMSG_MD_STATUS,        /**< Use to reply MD status */
} ExamsgType;

/* Automatically generated at compile time */
const char *examsgTypeName(ExamsgType type);

/* --- Generic message definitions ----------------------------------- */

/** Generic message header */
typedef struct ExamsgAny {
  ExamsgType type;	     /**< message type */
  uint32_t   pad;
} ExamsgAny;

#include "common/include/exa_assert.h"

#define EXAMSG_DCLMSG(typename, ...)	                  		\
  typedef struct typename {						\
    ExamsgAny any;							\
    __VA_ARGS__;							\
  } typename;								\
									\
  /** \cond skip */							\
  /* ensure message size is smaller than biggest admissible size */	\
  static inline void __attribute__((unused))				\
  __ ## typename ## _check_size(void)					\
  {									\
    COMPILE_TIME_ASSERT(sizeof(typename)				\
			<= EXAMSG_MSG_MAX);	                        \
  }									\
  /** \endcond */


/* --- Public message definitions ------------------------------------ */
/** EXAMSG_PAYLOAD_MAX is the maximum amount of data that a examsg can
 *  send. */
#define EXAMSG_PAYLOAD_MAX (EXAMSG_MSG_MAX - sizeof(ExamsgAny))

/** Generic message container (Maximum size for a message) */
typedef struct Examsg {
  ExamsgAny any;                         /**< header */
  char      payload[EXAMSG_PAYLOAD_MAX]; /**< payload */
} Examsg;

/** Examsg handle */
typedef struct ExamsgHandle *ExamsgHandle;

/** Predefined node sets */
extern const exa_nodeset_t *const EXAMSG_LOCALHOST;
extern const exa_nodeset_t *const EXAMSG_ALLHOSTS;

/* --- API ----------------------------------------------------------- */

/** Node identification on the network */
typedef struct ExamsgNetID {
  exa_uuid_t cluster;			/**< cluster id, MUST BE FIRST */
  exa_nodeid_t node;			/**< node id */
} ExamsgNetID;

/** Message identification */
typedef struct ExamsgMID {
  ExamsgNetID netid;			/**< sender network node id, MUST BE FIRST */
  char host[EXA_MAXSIZE_HOSTNAME+1];	/**< sender internet address */
  ExamsgID    id;			/**< sender id */
} ExamsgMID;

struct ExamsgAny;

/** examsgInit
 * \brief Create and initialize a ExamsgHandle.
 *
 * \param[in] owner: the caller component id.
 *
 * A handle is NOT thread safe.
 *
 * \return A handle with no Mailboxes (can only be used to send messages).
 */
ExamsgHandle __examsgInit(ExamsgID owner, const char *file, unsigned int line);
#define examsgInit(owner) __examsgInit(owner, __FILE__, __LINE__)

/** examsgExit
 * \brief Delete a ExamsgHandle.
 *
 * \param[in] mh: the handle.
 *
 * The owner is in charge of destroying the mailboxes the mh may
 * have BEFORE calling this function. This means that any undeleted
 * mbox the mh may have before calling this function will remain so.
 *
 * \return 0 or -EINVAL if Handle is NULL.
 */
int examsgExit(ExamsgHandle mh);

/* --- examsgOwner --------------------------------------------------- */
/** \brief Return owner of an examsg handle
 *
 * \param[in] mh	Examsg handle
 * \return the owner id
 */
ExamsgID examsgOwner(ExamsgHandle mh);

/** examsgAddMbox
 * \brief Add a mailbox to a ExamsgHandle.
 *
 * \param[in] mh:       examsg handle, created with examsgInit().
 * \param[in] id:       the id of the mbox to Add
 * \param[in] num_msg   Number of messages
 * \param[in] msg_size  Message size
 *
 * \return 0 on success, or a negative error code.
 */
int examsgAddMbox(ExamsgHandle mh, ExamsgID id, size_t num_msg, size_t msg_size);

/** examsgDelMbox
 * \brief Delete a mailbox from a ExamsgHandle.
 *
 * \param[in] mh: examsg handle, created with examsgInit().
 * \param[in] id: the id of the mbox to delete
 * \return 0 on success, or a negative error code.
 */
int examsgDelMbox(ExamsgHandle mh, ExamsgID id);

/** examsgSend
 * \brief Send a message to a mailbox.
 *
 * The function sends a message to the mailbox of id \a to. The
 * message appears to come from the owner of \a mh.
 * When the recipient msgbox is full, this function is blocking.
 *
 * \param[in] mh	Examsg handle created by examsgInit().
 * \param[in] to	Recipient id.
 * \param[in] dest_nodes Destination nodes
 * \param[in] buffer	Content of the message.
 * \param[in] nbytes	Length of the message, in bytes.
 *
 * \return number of bytes sent or a negative error code
 */
int examsgSend(ExamsgHandle mh, ExamsgID to, const exa_nodeset_t *dest_nodes,
	       const void *buffer, size_t nbytes);

/** examsgSendWithAck
 * \brief Send a message and wait for acknowledgement
 *
 * The function sends a message to the mailbox of component \a to, like
 * \a examsgSend(). THIS FUNCTION IS BLOCKING until the acknowledgement
 * is received or an error occurs. The mext message MUST be a ack message.
 * If this is not the case, this function asserts.
 *
 * \param[in] mh	Examsg handle created by examsgInit().
 * \param[in] to	Recipient id.
 * \param[in] dest_nodes Destination nodes
 * \param[in] msg	Message structure.
 * \param[in] nbytes	Length of the message, in bytes.
 * \param[out] ackError	If not NULL, contains the acknowledgement status
 *			or an error code if the sending of the message
 *			wasn't successful.
 *
 * \return number of bytes sent or a negative error code.
 */
int examsgSendWithAck(ExamsgHandle mh, ExamsgID to,
		      const exa_nodeset_t *dest_nodes,
		      const Examsg *msg, size_t nbytes,
		      int *ackError);

/** examsgAckReply
 * \brief Send acknowledgement message.
 *
 * \param[in] mh: examsg handle, created with examsgInit().
 * \param[in] msg: original message
 * \param[in] error: error code (0 for success)
 * \param[in] to: recipient id
 * \param[in] dest_nodes  Destination nodes, must be singleton
 *
 * \return 0 on success or a negative error code.
 */
int examsgAckReply(ExamsgHandle mh, const Examsg *msg, int error, ExamsgID to,
		   const exa_nodeset_t *dest_nodes);

/** examsgSendWithHeader
 * \brief Send a message to a mailbox after prepending a header.
 *
 * The function sends a message to the mailbox of id \a to,
 * after prepending the header pointer at by \a any. The message
 * appears to come from the owner of \a mh.
 * When the recipient msgbox is full, this function is blocking.
 *
 * \param[in] mh	Examsg handle created by examsgInit().
 * \param[in] to	Recipient id.
 * \param[in] dest_nodes Destination nodes
 * \param[in] any	Examsg header to prepend.
 * \param[in] buffer	Content of the message.
 * \param[in] nbytes	Length of the message, in bytes.
 *
 * \return number of bytes sent or a negative error code
 */
int examsgSendWithHeader(ExamsgHandle mh, ExamsgID to,
			 const exa_nodeset_t *dest_nodes,
			 const struct ExamsgAny *any,
			 const void *buffer, size_t nbytes);

/** examsgSendNoBlock
 * \brief Send a message to a mailbox without blocking.
 *
 * The function sends a message to the mailbox of id \a to. The
 * message appears to come from the owner of \a mh. In case the receiver
 * mail box is full the function returns without sending the message
 * and then returns -ENOSPC .
 *
 * \param[in] mh	Examsg handle created by examsgInit().
 * \param[in] to	Recipient id.
 * \param[in] dest_nodes Destination nodes
 * \param[in] buffer	Content of the message.
 * \param[in] nbytes	Length of the message, in bytes.
 *
 * \return number of bytes sent or a negative error code ; -ENOSPC in case
 * of mailbox full
 */
int examsgSendNoBlock(ExamsgHandle mh, ExamsgID to,
		      const exa_nodeset_t *dest_nodes,
		      const void *buffer, size_t nbytes);

/** examsgRecv
 * \brief Receive a message.
 *
 * Read local mailbox of handle \a mh and copy next message into
 * \a buffer. At most \a maxbytes are copied. The function does not block
 * if there is no message in the mailbox.
 *
 * \param[in] mh	Examsg handle, created with examsgInit().
 * \param[out] mid	If not NULL, filled with the sender id of the
 *			message.
 * \param[out] buffer	Filled with the message.
 * \param[in] maxbytes	Maximum number of bytes to copy into \a buffer.
 *
 * \return number of bytes of the message, or 0 if no message available
 *                or a negative error code.
 */
int examsgRecv(ExamsgHandle mh, ExamsgMID *mid, void *buffer,
	       size_t maxbytes);


/** examsgWait
 * \brief Sleep until there is something to read in one of mh mailboxes.
 *
 * \param mh  Examsg handle
 *
 * In kernel space, the signals are flushed with flush_signals().
 *
 * return 0 on success or a negative error code.
 */
int examsgWait(ExamsgHandle mh);

#include "os/include/os_time.h" /* for timeval */

/** examsgWaitTimeout
 * \brief Wait for an event on mh mailbox(es).
 *
 * \param         mh        Examsg handle
 * \param[in:out] timeout   Maximum time to wait
 *
 * Waits for at most #timeout, and return -ETIME if the timer expired.
 * In kernel space, the signals are flushed with flush_signals().
 *
 * When this function returns, timeout is updated.
 *
 * return 0 on success or a negative error code.
 */
int examsgWaitTimeout(ExamsgHandle mh, struct timeval *timeout);

/** examsgWaitInterruptible
 *
 * \brief Wait for an event on mh mailbox(es).
 *
 * \param[in]     mh        Examsg handle
 * \param[in:out] timeout   Maximum time to wait
 *
 * When the function returns, timeout contains the remaining time.
 *
 * return 0 on success or a negative error code.
 *                -EINTR if a signal was delivered.
 *                -ETIME if timeout is reached
 *                or another negative error code
 */
int examsgWaitInterruptible(ExamsgHandle mh, struct timeval *timeout);

/** examsg_static_init
 * Init the examessage framework.
 *
 * @param[in] op  Operation to perform, either EXAMSG_STATIC_CREATE or
 *                EXAMSG_STATIC_GET.
 *
 * return 0 or a negative error code.
 */
int examsg_static_init(examsg_static_op_t op);

/** examsg_static_clean
 * Cleanup the examsg framework.
 *
 * @param[in] op  Operation to perform, either EXAMSG_STATIC_DELETE or
 *                EXAMSG_STATIC_RELEASE.
 *
 * return 0 or a negative error code
 */
void examsg_static_clean(examsg_static_op_t op);

/** examsg_show_stats
 * Display on stdout the whole informations about examsg data
 */
void examsg_show_stats(void);

#endif /* !__cplusplus */

#endif /* H_EXAMSG */
