/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/adm_deserialize.h"

#include <libxml/parser.h>

#include "os/include/os_error.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_network.h"
#include "os/include/os_string.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "os/include/os_disk.h"

#include "config/exa_version.h"

#include "common/include/exa_version.h"
#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "vrt/common/include/spof.h"
#include "common/include/exa_conversion.h"

#include "log/include/log.h"

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_error.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nic.h"
#include "admind/src/adm_disk.h"
#include "admind/src/adm_group.h"
#include "admind/src/adm_volume.h"

#include "admind/services/rdev/include/rdev_config.h"

#ifdef WITH_FS
#include "admind/services/fs/generic_fs.h"
#include "admind/src/adm_fs.h"
#include "fs/include/fs_data.h"
#endif

#include "target/iscsi/include/lun.h"
#include "admind/include/service_lum.h"

typedef enum {
    ROOT,
    EXANODES,
    CLUSTER,
    CLUSTER_NODE,
    CLUSTER_NODE_NETWORK,
    CLUSTER_NODE_DISK,
    CLUSTER_SPOFS,
    CLUSTER_SPOFS_SPOF,
    CLUSTER_SPOFS_SPOF_NODE,
    DISKGROUP,
    DISKGROUP_PHYSICAL,
    DISKGROUP_PHYSICAL_DISK,
    DISKGROUP_LOGICAL,
    DISKGROUP_LOGICAL_VOLUME,
    DISKGROUP_LOGICAL_VOLUME_FS,
    TUNABLES,
    TUNABLES_TUNABLE,
} adm_deserialize_element_t;

typedef struct
{
    adm_deserialize_element_t element;
    union {
	struct adm_node *node;
	struct adm_net *net;
	struct adm_group *group;
	struct adm_volume *volume;
    } current;
    int create;
    int failed;
    spof_id_t cur_spof_id;
    char *error_msg;
    const char *element_str;
    const char *identifier_str;
} adm_deserialize_state_t;


/**
 * @brief Validates and copies a value of an attribute to a buffer.
 *
 * @param[out] state    State of the validation
 * @param[out] dest     The buffer
 * @param[in] src       The value of the attribute
 * @param[in] maxlen    The max allowed length of the attribute
 * @param[in] accept    The allowed characters to validate against
 * @param[in] attribute The attribute name
 */
static void
adm_validate_string(adm_deserialize_state_t *state, char *dest,
		    const char *src, unsigned int maxlen, const char *accept,
		    const char *attribute)
{
    unsigned int len;
    unsigned int accepted_len;

    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute '%s' in <%s>", attribute,
		    state->element_str);
	state->failed = true;
	return;
    }

    len = strnlen(src, maxlen + 1);

    if (len == 0)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Empty value for attribute '%s' in <%s>", attribute,
		    state->element_str);
	state->failed = true;
	return;
    }

    if (len > maxlen)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Too long value for attribute '%s' in <%s> (max %u)",
		    attribute, state->element_str, maxlen);
	state->failed = true;
	return;
    }

    accepted_len = strspn(src, accept);
    if (accepted_len != len)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid character '%c' in attribute '%s' in <%s>",
		    src[accepted_len], attribute, state->element_str);
	state->failed = true;
	return;
    }

    if (dest != NULL)
	strlcpy(dest, src, maxlen + 1);
}

#ifdef WITH_FS
static void
__adm_validate_gfs_uuid(adm_deserialize_state_t *state, char *dest,
                        const char *src, size_t maxlen, const char *attribute)
{
    /* FIXME We should have some function dedicated to checking the validity
     * of a GFS UUID (like uuid_scan() for exa_uuid_t). */
    adm_validate_string(state, dest, src, maxlen,
                        EXACONFIG_GFS_UUID_REGEXP_EXPANDED,
                        attribute);
}
#define adm_validate_gfs_uuid(state, dest, src) \
    __adm_validate_gfs_uuid(state, dest, src, sizeof(dest) - 1, #src)
#endif  /* WITH_FS */

static void
__adm_validate_exaname(adm_deserialize_state_t *state, char *dest,
		       const char *src, size_t maxlen, const char *attribute)
{
    adm_validate_string(state, dest, src, maxlen,
                        EXACONFIG_EXANAME_REGEXP_EXPANDED,
                        attribute);
}
#define adm_validate_exaname(state, dest, src)				\
    __adm_validate_exaname(state, dest, src, sizeof(dest)-1, #src)


static void
__adm_validate_posixname(adm_deserialize_state_t *state, char *dest,
			 const char *src, size_t maxlen, const char *attribute)
{
    adm_validate_string(state, dest, src, maxlen,
                        EXACONFIG_POSIXNAME_REGEXP_EXPANDED,
                        attribute);
}
#define adm_validate_posixname(state, dest, src)			\
    __adm_validate_posixname(state, dest, src, sizeof(dest)-1, #src)

#ifdef WITH_FS
static void
__adm_validate_mountpoint(adm_deserialize_state_t *state, char *dest,
		    const char *src, size_t maxlen, const char *attribute)
{
    adm_validate_string(state, dest, src, maxlen,
                        EXACONFIG_MOUNTPOINT_REGEXP_EXPANDED,
                        attribute);
}
#define adm_validate_mountpoint(state, dest, src)			\
    __adm_validate_mountpoint(state, dest, src, sizeof(dest)-1, #src)
#endif

static void
__adm_validate_disk(adm_deserialize_state_t *state, char *dest,
                    const char *src, size_t maxlen, const char *attribute)
{
    cl_error_desc_t err_desc;
    char normalized[maxlen + 1];

    if (os_disk_normalize_path(src, normalized, sizeof(normalized)) != 0)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Failed to normalize disk path '%s' in <%s>",
		    src, state->element_str);
	state->failed = true;
    }

    if (!os_disk_path_is_valid(normalized))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid disk path '%s'", normalized);
	state->failed = true;
    }

    /* Check if the device is locally available */
    if (state->current.node == adm_myself())
    {
	bool available;

        /* XXX This is awfully suboptimal, as rdev_is_path_available()
               allocates a buffer, reads the disk conf file into it, and
               deallocates it every time it's called! */
	available = rdev_is_path_available(normalized, &err_desc);
	if (err_desc.code != EXA_SUCCESS)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			err_desc.msg);
	    state->failed = true;
	}
	else if (!available)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Disk path '%s' is not available for Exanodes", normalized);
	    state->failed = true;
	}
    }

    adm_validate_string(state, dest, normalized, maxlen,
                        EXACONFIG_DEVPATH_REGEXP_EXPANDED,
                        attribute);
}
#define adm_validate_disk(state, dest, src) \
        __adm_validate_disk(state, dest, src, sizeof(dest) - 1, #src)

#ifdef WITH_FS
static void
__adm_validate_option(adm_deserialize_state_t *state, char *dest,
		      const char *src, size_t maxlen, const char *attribute)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute '%s' in <%s>", attribute, state->element_str);
	state->failed = true;
	return;
    }

    /* An option is... optional */
    if (src[0] == '\0')
        return;

    adm_validate_string(state, dest, src, maxlen,
			EXA_FS_MOUNT_OPTION_ACCEPTED_CHARS,
			attribute);
}
#define adm_validate_option(state, dest, src)				\
    __adm_validate_option(state, dest, src, sizeof(dest)-1, #src)
#endif /* WITH_FS */

static void
adm_validate_uuid(adm_deserialize_state_t *state, exa_uuid_t *dest,
		  const char *src)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'uuid' in <%s>", state->element_str);
	state->failed = true;
	return;
    }

    if (uuid_scan(src, dest) != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid format for attribute 'uuid' in <%s>",
		    state->element_str);
	state->failed = true;
    }
}


