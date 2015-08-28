/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>

#include "admind/services/lum/include/target_config.h"
#include "admind/include/service_lum.h"
#include "admind/src/adm_cluster.h"
#include "admind/src/adm_globals.h"
#include "lum/export/include/export.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/evmgr/evmgr.h"
#include "admind/src/rpc.h"
#include "admind/src/instance.h"
#include "admind/src/commands/command_api.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "os/include/os_mem.h"
#include "os/include/os_network.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"
#include "lum/client/include/lum_client.h"
#include "target/iscsi/include/iqn.h"

typedef struct
{
    int ret;                      /**< return value of the local command */
    iqn_t target_iqn;
    in_addr_t target_addr;
    lum_export_info_reply_t info; /**< export information */
} export_reply_t;

typedef struct
{
    exa_uuid_t export;
} export_request_t;


void local_clinfo_export(int thr_nb, void *msg)
{
    export_reply_t reply;
    export_request_t *request;

    request = msg;

    /* assert that target iqn is set correctly. Actually, if we are performing
     * a clusterized command without having lum inited correctly, this is not
     * acceptable. */
    EXA_ASSERT(lum_get_target_iqn() != NULL);

    iqn_copy(&reply.target_iqn, lum_get_target_iqn());

    reply.target_addr = target_config_get_listen_address();

    reply.ret = lum_client_export_info(adm_wt_get_localmb(), &request->export,
				     &reply.info);

    COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
    admwrk_reply(thr_nb, &reply, sizeof(reply));
}


typedef struct
{
    int ret;    /**< return value of the local command */
    iqn_t iqn;  /**< nth IQN */
} get_nth_iqn_reply_t;

typedef struct
{
    exa_uuid_t export;
    unsigned int iqn_num;
} get_nth_iqn_request_t;

void local_clinfo_get_nth_iqn(int thr_nb, void *msg)
{
  get_nth_iqn_reply_t reply;
  get_nth_iqn_request_t *request = (get_nth_iqn_request_t *)msg;

  reply.ret = lum_client_get_nth_connected_iqn(adm_wt_get_localmb(),
                                               &request->export,
                                               request->iqn_num,
                                               &reply.iqn);

  COMPILE_TIME_ASSERT(sizeof(reply) <= ADM_MAILBOX_PAYLOAD_PER_NODE);
  admwrk_reply(thr_nb, &reply, sizeof(reply));
}

