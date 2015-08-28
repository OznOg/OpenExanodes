
/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_CONFIG_H
#define _EXA_CONFIG_H

/** \file exa_config.h
 * \brief Configuration file parsing routines
 */
#include <os/include/os_stdio.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "os/include/os_inttypes.h"

#include "common/include/uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXA_CONF_FORMAT_VERSION       1

/* --- Syntax elements ----------------------------------------------- */
#define       EXA_CONF_CLUSTER			"cluster"
#define       EXA_CONF_CLUSTER_NAME		"name"
#define       EXA_CONF_CLUSTER_UUID		"uuid"
#define       EXA_CONF_EXANODES			"Exanodes"
#define       EXA_CONF_GROUP			"diskgroup"
#define       EXA_CONF_GROUP_NAME		"name"
#define       EXA_CONF_SPARE			"nb_spare"
#define       EXA_CONF_LOGICAL			"logical"
#define       EXA_CONF_PHYSICAL			"physical"
#define       EXA_CONF_LOGICAL_VOLUME		"volume"
#define       EXA_CONF_LOGICAL_VOLUME_NAME	"name"
#define       EXA_CONF_NODE			"node"
#define       EXA_CONF_NODE_HOSTNAME		"hostname"
#define       EXA_CONF_NODE_NAME		"name"
#define       EXA_CONF_NODE_NUMBER		"number"
#define       EXA_CONF_DISK			"disk"
#define       EXA_CONF_DISK_NAME		"name"
#define       EXA_CONF_DISK_PATH                "path"
#define       EXA_CONF_XML_DATANETWORK          "datanetwork"
#define       EXA_CONF_SEPARATOR  ':'

/* A few defaults for software */
#define       EXA_CONF_SOFTWARE			"software"
#define       EXA_CONF_SOFTWARE_NODE		"node"
#define       EXA_CONF_SOFTWARE_NODE_NAME	"name"
#define       EXA_CONF_SOFTWARE_NODE_STATUS	"status"

/* A few definitions for software subtree */
#define       EXA_CONF_SERVICE			"service"

/* filesystem :  XML constants for tags and attributes */
#define EXA_CONF_FS                          "fs"
#define EXA_CONF_FS_MOUNTPOINT               "mountpoint"
#define EXA_CONF_FS_SIZE                     "size"
#define EXA_CONF_FS_TYPE                     "type"
#define EXA_CONF_FS_USED                     "used"
#define EXA_CONF_FS_HANDLE_GFS               "handle_sfs"
#define EXA_CONF_GFS_NB_LOGS                 "nb-logs"
#define EXA_CONF_GFS_UUID		     "uuid"

/* Filesystem names */
#define FS_NAME_EXT3                     "ext3"
#define FS_NAME_XFS                      "xfs"
#define FS_NAME_GFS                      "sfs"

/* GFS Lock protocol names */
#define GFS_LOCK_DLM                     "lock_dlm"
#define GFS_LOCK_GULM                    "lock_gulm"

/* GFS Options Names */
#define EXA_OPTION_FS_GFS_LOCK_PROTOCOL           "sfs_lock_protocol"
#define EXA_OPTION_FS_GULM_MASTERS                "gulm_masters"
#define EXA_OPTION_FS_GFS_READ_AHEAD              "sfs_read_ahead"

#define EXA_CONF_MONITORING              "monitoring"
#define EXA_CONF_MONITORING_SNMPD_HOST   "snmpd_host"
#define EXA_CONF_MONITORING_SNMPD_PORT   "snmpd_port"
#define EXA_CONF_MONITORING_STARTED_ON   "started_on"
#define EXA_CONF_MONITORING_STOPPED_ON   "stopped_on"
#define EXA_CONF_MONITORING_STARTED      "started"

/* --- Parser API ---------------------------------------------------- */

void          xml_conf_init ( void );
xmlDocPtr     xml_conf_init_from_file ( const char * filename);
xmlDocPtr     xml_conf_init_from_buf(const char *buffer, size_t len);

bool          xml_conf_xpath_predicate(const xmlDocPtr tree, const char *xpath);
xmlNodeSetPtr xml_conf_xpath_query (const xmlDocPtr tree, const char *fmt, ...);
void          __xml_conf_xpath_free (xmlNodeSetPtr nodeSet);
#define xml_conf_xpath_free(nodeSet) \
    (__xml_conf_xpath_free(nodeSet), nodeSet = NULL)

int           xml_conf_xpath_result_entity_count (xmlNodeSetPtr);
xmlNodePtr    xml_conf_xpath_result_entity_get (xmlNodeSetPtr, int);
xmlNodePtr    xml_conf_xpath_singleton(xmlDoc *tree, const char *fmt, ...);

#define xml_conf_xpath_result_for_each(nodeSet, node, i)            \
   for(i = 0, node = xml_conf_xpath_result_entity_get(nodeSet, i) ; \
       i < xml_conf_xpath_result_entity_count(nodeSet) ;            \
       i++, node = xml_conf_xpath_result_entity_get(nodeSet, i))

const char *  xml_get_prop(const xmlNode *node, const char *name);
xmlDocPtr     xml_new_doc (const char * version);
xmlNodePtr    xml_new_doc_node (xmlDocPtr doc, xmlNsPtr ns, const char * name,
				const char * content);
xmlNodePtr    xml_new_child (xmlNodePtr parent, xmlNsPtr ns, const char * name,
			     const char * content);

/************************************************************/

int xml_set_prop(xmlNodePtr xml_node, const char *name, const char *value);
int xml_set_prop_bool(xmlNodePtr xml_node, const char *name, int value);
int xml_set_prop_ok(xmlNodePtr xml_node, const char *name, int value);
int xml_set_prop_u64(xmlNodePtr xml_node, const char *name, uint64_t value);
int xml_set_prop_uuid(xmlNodePtr xml_node, const char *name, const exa_uuid_t *value);

xmlNodePtr xml_get_child(xmlNodePtr parent, const char *node_name,
			 const char *prop_name, const char *prop_value);


#ifdef __cplusplus
}
#endif

#endif  // _EXA_CONFIG_H