static void
adm_validate_group_goal(adm_deserialize_state_t *state,
			adm_group_goal_t *dest, const char *src)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'goal' in <%s>", state->element_str);
	state->failed = true;
	return;
    }

    if (strcmp(src, ADMIND_PROP_STOPPED) == 0)
    {
	*dest = ADM_GROUP_GOAL_STOPPED;
    }
    else if (strcmp(src, ADMIND_PROP_STARTED) == 0)
    {
	*dest = ADM_GROUP_GOAL_STARTED;
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute 'goal' in <%s>"
		    " (must be " ADMIND_PROP_STOPPED " or " ADMIND_PROP_STARTED ")",
		    state->element_str);
	state->failed = true;
    }
}


static void
adm_validate_accessmode(adm_deserialize_state_t *state, int *dest,
			const char *src)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'accessmode' in <%s>",
		    state->element_str);
	state->failed = true;
	return;
    }

    if (strcmp(src, ADMIND_PROP_SHARED) == 0)
    {
	*dest = true;
    }
    else if (strcmp(src, ADMIND_PROP_PRIVATE) == 0)
    {
	*dest = false;
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute accessmode in <%s>"
		    " (must be " ADMIND_PROP_SHARED " or " ADMIND_PROP_PRIVATE ")",
		    state->element_str);
	state->failed = true;
    }
}


static void
adm_validate_transaction(adm_deserialize_state_t *state, int *dest,
			 const char *src)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'transaction' in <%s>",
		    state->element_str);
	state->failed = true;
	return;
    }

    if (strcmp(src, ADMIND_PROP_COMMITTED) == 0)
    {
	*dest = true;
    }
    else if (strcmp(src, ADMIND_PROP_INPROGRESS) == 0)
    {
	*dest = false;
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute accessmode in <%s>"
		    " (must be " ADMIND_PROP_COMMITTED " or " ADMIND_PROP_INPROGRESS ")",
		    state->element_str);
	state->failed = true;
    }
}


