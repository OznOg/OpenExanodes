/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_NETWORK
#define H_NETWORK

#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"

#include "examsg/include/examsg.h"

#include "examsgd.h"

/** Message to request network communication */
typedef struct ExamsgNetRqst
  {
    uint8_t to;                         /* recipient mailbox id */
    uint8_t flags;                      /* delivery flags */
    uint16_t size;                      /* msg size */
    exa_nodeset_t dest_nodes;           /**< Destination nodes */
    char msg[];                         /* message body (actual size is \a size) */
  } ExamsgNetRqst;

/* multicast sequence number size */
typedef uint16_t mseq_t;

/** Request a new transmission */
EXAMSG_DCLMSG(ExamsgRetransmitReq, struct {
  exa_nodeid_t node_id;               /**< Id of node asked to retransmit */
  mseq_t count;                       /**< number of the msg to resend */
});

/** Retransmit a message */
EXAMSG_DCLMSG(ExamsgRetransmit, struct {
  mseq_t count;                         /**< number of the msg to resend */
});

int network_init(const exa_uuid_t *cluster_uuid, const char *node_name,
	      const char *hostname,  exa_nodeid_t nodeid,
	      const char *mgroup, unsigned short mport, unsigned short inca);

void network_exit(void);

void send_ping(void);

int network_add_node(exa_nodeid_t id, const char *name);
int network_del_node(exa_nodeid_t id);

void network_saw_node_up(exa_nodeid_t id);
bool network_node_seen_up(exa_nodeid_t id);

void network_handle_fence_request(const examsgd_fencing_req_t *req);

void network_set_status(int status);
bool network_manageable(int err);
int network_status(void);
int network_waitup(void);

int network_send(const ExamsgMID *mid, const ExamsgNetRqst *msg);
int network_send_retransmit_req(exa_nodeid_t node_id, mseq_t count);
int retransmit(mseq_t count);

int network_recv(ExamsgHandle mh, ExamsgMID *mid, char **msg, size_t *nbytes,
		 int *to);

void network_ignore_message(exa_nodeid_t node_id);

int network_special_send(const ExamsgMID *mid, const Examsg *msg,
	                 size_t msgsize);
void network_ack(void);

#endif /* H_NETWORK */
