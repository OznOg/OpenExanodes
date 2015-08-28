/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __NBD_TCP_H_
#define __NBD_TCP_H_

#include "common/include/exa_nodeset.h"
#include "os/include/os_network.h"

int init_tcp(struct nbd_tcp *nbd_tcp, const char *hostname,
             const char *net_type);
void cleanup_tcp(struct nbd_tcp *nbd_tcp);

int tcp_add_peer(exa_nodeid_t client_id, const char *net_id, struct nbd_tcp *tcp);

int tcp_remove_peer(uint64_t client_id, struct nbd_tcp *tcp);

int tcp_connect_to_peer(nbd_tcp_t *nbd_tcp, exa_nodeid_t nid);

int tcp_start_listening(nbd_tcp_t *nbd_tcp);

void tcp_stop_listening(nbd_tcp_t *nbd_tcp);

int tcp_send_data(struct header *data_header, struct nbd_tcp *tcp);


#endif