static void
__adm_validate_boolean(adm_deserialize_state_t *state, int *dest,
		       const char *src, const char *attribute)
{
    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    if (strcmp(src, ADMIND_PROP_TRUE) == 0)
    {
	*dest = true;
    }
    else if (strcmp(src, ADMIND_PROP_FALSE) == 0)
    {
	*dest = false;
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute '%s' in <%s>"
		    " (must be " ADMIND_PROP_TRUE " or " ADMIND_PROP_FALSE ")",
		    attribute, state->element_str);
	state->failed = true;
    }
}
#define adm_validate_boolean(state, dest, src)		\
    __adm_validate_boolean(state, dest, src, #src)


static void
__adm_validate_uint64(adm_deserialize_state_t *state, uint64_t *dest,
		      const char *src, const char *attribute)
{
    char *endptr;
    uint64_t val;

    if (src == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    errno = 0;    /* To distinguish success/failure after call */

    val = strtoull(src, &endptr, 10);

    if ((errno == ERANGE && val == UINT64_MAX)
	|| (errno != 0 && val == 0))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid range for attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    if (endptr == src)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Empty value for attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    /* If we got here, strtol() successfully parsed a number */

    if (*endptr != '\0')
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Trailing chars after the value for attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    *dest = val;
}
#define adm_validate_uint64(state, dest, src)		\
    __adm_validate_uint64(state, dest, src, #src)


static void
__adm_validate_uint32(adm_deserialize_state_t *state, uint32_t *dest,
		      const char *src, const char *attribute)
{
    uint64_t tmp;

    __adm_validate_uint64(state, &tmp, src, attribute);
    if (state->failed)
	return;

    if (tmp > UINT32_MAX)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid range for attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
	return;
    }

    *dest = (uint32_t)tmp;
}
#define adm_validate_uint32(state, dest, src)		\
    __adm_validate_uint32(state, dest, src, #src)


static void
adm_validate_nodeid(adm_deserialize_state_t *state, exa_nodeid_t *dest,
		    const char *src)
{
    unsigned int tmp;

    __adm_validate_uint32(state, &tmp, src, "number");
    if (state->failed)
	return;

    if (!EXA_NODEID_VALID(tmp))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute 'number' in <%s> (max %u)",
		    state->element_str, EXA_MAX_NODES_NUMBER);
	state->failed = true;
	return;
    }

    *dest = (exa_nodeid_t)tmp;
}


static void
__adm_validate_nodeset(adm_deserialize_state_t *state, exa_nodeset_t *dest,
		       const char *src, const char *attribute)
{
    if (exa_nodeset_from_hex(dest, src) != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid value for attribute '%s' in <%s>",
		    attribute, state->element_str);
	state->failed = true;
    }
}
#define adm_validate_nodeset(state, dest, src)		\
    __adm_validate_nodeset(state, dest, src, #src)


static void
adm_deserialize_exanodes(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    const char *release = NULL;
    exa_version_t running_major, config_major;
    uint32_t format_version = 0; /* We'll use 0 as default format if it's not
                                  * specified, as we broke compatibility during
                                  * the sprint in which we introduced this
                                  * format version. */

    EXA_ASSERT(adm_cluster.version == -1);

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("release")))
	    release = (char *)attrs[1];
	else if (!state->create && xmlStrEqual(attrs[0], BAD_CAST("config_version")))
	{
	    if (to_int64((char *)attrs[1], &adm_cluster.version))
	    {
		os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			    "Cannot parse configuration version '%s' in /Exanodes", attrs[1]);
		goto error;
	    }
	}
	else if (!state->create && xmlStrEqual(attrs[0], BAD_CAST("format_version")))
	{
	    if (to_uint32((char *)attrs[1], &format_version))
	    {
		os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			    "Cannot parse configuration format version '%s' in "
                            "/Exanodes", attrs[1]);
		goto error;
	    }
	}
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes", attrs[0]);
	    goto error;
	}
    }

    if (!state->create && format_version != EXA_CONF_FORMAT_VERSION)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Wrong format version '%"PRIu32"' in /Exanodes; "
                    "expected %"PRIu32, format_version,
                    EXA_CONF_FORMAT_VERSION);
	goto error;
    }

    if (state->create)
    {
	if (release == NULL)
	    release = EXA_VERSION;
	adm_cluster.version = 0;
        format_version = EXA_CONF_FORMAT_VERSION;
    }

    if (release == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'release' in /Exanodes");
	goto error;
    }

    if (!exa_version_get_major(EXA_VERSION, running_major))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Can not get major for running version %s",
                    EXA_VERSION);
	goto error;
    }

    if (!exa_version_get_major(release, config_major))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Can not get major for configuration file version %s",
                    release);
	goto error;
    }

    if (!exa_version_is_equal(config_major, running_major))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "The configuration file describes a cluster for Exanodes "
                    "%s, which is incompatible with this release of Exanodes %s",
		    release, EXA_VERSION);
	goto error;
    }

    if (adm_cluster.version < 0)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'version' in /Exanodes");
	goto error;
    }

    return;

error:
    state->failed = true;
}

/**
 * Deserialize a cluster from an xml tree.
 *
 * Sets the persistent goal on success; leaves it unchanged on failure.
 */
static void
adm_deserialize_cluster(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    const char *name = NULL;
    const char *uuid = NULL;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("uuid")))
	    uuid = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster", attrs[0]);
	    state->failed = true;
	    return;
	}
    }

    adm_validate_exaname(state, adm_cluster.name, name);
    adm_validate_uuid(state, &adm_cluster.uuid, uuid);
    if (state->failed)
	return;
}



static void
adm_deserialize_monitoring(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    const char *snmpd_host_attr = NULL;
    const char *snmpd_port_attr = NULL;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST(EXA_CONF_MONITORING_SNMPD_HOST)))
	    snmpd_host_attr = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST(EXA_CONF_MONITORING_SNMPD_PORT)))
	    snmpd_port_attr = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/%s/%s",
			attrs[0], EXA_CONF_CLUSTER, EXA_CONF_MONITORING);
	    state->failed = true;
	    return;
	}
    }

    if (snmpd_host_attr != NULL)
	adm_validate_posixname(state,
			       adm_cluster.monitoring_parameters.snmpd_host,
			       snmpd_host_attr);

    if (snmpd_port_attr != NULL)
	adm_validate_uint32(state,
			    &adm_cluster.monitoring_parameters.snmpd_port,
			    snmpd_port_attr);

    adm_cluster.monitoring_parameters.started =
        strcmp("", adm_cluster.monitoring_parameters.snmpd_host);

}

/* Used to set the node's SPOF id from the XML configuration file
 * spof_id property of a <node>.
 */
static void adm_validate_spof_id(adm_deserialize_state_t *state,
                                struct adm_node *node,
                                const char *spof_id_str)
{
    spof_id_t spof_id;

    if (adm_node_get_spof_id(node) != SPOF_ID_NONE)
    {
        /* This should never happen unless the user fiddled with
         * exanodes.conf with his greasy fingers.
         */
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Node '%s' is already in SPOF group %"PRIspof_id,
                    node->name, adm_node_get_spof_id(node));
        state->failed = true;
        return;
    }

    if (spof_id_str != NULL)
    {
        /* Set the node's SPOF id from the string */
        if (spof_id_from_str(&spof_id, spof_id_str) != EXA_SUCCESS)
        {
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Invalid spof_id '%s' for node '%s'",
                        spof_id_str, node->name);
            state->failed = true;
        }
        else
            adm_node_set_spof_id(node, spof_id);
    } else
        /* The node's SPOF id isn't set; it'll be put in its own
         * SPOF at the end of the parsing.
         * This helps with backward compatilibity.
         */
        adm_node_set_spof_id(node, SPOF_ID_NONE);
}

