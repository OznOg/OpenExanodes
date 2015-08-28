/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "lum/client/include/lum_client.h"
#include "lum/client/src/lum_msg.h"

#include "log/include/log.h"

#include "common/include/daemon_api_client.h"
#include "common/include/daemon_request_queue.h"

#include "os/include/strlcpy.h"
#include "os/include/os_string.h"

#include <errno.h>

#include "target/iscsi/include/iqn.h"

int lum_client_init(ExamsgHandle mh, const lum_init_params_t *params)
{
    lum_request_t req;
    lum_answer_t answer;
    int retval;

    memset(&req, 0xEE, sizeof(req));

    req.type  = LUM_CMD_INIT;

    req.init.init_params.iscsi_queue_depth = params->iscsi_queue_depth;
    req.init.init_params.bdev_queue_depth = params->bdev_queue_depth;

    req.init.init_params.buffer_size = params->buffer_size;
    req.init.init_params.target_listen_address = params->target_listen_address;
    iqn_copy(&req.init.init_params.target_iqn, &params->target_iqn);

    /* Use nointr as the cluster is being inited and thus failure detection
     * is not ready yet -> without nointr the node may detect erroneously a
     * node down. */
    retval = admwrk_daemon_query_nointr(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                                        &req, sizeof(req),
                                        &answer, sizeof(answer));

    return retval < 0 ? retval : answer.error;
}

int lum_client_cleanup(ExamsgHandle mh)
{
    lum_request_t req;
    lum_answer_t answer;
    int retval;

    memset(&req, 0xEE, sizeof(req));

    req.type  = LUM_CMD_CLEANUP;

    /* Use nointr as the cluster is being inited and thus failure detection
     * is not ready yet -> without nointr the node may detect erroneously a
     * node down. */
    retval = admwrk_daemon_query_nointr(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                                        &req, sizeof(req),
                                        &answer, sizeof(answer));

    return retval < 0 ? retval : answer.error;
}

int lum_client_suspend(ExamsgHandle mh)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type  = LUM_CMD_SUSPEND;

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req),
                              &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}

int lum_client_resume(ExamsgHandle mh)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type  = LUM_CMD_RESUME;

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
				 &req, sizeof(req),
				 &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}

int lum_client_set_peers(ExamsgHandle mh, const adapter_peers_t *peers)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type  = LUM_CMD_SET_PEERS;

    memcpy(&req.set_peers, peers, sizeof(req.set_peers));

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
				 &req, sizeof(req),
				 &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}

int lum_client_set_targets(ExamsgHandle mh, uint32_t num_addr,
                           const in_addr_t target_addresses[])
{
    int msgsize = num_addr * sizeof(in_addr_t) + sizeof(lum_request_t);
    char __tmp[msgsize];
    lum_request_t *cmd = (lum_request_t *)__tmp;
    lum_target_addresses_t *targetreq = &cmd->target_addresses;
    lum_answer_t reply;
    int err;

    if (msgsize >= DAEMON_REQUEST_MSG_MAXSIZE)
    {
        exalog_error("Too many target IPs to pass to LUM. Maximum is 1280 IPs.");
        return -EMSGSIZE;
    }

    cmd->type = LUM_CMD_SET_TARGETS;

    targetreq->num_addr = num_addr;

    memcpy(targetreq->addr, target_addresses, num_addr * sizeof(in_addr_t));

    err = admwrk_daemon_query_nointr(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                                     __tmp, sizeof(__tmp),
                                     &reply, sizeof(reply));

    return err != 0 ? err : reply.error;
}

int lum_client_set_membership(ExamsgHandle mh, const exa_nodeset_t *mship)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type = LUM_CMD_SET_MSHIP;
    exa_nodeset_copy(&req.set_mship.mship, mship);

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req),
                              &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}

int lum_client_export_publish(ExamsgHandle mh, const char *buf, size_t buf_size)
{
    char __tmp[buf_size + sizeof(lum_request_t)];
    lum_request_t *cmd = (lum_request_t *)__tmp;
    lum_cmd_publish_t *exportreq = &cmd->publish;
    lum_answer_t reply;
    int ret;

    cmd->type = LUM_CMD_EXPORT_PUBLISH;

    exportreq->buf_size = buf_size;

    memcpy(exportreq->buf, buf, buf_size);

    ret = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              __tmp, sizeof(__tmp),
                              &reply, sizeof(reply));

    return ret != 0 ? ret : reply.error;
}

