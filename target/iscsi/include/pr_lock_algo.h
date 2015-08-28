/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef PR_LOCK_ALGO_H
#define PR_LOCK_ALGO_H

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "os/include/os_inttypes.h"

#define ALGOPR_PRIVATE_DATA ((void *) 0x1)

#define ISCSI_THREAD_STACK_SIZE 16384

/* provided by target */
void scsi_pr_read_metadata(void *ref_msg, void *cdb, int cdb_size);

/**
 * Algo pr call this function to process a pr
 * @param private_data private data knwown by callee
 *                     it's the private_data given when calling algopr_pr_new_pr()
 * @param[out] buffer data to transmit to other node (it will be given as cdb
 *                    when scsi_pr_read_metadata() will be called in other nodes).
 *                    The buffer data must stay valid while scsi_pr_finished()
 *                    is not called.
 * @return >= 0 size of buffer
 *           -1 the operation fail. Stop here, scsi_pr_finished()
 */
int scsi_pr_write_metadata(void *private_data, void **buffer);
void scsi_pr_finished(void *private_data);

/* provided by algo pr and called by target*/
void algopr_pr_new_pr(void *private_data);
int algopr_init(exa_nodeid_t nodeid, int max_buffer_size);
void algopr_cleanup(void);

/**
 * provided by algo pr and called by algopr_network
 * algopr_network send a new msg to the pr_algo
 * @param data data provide by caller,
 * FIXME comment is wrong
 * *** the callee must free
 * *** this data here or asynchronously by calling algpr_recv_msg_free()
 * @param size of data
 */
void algopr_new_msg(const unsigned char *data, int size, unsigned char *buffer,
                    int bsize);

void algopr_resume(void);
void algopr_suspend(void);

void algopr_network_suspend(void);
void algopr_network_resume(void);

/* provided by algo pr and called by vrt */
void algopr_new_membership(const exa_nodeset_t *membership);

/* algopr_network
   FIXME Some (all?) of these prototypes have *nothing* to do here. They're
         defined in algopr_network.c and thus should be declared in
         non-existent algopr_network.h */
int algopr_init_plugin(exa_nodeid_t node_id, int max_buffer_size);
int algopr_close_plugin(void);
void algopr_set_clients(const char addresses[][EXA_MAXSIZE_NICADDRESS + 1]);
void algopr_update_client_connections(const exa_nodeset_t *mship);

int algopr_send_data(exa_nodeid_t node_id, void *buffer1, int size1,
                     void *buffer2, int size2);

/**
 * free the buffer given by algopr_network() at algopr_new_msg() call.
 * @param buffer
 */
void __algpr_recv_msg_free(void *payload);
#define algpr_recv_msg_free(payload) \
    (__algpr_recv_msg_free(payload), payload = NULL)

#endif