static struct adm_node *
adm_deserialize_node(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_node *node;
    const char *name = NULL;
    const char *hostname = NULL;
    const char *number = NULL;
    const char *spof_id_str = NULL;
    int ret;

    if (adm_cluster_nb_nodes() == EXA_MAX_NODES_NUMBER)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Too many nodes (max %u)", EXA_MAX_NODES_NUMBER);
	goto error;
    }

    node = adm_node_alloc();
    if (node == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	goto error;
    }

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("hostname")))
	    hostname = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("number")))
	    number = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("spof_id")))
	    spof_id_str = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/node[@name=\"%s\"]",
			attrs[0], name);
	    goto error;
	}
    }

    adm_validate_posixname(state, node->name, name);
    adm_validate_posixname(state, node->hostname, hostname);
    adm_validate_nodeid(state, &node->id, number);
    adm_validate_spof_id(state, node, spof_id_str);

    if (state->failed)
        goto error;

    ret = adm_cluster_insert_node(node);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate nodes /Exanodes/cluster/node[@name=\"%s\" or @number=\"%u\"]",
		    name, node->id);
	goto error;
    }

    return node;

error:
    if (node != NULL)
        adm_node_delete(node);
    state->failed = true;
    return NULL;
}


static void
adm_deserialize_nic(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_nic *nic;
    char hostname[EXA_MAXSIZE_HOSTNAME + 1];
    int ret;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("hostname")))
        {
	    adm_validate_posixname(state, hostname, (char *)attrs[1]);
            if (state->failed)
	        os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Missing hostname attribute in "
                        "/Exanodes/cluster/node[@name=\"%s\"]/network",
			attrs[0], state->current.node->name);
        }
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/node[@name=\"%s\"]/network",
			attrs[0], state->current.node->name);
	    goto error;
	}
    }

    if (state->failed)
	return;

    ret = adm_nic_new(hostname, &nic);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Failed to create new nic with hostname='%s': %s (%d)",
		    hostname, exa_error_msg(ret), ret);
	goto error;
    }

    ret = adm_node_insert_nic(state->current.node, nic);
    if (ret == -EEXIST)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate nodes /Exanodes/cluster/node[@name=\"%s\"]/network",
		    state->current.node->name);
	goto error;
    }
    else if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Too many nodes in /Exanodes/cluster/node[@name=\"%s\"]/network",
		    state->current.node->name);
	goto error;
    }

    return;

error:
    state->failed = true;
}


static void
adm_deserialize_disk(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_disk *disk;
    const char *path = NULL;
    const char *uuid = NULL;
    int ret;

    disk = adm_disk_alloc();
    if (disk == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	goto error;
    }

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (state->create && xmlStrEqual(attrs[0], BAD_CAST("path")))
	    path = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("uuid")))
	    uuid = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/node[@name=\"%s\"]/disk",
			attrs[0], state->current.node->name);
	    goto error;
	}
    }

    if (state->create)
        adm_validate_disk(state, disk->path, path);

    adm_validate_uuid(state, &disk->uuid, uuid);
    if (state->failed)
        goto error;

    if (state->current.node == adm_myself())
    {
	ret = adm_disk_local_new(disk);
	if (ret != -EXA_SUCCESS)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	    goto error;
	}
    }

    ret = adm_cluster_insert_disk();
    if (ret == -ADMIND_ERR_TOO_MANY_DISKS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Too many disks in the cluster (max %u)", NBMAX_DISKS);
	goto error;
    }
    EXA_ASSERT(ret == EXA_SUCCESS);

    ret = adm_node_insert_disk(state->current.node, disk);
    if (ret == -EEXIST)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate nodes /Exanodes/cluster/node/disk[@uuid=\"%s\"]",
		    uuid);
	goto error;
    }
    else if (ret == -ADMIND_ERR_TOO_MANY_DISKS_IN_NODE)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Too many disks in node %s (max %u)", state->current.node->name,
		    NBMAX_DISKS_PER_NODE);
	goto error;
    }
    EXA_ASSERT(ret == EXA_SUCCESS);

    return;

error:
    if (disk != NULL)
        adm_disk_delete(disk);

    state->failed = true;
}


static struct adm_group *
adm_deserialize_group(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_group *group = NULL;
    const char *name = NULL;
    const char *layout = NULL;
    const char *uuid = NULL;
    const char *transaction = NULL;
    const char *goal = NULL;
    const char *tainted = NULL;
    int ret;

    group = adm_group_alloc();
    if (group == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	goto error;
    }

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("layout")))
	    layout = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("uuid")))
	    uuid = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("transaction")))
	    transaction = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("goal")))
	    goal = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("tainted")))
	    tainted = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/diskgroup[@name=\"%s\"]",
			attrs[0], name);
	}
    }

    adm_validate_exaname(state, group->name, name);
    if (layout == NULL) {
        os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
        	    "Unspecified layout type for group \"%s\"", group->name);
        goto error;
    }

    group->layout = vrt_layout_from_name(layout);
    if (!VRT_LAYOUT_IS_VALID(group->layout))
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Invalid layout \"%s\" for group \"%s\"", layout,
                    group->name);
        state->failed = true;
        goto error;
    }
    adm_validate_uuid(state, &group->uuid, uuid);
    adm_validate_transaction(state, &group->committed, transaction);
    adm_validate_group_goal(state, &group->goal, goal);
    adm_validate_boolean(state, &group->tainted, tainted);
    if (state->failed)
	goto error;

    group->sb_version = sb_version_load(&group->uuid);
    if (group->sb_version == NULL)
    {
        /* Either the sb version will be correct, either it will be
         * out-of-date and synched during the VRT recovery up
         */
        group->sb_version = sb_version_new(&group->uuid);
    }

    if (group->sb_version == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Can't load superblock version for group \"" UUID_FMT "\"",
		    UUID_VAL(&group->uuid));
	goto error;
    }

    group->started = false;
    group->synched = false;

    ret = adm_group_insert_group(group);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate nodes /Exanodes/cluster/diskgroup[@name=\"%s\" or @uuid=\"" UUID_FMT "\"]",
		    group->name, UUID_VAL(&group->uuid));
	goto error;
    }

    return group;