int lum_client_export_unpublish(ExamsgHandle mh,
                                const exa_uuid_t *export_uuid)
{
    lum_request_t req;
    lum_answer_t answer;
    int retval;

    memset(&req, 0xEE, sizeof(req));

    req.type = LUM_CMD_EXPORT_UNPUBLISH;
    uuid_copy(&req.unpublish.export_uuid, export_uuid);

    retval = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                                 &req, sizeof(req),
                                 &answer, sizeof(answer));

    return retval < 0 ? retval : answer.error;
}

int lum_client_export_info(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                           lum_export_info_reply_t *reply)
{
    lum_request_t req;
    lum_answer_t answer;
    int retval;

    EXA_ASSERT(reply);

    memset(&req, 0xEE, sizeof(req));

    req.type = LUM_INFO;

    uuid_copy(&req.info.export_uuid, export_uuid);

    retval = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                                 &req, sizeof(req),
                                 &answer, sizeof(answer));

    *reply = answer.info;

    return retval < 0 ? retval : answer.error;
}

int lum_client_set_readahead(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                             uint32_t readahead)
{
    lum_request_t req;
    lum_answer_t answer;
    int r;

    exalog_debug("Setting readahead of export "UUID_FMT" to %"PRIu32,
                UUID_VAL(export_uuid), readahead);

    req.type = LUM_CMD_SET_READAHEAD;
    uuid_copy(&req.set_readahead.export_uuid, export_uuid);
    req.set_readahead.readahead = readahead;

    r = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                            &req, sizeof(req),
                            &answer, sizeof(answer));
    return r < 0 ? r : answer.error;
}

int lum_client_export_resize(ExamsgHandle mh, const exa_uuid_t *export_uuid,
                          uint64_t size)
{
    lum_request_t req;
    lum_answer_t answer;
    int r;

    req.type = LUM_CMD_EXPORT_RESIZE;

    uuid_copy(&req.export_resize.export_uuid, export_uuid);
    req.export_resize.size = size;

    r = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
			    &req, sizeof(req),
			    &answer, sizeof(answer));
    return r < 0 ? r : answer.error;
}

int lum_client_export_update_iqn_filters(ExamsgHandle mh, const char *buf, size_t buf_size)
{
    char __tmp[buf_size + sizeof(lum_request_t)];
    lum_request_t *cmd = (lum_request_t *)__tmp;
    lum_cmd_update_iqn_filters_t *update_cmd = &cmd->update_iqn_filters;
    lum_answer_t reply;
    int ret;

    cmd->type = LUM_CMD_EXPORT_UPDATE_IQN_FILTERS;

    update_cmd->buf_size = buf_size;

    memcpy(update_cmd->buf, buf, buf_size);

    ret = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              __tmp, sizeof(__tmp),
                              &reply, sizeof(reply));
    if (ret != 0)
        return ret;
    else
        return reply.error;
}

int lum_client_get_nth_connected_iqn(ExamsgHandle mh,
                                     const exa_uuid_t *export_uuid,
                                     unsigned int iqn_num,
                                     iqn_t *iqn)
{
    lum_request_t req;
    lum_answer_t answer;
    int retval;

    memset(&req, 0xEE, sizeof(req));

    req.type  = LUM_INFO_GET_NTH_CONNECTED_IQN;
    req.nth_iqn.iqn_num = iqn_num;
    uuid_copy(&req.nth_iqn.export_uuid, export_uuid);

    retval = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
				 &req, sizeof(req),
				 &answer,  sizeof(answer));

    if (retval == EXA_SUCCESS)
        iqn_copy(iqn, &answer.connected_iqn);

    return retval < 0 ? retval : answer.error;
}

int lum_client_start_target(ExamsgHandle mh)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type  = LUM_CMD_START_TARGET;

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req),
                              &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}

int lum_client_stop_target(ExamsgHandle mh)
{
    lum_request_t req;
    lum_answer_t answer;
    int err;

    req.type  = LUM_CMD_STOP_TARGET;

    err = admwrk_daemon_query(mh, EXAMSG_LUM_ID, EXAMSG_DAEMON_RQST,
                              &req, sizeof(req),
                              &answer, sizeof(answer));

    return err < 0 ? err : answer.error;
}