static int cluster_clinfo_export(int thr_nb, xmlNodePtr father_node,
				 const export_info_t *export_info)
{
    admwrk_request_t rpc;
    export_request_t info_request;
    export_reply_t info_reply;
    xmlNodePtr export_node;
    xmlNodePtr gateway_node[EXA_MAX_NODES_NUMBER];
    exa_nodeid_t nodeid;
    exa_nodeset_t status_readonly = EXA_NODESET_EMPTY;
    exa_nodeset_t status_in_use = EXA_NODESET_EMPTY;
    int compound_ret, ret;
    unsigned int i;
    get_nth_iqn_request_t iqn_request;
    bool found_iqn;

    /* Initialize the gateway table */
    for (i = 0; i < EXA_MAX_NODES_NUMBER; i++)
	gateway_node[i] = NULL;

    /* Create the export node as a sub-node of the volume node */
    export_node = xmlNewChild(father_node, NULL, BAD_CAST("export"), NULL);
    if (export_node == NULL)
    {
	exalog_error("Failed to allocate XML structures");
	return -EXA_ERR_XML_ADD;
    }

    switch (export_info->type)
    {
    case EXPORT_ISCSI:
        ret = xml_set_prop(export_node, "method", "iSCSI");
        ret = ret ? ret : xml_set_prop(export_node, "id_type", "LUN");
        ret = ret ? ret : xml_set_prop_u64(export_node, "id_value", export_info->iscsi.lun);
        break;
    case EXPORT_BDEV:
        ret = xml_set_prop(export_node, "method", "bdev");
        ret = ret ? ret : xml_set_prop(export_node, "id_type", "");
        ret = ret ? ret : xml_set_prop(export_node, "id_value", export_info->bdev.path);
        break;
    }

    if (ret != EXA_SUCCESS)
	return ret;


    /* Get the export_info structure */
    uuid_copy(&info_request.export, &export_info->uuid);
    admwrk_run_command(thr_nb, &adm_service_admin, &rpc, RPC_ADM_CLINFO_EXPORT,
		       &info_request, sizeof(info_request));

    compound_ret = EXA_SUCCESS;
    while (admwrk_get_reply(&rpc, &nodeid, &info_reply, sizeof(info_reply), &ret))
    {
	if (ret == -ADMIND_ERR_NODE_DOWN)
	    continue;
	EXA_ASSERT(ret == EXA_SUCCESS);

	if (compound_ret != EXA_SUCCESS)
	    continue;

        /* Whenever a volume is not exported, we do not report an error but
         * simply swallow the error */
        if (info_reply.ret == -VRT_ERR_VOLUME_NOT_EXPORTED)
            continue;

	if (info_reply.ret != EXA_SUCCESS)
	{
	    compound_ret = info_reply.ret;
	    continue;
	}

	/* create an XML node corresponding to each node being an export gateway */
	gateway_node[nodeid] = xmlNewChild(export_node, NULL, BAD_CAST("gateway"), NULL);
	if (gateway_node[nodeid] == NULL)
	{
	    exalog_error("Failed to allocate XML structures");
	    compound_ret = -EXA_ERR_XML_ADD;
	    continue;
	}

	compound_ret = xml_set_prop(gateway_node[nodeid], "node_name",
				    adm_cluster_get_node_by_id(nodeid)->name);

        if (compound_ret == EXA_SUCCESS)
            compound_ret = xml_set_prop(gateway_node[nodeid], "iqn",
                                        iqn_to_str(&info_reply.target_iqn));

        if (compound_ret == EXA_SUCCESS)
        {
            struct in_addr addr = { .s_addr = info_reply.target_addr };

            compound_ret = xml_set_prop(gateway_node[nodeid], "listen_address",
                                        os_inet_ntoa(addr));
        }

        if (info_reply.info.readonly)
            exa_nodeset_add(&status_readonly, nodeid);

        if (info_reply.info.in_use)
            exa_nodeset_add(&status_in_use, nodeid);
    }

    if (compound_ret != EXA_SUCCESS)
	return compound_ret;

    /* XXX in use and readonly inserted in father_node which is wrong... But as
     * the cli expects it to be there, I do not want to change it rigth now.
     * Obviously, it would be nice to change this behaviour, thus puting in use
     * and readonly in the correct place in export infos. */
    ret = xml_set_prop_nodeset(father_node, "status_in_use", &status_in_use);
    if (ret == EXA_SUCCESS)
         ret = xml_set_prop_nodeset(father_node, "status_readonly", &status_readonly);

    if (ret != EXA_SUCCESS)
        return ret;

    if (export_info->type == EXPORT_BDEV)
        return ret;

    /* Get the IQN connected on each gateway
     * It gets the IQNs one-by-one on all the nodes (due to admwrk behavior)
     * The main loop iterate until no node reply with a non-empty string (found_iqn)
     *
     * We must do this way due to the size limitation of the replies
     * (ADM_MAILBOX_PAYLOAD_PER_NODE)
     */
    uuid_copy(&iqn_request.export, &export_info->uuid);
    iqn_request.iqn_num = 0;
    compound_ret = EXA_SUCCESS;
    do
    {
	get_nth_iqn_reply_t iqn_reply;

	found_iqn = false;
	admwrk_run_command(thr_nb, &adm_service_admin, &rpc, RPC_ADM_CLINFO_GET_NTH_IQN,
			   &iqn_request, sizeof(iqn_request));

	while (admwrk_get_reply(&rpc, &nodeid, &iqn_reply, sizeof(iqn_reply), &ret))
	{
	    xmlNodePtr iqn_node;

	    if (ret == -ADMIND_ERR_NODE_DOWN)
		continue;

	    EXA_ASSERT_VERBOSE(ret == EXA_SUCCESS, "Unexpected error code");

            /* Whenever a volume is not exported, we do not report an error but
             * simply swallow the error */
            if (iqn_reply.ret == -VRT_ERR_VOLUME_NOT_EXPORTED)
                continue;

            /* When target has no more entry, it replies -ENOENT so here, we
             * need to catch the value.
             * XXX a better way to do this would probably be to pass a
             * 'continue' flag into reply to signal that data are still
             * available. */
            if (iqn_reply.ret == -ENOENT)
                continue;

	    EXA_ASSERT_VERBOSE(gateway_node[nodeid] != NULL, "Unexpected node replying");

	    if (compound_ret != EXA_SUCCESS)
		continue;

	    if (iqn_reply.ret != EXA_SUCCESS)
	    {
		compound_ret = iqn_reply.ret;
		continue;
	    }

	    found_iqn = true;
	    iqn_node = xmlNewChild(gateway_node[nodeid], NULL, BAD_CAST("iqn"), NULL);
	    if (iqn_node == NULL)
	    {
		exalog_error("Failed to allocate XML structures");
		compound_ret = EXA_ERR_XML_ADD;
		continue;
	    }

	    compound_ret = xml_set_prop(iqn_node, "id", iqn_to_str(&iqn_reply.iqn));
	}

	if (compound_ret != EXA_SUCCESS)
	    return compound_ret;

	iqn_request.iqn_num++;
    } while(found_iqn);

    return compound_ret;
}

int cluster_clinfo_export_by_volume(int thr_nb, xmlNodePtr father_node,
				    const exa_uuid_t *volume_uuid)
{
    export_info_t *export_infos;
    unsigned int export_nb, i;
    int result;

    /* Get the description of the exports from the LUM service */
    result = lum_exports_get_info(&export_infos);
    if (result < 0)
        return result;

    export_nb = result;

    result = EXA_SUCCESS;
    for (i = 0; i < export_nb && result == EXA_SUCCESS; i++)
    {
	if (!uuid_is_equal(&export_infos[i].uuid , volume_uuid))
	    continue;

	result = cluster_clinfo_export(thr_nb, father_node, &export_infos[i]);
    }

    os_free(export_infos);

    return result;
}