error:
    if (group != NULL)
        adm_group_free(group);

    state->failed = true;
    return NULL;
}


static void
adm_deserialize_group_disk(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    const char *uuid_str = NULL;
    const char *vrt_uuid_str = NULL;
    struct adm_disk *disk;
    exa_uuid_t uuid;
    int ret;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("uuid")))
	    uuid_str = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("vrt_uuid")))
	    vrt_uuid_str = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/diskgroup[@name=\"%s\"]/disk",
			attrs[0], state->current.group->name);
	    goto error;
	}
    }

    adm_validate_uuid(state, &uuid, uuid_str);
    if (state->failed)
	return;

    disk = adm_cluster_get_disk_by_uuid(&uuid);

    if (disk == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Unknown uuid in /Exanodes/cluster/diskgroup[@name=\"%s\"]/disk[@uuid=\"%s\"]",
		    state->current.group->name, uuid_str);
	goto error;
    }

    adm_validate_uuid(state, &disk->vrt_uuid, vrt_uuid_str);
    if (state->failed)
	return;

    ret = adm_group_insert_disk(state->current.group, disk);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate nodes /Exanodes/cluster/diskgroup[@name=\"%s\"]/disk",
		    state->current.group->name);
	goto error;
    }

    return;

error:
    state->failed = true;
}


static struct adm_volume *
adm_deserialize_volume(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_volume *volume;
    const char *name = NULL;
    const char *size = NULL;
    const char *accessmode = NULL;
    const char *transaction = NULL;
    const char *goal_stopped = NULL;
    const char *goal_started = NULL;
    const char *goal_readonly = NULL;
    const char *uuid = NULL;
    const char *readahead = NULL;
    int ret;

    EXA_ASSERT(state->current.group);

    volume = adm_volume_alloc();
    if (volume == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	goto error;
    }

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("size")))
	    size = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("accessmode")))
	    accessmode = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("transaction")))
	    transaction = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("goal_stopped")))
	    goal_stopped = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("goal_started")))
	    goal_started = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("goal_readonly")))
	    goal_readonly = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("uuid")))
	    uuid = (char *)attrs[1];
        else if (xmlStrEqual(attrs[0], BAD_CAST("readahead")))
	    readahead = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/diskgroup[@name=\"%s\"]/volume[@name=\"%s\"]",
			attrs[0], state->current.group->name, name);
	    goto error;
	}
    }

    adm_validate_exaname(state, volume->name, name);
    adm_validate_uint64(state, &volume->size, size);
    adm_validate_accessmode(state, &volume->shared, accessmode);
    adm_validate_transaction(state, &volume->committed, transaction);
    adm_validate_nodeset(state, &volume->goal_stopped, goal_stopped);
    adm_validate_nodeset(state, &volume->goal_started, goal_started);
    adm_validate_nodeset(state, &volume->goal_readonly, goal_readonly);
    adm_validate_uuid(state, &volume->uuid, uuid);

    /* This allows to transition the configuration file from clusters that
     * didn't handle ISCSI + bdev at the same time.
     * ISCSI "volumes" had no readahead field and their readahead struct
     * member was set to 0.
     */
    if (readahead == NULL)
        readahead = "0";

    /* FIXME ISCSI the readahead field should maybe be somewhere where it belongs,
     * like in the exports file.
     */
    adm_validate_uint32(state, &volume->readahead, readahead);

    if (state->failed)
	goto error;

    ret = adm_group_insert_volume(state->current.group, volume);
    if (ret != EXA_SUCCESS)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Duplicate node /Exanodes/cluster/diskgroup[@name=\"%s\"]/volume[@name=\"%s\" or @uuid=\"%s\"]",
		    state->current.group->name, name, uuid);
	goto error;
    }

    return volume;

error:
    if (volume != NULL)
        adm_volume_free(volume);
    state->failed = true;
    return NULL;
}


#ifdef WITH_FS
static struct adm_fs *
adm_deserialize_fs(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_fs *fs;
    const char *size = NULL;
    const char *type = NULL;
    const char *sfs_uuid = NULL;
    const char *sfs_nb_logs = NULL;
    const char *sfs_readahead = NULL;
    const char *sfs_rg_size = NULL;
    const char *sfs_fuzzy_statfs = NULL;
    const char *sfs_demote_secs = NULL;
    const char *sfs_glock_purge = NULL;
    const char *mountpoint = NULL;
    const char *mount_option = NULL;
    const char *transaction = NULL;
    const fs_definition_t *fs_definition = NULL;

    fs = adm_fs_alloc();
    if (fs == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "Failed to alloc memory");
	goto error;
    }

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("size")))
	    size = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("type")))
	    type = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_uuid")))
	    sfs_uuid = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_nb_logs")))
	    sfs_nb_logs = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_readahead")))
	    sfs_readahead = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_rg_size")))
	    sfs_rg_size = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_fuzzy_statfs")))
	    sfs_fuzzy_statfs = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_demote_secs")))
	    sfs_demote_secs = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("sfs_glock_purge")))
	    sfs_glock_purge = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("mountpoint")))
	    mountpoint = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("mount_option")))
	    mount_option = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("transaction")))
	    transaction = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/cluster/filesystems/fs",
			attrs[0]);
	    goto error;
	}
    }

    adm_validate_exaname(state, fs->type, type);
    if (state->failed)
	goto error;

    /* FIXME calling fs_get_definition make the include
     * admind/services/fs/generic_fs.h necessary and link to the service fs lib
     * mandatory for this function to be used. This is really ugly. */
    fs_definition = fs_get_definition(type);
    if (fs_definition == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Unknown filesystem type '%s'", type);
	goto error;
    }

    adm_validate_mountpoint(state, fs->mountpoint, mountpoint);
    adm_validate_option(state, fs->mount_option, mount_option);
    adm_validate_transaction(state, &fs->committed, transaction);

    /* Don't use fs->committed below if it is not correctly parsed. */
    if (state->failed)
	goto error;

    adm_validate_uint64(state, &fs->size, size);

    if (strcmp(type, "sfs") == 0 && (fs->committed))
    {
	adm_validate_gfs_uuid(state, fs->gfs_uuid, sfs_uuid);
	adm_validate_uint32(state, &fs->gfs_nb_logs, sfs_nb_logs);
	adm_validate_uint64(state, &fs->gfs_readahead, sfs_readahead);
	adm_validate_uint64(state, &fs->gfs_rg_size, sfs_rg_size);
	adm_validate_boolean(state, (int *)&fs->gfs_fuzzy_statfs, sfs_fuzzy_statfs);
	adm_validate_uint32(state, &fs->gfs_demote_secs, sfs_demote_secs);
	adm_validate_uint32(state, &fs->gfs_glock_purge, sfs_glock_purge);
    }

    if (state->failed)
	goto error;

    EXA_ASSERT(state->element == DISKGROUP_LOGICAL_VOLUME_FS);
    state->current.volume->filesystem = fs;
    fs->volume = state->current.volume;

    return fs;

error:
    if (fs != NULL)
        adm_fs_free(fs);
    state->failed = true;
    return NULL;
}
#endif


static void
adm_deserialize_tunable(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    const char *name = NULL;
    const char *default_value = NULL;
    const char *value = NULL;
    int ret;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("default_value")))
	    default_value = (char *)attrs[1];
	else if (xmlStrEqual(attrs[0], BAD_CAST("value")))
	    value = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanode/tunables/tunable[@name=\"%s\"]",
			(char *)attrs[0], name);
	    return;
	}
    }

    if (name == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'name' in /Exanode/tunables/tunable[@value=\"%s\"]",
		    value);
	state->failed = true;
	return;
    }

    if (!state->create && default_value == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Missing attribute 'default_value' in /Exanode/tunables/tunable[@name=\"%s\"]",
		    name);
	state->failed = true;
	return;
    }

    if (default_value != NULL)
    {
	ret = adm_cluster_set_param_default(name, default_value);
	if (ret == -EXA_ERR_SERVICE_PARAM_UNKNOWN)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Unknown tunable name '%s'", name);
	    state->failed = true;
	    return;
	}
	else if (ret != EXA_SUCCESS)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Invalid default_value '%s' for tunable '%s'",
			default_value, name);
	    state->failed = true;
	    return;
	}
    }

    if (value != NULL)
    {
	ret = adm_cluster_set_param(name, value);
	if (ret == -EXA_ERR_SERVICE_PARAM_UNKNOWN)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Unknown tunable name '%s'", name);
	    state->failed = true;
	    return;
	}
	else if (ret != EXA_SUCCESS)
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Invalid value '%s' for tunable '%s'", value, name);
	    state->failed = true;
	    return;
	}
    }
}

/* Used when parsing the configuration as sent from the exa_clcreate
 * command, where SPOFs are described as lists of nodes.
 */
static void adm_deserialize_spof_node(void *data, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_node *node;
    const char *name = NULL;

    for (; attrs && attrs[0]; attrs += 2)
    {
	if (xmlStrEqual(attrs[0], BAD_CAST("name")))
	    name = (char *)attrs[1];
	else
	{
	    os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
			"Extra attribute '%s' in /Exanodes/spof-groups/spof-group/node",
			(char *)attrs[0]);
            state->failed = true;
	    return;
	}
    }

    node = adm_cluster_get_node_by_name(name);
    if (node == NULL)
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Unknown node '%s' in SPOF group %"PRIspof_id,
		    name, state->cur_spof_id);
        state->failed = true;
	return;
    }

    if (adm_node_get_spof_id(node) != SPOF_ID_NONE)
    {
        /* We won't confuse the user with spof IDs, because implicit SPOFs
         * are done after explicit ones, so it'll appear in the same order
         * on his cmdline. */
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "Node '%s' is already in SPOF group %"PRIspof_id,
                    node->name, adm_node_get_spof_id(node));
        state->failed = true;
        return;
    }

    /* Assign the current SPOF id to the node found */
    adm_node_set_spof_id(node, state->cur_spof_id);
}

static void
adm_deserialize_start_element(void *data, const xmlChar *name, const xmlChar **attrs)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;

    if (state->failed)
	return;

    state->element_str = (char *)name;

    if (state->element == ROOT &&
	xmlStrEqual(name, BAD_CAST("Exanodes")))
    {
	state->element = EXANODES;
	adm_deserialize_exanodes(data, attrs);
    }
    else if (state->element == EXANODES &&
	     xmlStrEqual(name, BAD_CAST("cluster")))
    {
	state->element = CLUSTER;
	adm_deserialize_cluster(data, attrs);
    }
    else if (state->element == CLUSTER &&
	     xmlStrEqual(name, BAD_CAST(EXA_CONF_MONITORING)))
    {
	EXA_ASSERT(state->current.node == NULL);
	adm_deserialize_monitoring(data, attrs);
    }
    else if (state->element == CLUSTER &&
	     xmlStrEqual(name, BAD_CAST("node")))
    {
	EXA_ASSERT(state->current.node == NULL);
	state->element = CLUSTER_NODE;
	state->current.node = adm_deserialize_node(data, attrs);
    }
    else if (state->element == CLUSTER_NODE &&
	     xmlStrEqual(name, BAD_CAST("network")))
    {
	state->element = CLUSTER_NODE_NETWORK;
	adm_deserialize_nic(data, attrs);
    }
    else if (state->element == CLUSTER_NODE &&
	     xmlStrEqual(name, BAD_CAST("disk")))
    {
	state->element = CLUSTER_NODE_DISK;
	adm_deserialize_disk(data, attrs);
    }
    else if (!state->create &&
	     state->element == EXANODES &&
	     xmlStrEqual(name, BAD_CAST("diskgroup")))
    {
	EXA_ASSERT(state->current.group == NULL);
	state->element = DISKGROUP;
	state->current.group = adm_deserialize_group(data, attrs);
    }
    else if (!state->create &&
	     state->element == DISKGROUP &&
	     xmlStrEqual(name, BAD_CAST("physical")))
    {
	state->element = DISKGROUP_PHYSICAL;
    }
    else if (!state->create &&
	     state->element == DISKGROUP_PHYSICAL &&
	     xmlStrEqual(name, BAD_CAST("disk")))
    {
	state->element = DISKGROUP_PHYSICAL_DISK;
	adm_deserialize_group_disk(data, attrs);
    }
    else if (!state->create &&
	     state->element == DISKGROUP &&
	     xmlStrEqual(name, BAD_CAST("logical")))
    {
	state->element = DISKGROUP_LOGICAL;
    }
    else if (!state->create &&
	     state->element == DISKGROUP_LOGICAL &&
	     xmlStrEqual(name, BAD_CAST("volume")))
    {
	state->element = DISKGROUP_LOGICAL_VOLUME;
	state->current.volume = adm_deserialize_volume(data, attrs);
    }
#ifdef WITH_FS
    else if (!state->create &&
	     state->element == DISKGROUP_LOGICAL_VOLUME &&
	     xmlStrEqual(name, BAD_CAST("fs")))
    {
	state->element = DISKGROUP_LOGICAL_VOLUME_FS;
	adm_deserialize_fs(data, attrs);
    }
#endif
    else if (state->element == EXANODES &&
	     xmlStrEqual(name, BAD_CAST("tunables")))
    {
	state->element = TUNABLES;
    }
    else if (state->element == TUNABLES &&
	     xmlStrEqual(name, BAD_CAST("tunable")))
    {
	state->element = TUNABLES_TUNABLE;
	adm_deserialize_tunable(data, attrs);
    }
    else if (state->element == CLUSTER &&
	     xmlStrEqual(name, BAD_CAST("spof-groups")))
    {
	state->element = CLUSTER_SPOFS;
        /* Make sure we didn't already have a <spofs> node */
	EXA_ASSERT(state->cur_spof_id == SPOF_ID_NONE);
    }
    else if (state->element == CLUSTER_SPOFS &&
	     xmlStrEqual(name, BAD_CAST("spof-group")))
    {
	state->element = CLUSTER_SPOFS_SPOF;
        /* We start a new SPOF group, so increment the spof id
         * counter (First spof ID is 1)
         */
        if (state->cur_spof_id == SPOF_ID_NONE)
            state->cur_spof_id = 1;
        else
            state->cur_spof_id++;
    }
    else if (state->element == CLUSTER_SPOFS_SPOF &&
	     xmlStrEqual(name, BAD_CAST("node")))
    {
	state->element = CLUSTER_SPOFS_SPOF_NODE;
      	adm_deserialize_spof_node(data, attrs);
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1,
		    "extra node <%s>", name);
	state->failed = true;
    }

    state->element_str = NULL;
}

static void adm_deserialize_assign_implicit_spofs(void *data)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;
    struct adm_node *node;

    if (state->failed)
	return;

    /* When reaching the end of the <spofs> XML node, we'll
     * assign new SPOF ids to nodes which aren't explicitely
     * in SPOF groups
     */
    adm_cluster_for_each_node(node)
    {
        if (adm_node_get_spof_id(node) == SPOF_ID_NONE)
        {
            state->cur_spof_id++;
            adm_node_set_spof_id(node, state->cur_spof_id);
        }
    }
}

static void
adm_deserialize_end_element(void *data, const xmlChar *name)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)data;

    if (state->failed)
	return;

    if (state->element == EXANODES &&
	xmlStrEqual(name, BAD_CAST("Exanodes")))
    {
	state->element = ROOT;
    }
    else if (state->element == CLUSTER &&
	     xmlStrEqual(name, BAD_CAST("cluster")))
    {
	state->element = EXANODES;
        adm_deserialize_assign_implicit_spofs(data);
    }
    else if (state->element == CLUSTER_NODE &&
	     xmlStrEqual(name, BAD_CAST("node")))
    {
	state->current.node = NULL;
	state->element = CLUSTER;
    }
    else if (state->element == CLUSTER &&
	     xmlStrEqual(name, BAD_CAST(EXA_CONF_MONITORING)))
    {
	/* Nothing to do */
    }
    else if (state->element == CLUSTER_NODE_NETWORK &&
	     xmlStrEqual(name, BAD_CAST("network")))
    {
	state->element = CLUSTER_NODE;
    }
    else if (state->element == CLUSTER_NODE_DISK &&
	     xmlStrEqual(name, BAD_CAST("disk")))
    {
	state->element = CLUSTER_NODE;
    }
    else if (state->element == DISKGROUP &&
	     xmlStrEqual(name, BAD_CAST("diskgroup")))
    {
	state->current.group = NULL;
	state->element = EXANODES;
    }
    else if (state->element == DISKGROUP_PHYSICAL &&
	     xmlStrEqual(name, BAD_CAST("physical")))
    {
	state->element = DISKGROUP;
    }
    else if (state->element == DISKGROUP_PHYSICAL_DISK &&
	     xmlStrEqual(name, BAD_CAST("disk")))
    {
	state->element = DISKGROUP_PHYSICAL;
    }
    else if (state->element == DISKGROUP_LOGICAL &&
	     xmlStrEqual(name, BAD_CAST("logical")))
    {
	state->element = DISKGROUP;
    }
    else if (state->element == DISKGROUP_LOGICAL_VOLUME &&
	     xmlStrEqual(name, BAD_CAST("volume")))
    {
	state->current.group = state->current.volume->group;
	state->element = DISKGROUP_LOGICAL;
    }
#ifdef WITH_FS
    else if (state->element == DISKGROUP_LOGICAL_VOLUME_FS &&
	     xmlStrEqual(name, BAD_CAST("fs")))
    {
	state->element = DISKGROUP_LOGICAL_VOLUME;
    }
#endif
    else if (state->element == TUNABLES &&
	     xmlStrEqual(name, BAD_CAST("tunables")))
    {
	state->element = EXANODES;
    }
    else if (state->element == TUNABLES_TUNABLE &&
	     xmlStrEqual(name, BAD_CAST("tunable")))
    {
	state->element = TUNABLES;
    }
    else if (state->element == CLUSTER_SPOFS &&
	     xmlStrEqual(name, BAD_CAST("spof-groups")))
    {
	state->element = CLUSTER;
    }
    else if (state->element == CLUSTER_SPOFS_SPOF &&
	     xmlStrEqual(name, BAD_CAST("spof-group")))
    {
	state->element = CLUSTER_SPOFS;
    }
    else if (state->element == CLUSTER_SPOFS_SPOF_NODE &&
	     xmlStrEqual(name, BAD_CAST("node")))
    {
        state->element = CLUSTER_SPOFS_SPOF;
    }
    else
    {
	os_snprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, "<%d> closed by </%s>", state->element, name);
	state->failed = true;
    }
}


static void adm_deserialize_error(void *_state, const char *msg, ...)
{
    adm_deserialize_state_t *state = (adm_deserialize_state_t *)_state;
    va_list args;

    va_start(args, msg);
    os_vsnprintf(state->error_msg, EXA_MAXSIZE_LINE + 1, msg, args);
    va_end(args);
}


static xmlSAXHandler adm_deserialize_parser =
{
    .startElement = adm_deserialize_start_element,
    .endElement = adm_deserialize_end_element,
    .warning = adm_deserialize_error,
    .error = adm_deserialize_error,
    .fatalError = adm_deserialize_error,
};

static void adm_deserialize_init_state(adm_deserialize_state_t *state)
{
    state->element = ROOT;
    state->current.node = NULL;
    state->current.net = NULL;
    state->current.group = NULL;
    state->current.volume = NULL;
    state->create  = 0;
    state->failed  = 0;
    state->cur_spof_id = SPOF_ID_NONE;
    state->error_msg = NULL;
    state->element_str = NULL;
    state->identifier_str = NULL;
}

int
adm_deserialize_from_file(const char *path, char *error_msg, int create)
{
    adm_deserialize_state_t state;
    int ret;

    exalog_debug("deserialize %s", path);
    EXA_ASSERT(!adm_cluster.created);

    memset(&state, 0xEE, sizeof(state));
    adm_deserialize_init_state(&state);

    state.create = create;
    state.error_msg = error_msg;


    ret = xmlSAXUserParseFile(&adm_deserialize_parser, &state, path);
    if (ret != 0)
    {
	exalog_debug("xmlSAXUserParseFile(): %d", ret);
	if (error_msg[0] == '\0')
	    os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "libxml2 parsing error %d", ret);
	adm_cluster_cleanup();
	return -EXA_ERR_XML_PARSE;
    }
    if (state.failed)
    {
	exalog_error("%s", state.error_msg);
	adm_cluster_cleanup();
	return -ADMIND_ERR_CONFIG_LOAD;
    }
    if (adm_myself() == NULL)
    {
	os_snprintf(state.error_msg, EXA_MAXSIZE_LINE + 1,
		    "my hostname is not part of the cluster");
	exalog_error("%s", state.error_msg);
	adm_cluster_cleanup();
	return -ADMIND_ERR_CONFIG_LOAD;
    }

    adm_cluster.created = true;

    exalog_debug("success");

    return EXA_SUCCESS;
}


int
adm_deserialize_from_memory(const char *buffer, int size,
			    char *error_msg, int create)
{
    adm_deserialize_state_t state;
    int ret;

    exalog_debug("deserialize buffer of %d bytes", size);
    EXA_ASSERT(!adm_cluster.created);

    memset(&state, 0xEE, sizeof(state));
    adm_deserialize_init_state(&state);

    state.create = create;
    state.error_msg = error_msg;

    ret = xmlSAXUserParseMemory(&adm_deserialize_parser, &state, buffer, size);
    if (ret != 0)
    {
	exalog_debug("xmlSAXUserParseMemory(): %d", ret);
	if (error_msg[0] == '\0')
	    os_snprintf(error_msg, EXA_MAXSIZE_LINE + 1, "libxml2 parsing error %d", ret);
	adm_cluster_cleanup();
	return -EXA_ERR_XML_PARSE;
    }
    if (state.failed)
    {
	exalog_error("%s", state.error_msg);
	adm_cluster_cleanup();
	return -ADMIND_ERR_CONFIG_LOAD;
    }
    if (adm_myself() == NULL)
    {
	os_snprintf(state.error_msg, EXA_MAXSIZE_LINE + 1,
		    "my hostname is not part of the cluster");
	exalog_error("%s", state.error_msg);
	adm_cluster_cleanup();
	return -ADMIND_ERR_CONFIG_LOAD;
    }

    adm_cluster.created = true;

    exalog_debug("success");

    return EXA_SUCCESS;
}
