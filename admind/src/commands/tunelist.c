/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/commands/tunelist.h"
#include "common/include/exa_error.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_assert.h"
#include "os/include/os_mem.h"
#include "os/include/strlcpy.h"

#include <string.h>

/**
 * @file
 *
 * An interface for all "tune" commands (exa_fstune, exa_vltune, exa_cltune),
 * to create an XML response document to a command of type '-l', with a list of
 * n-uplet of type name/value/description.
 * To use this, it just needs to declare an opaque "handler".
 *
 * Create with tunelist_create_result,
 * Add values with tunelist_add_name_value
 * When the process is aborted, call tunelist_abort.
 * If the process is successful, call tunelist_send_result.
 *
 * It does all XML tree creation/deletion/processing without the user's
 * need to know it's an XML tree.
 */

struct tune_t
{
    char name[EXA_MAXSIZE_TUNE_NAME];
    char default_value[EXA_MAXSIZE_TUNE_VALUE];
    char description[EXA_MAXSIZE_TUNE_DESCRIPTION];
    unsigned int nb_values;
    tune_value_t *value;
};

struct tunelist
{
    xmlDocPtr doc;	    /**< pointer to the XML structure */
    xmlChar *result_buffer; /**< buffer cotaining the resulting string*/
};

tune_t* tune_create(unsigned int nb_values)
{
    tune_t* tune = os_malloc(sizeof(tune_t));
    if (tune == NULL)
	return NULL;

    memset(tune, 0, sizeof(tune_t));
    tune->nb_values = nb_values;

    /* allocate the char* array */
    tune->value = os_malloc(nb_values*sizeof(tune_value_t));
    if (tune->value == NULL)
    {
	os_free(tune);
	return NULL;
    }

    memset(tune->value, 0, nb_values*sizeof(tune_value_t));

    return tune;
}

void tune_delete(tune_t* tune)
{
    EXA_ASSERT(tune != NULL);

    os_free(tune->value);
    os_free(tune);
}

void tune_set_name(tune_t *tune, const char *name)
{
    EXA_ASSERT(tune != NULL);

    strlcpy(tune->name, name, sizeof(tune->name));
}

const char *tune_get_name(const tune_t *tune)
{
    EXA_ASSERT(tune != NULL);
    return tune->name;
}

void tune_set_description(tune_t *tune, const char *descr)
{
    EXA_ASSERT(tune != NULL);

    strlcpy(tune->description, descr, sizeof(tune->description));
}

void tune_set_nth_value(tune_t *tune, unsigned int n, const char *fmt, ...)
{
    va_list al;
    EXA_ASSERT(tune != NULL);
    EXA_ASSERT(n < tune->nb_values);

    va_start(al, fmt);
    os_vsnprintf(tune->value[n], sizeof(tune->value[n]), fmt, al);
    va_end(al);
}

void tune_set_default_value(tune_t *tune, const char *fmt, ...)
{
    va_list al;
    EXA_ASSERT(tune != NULL);

    va_start(al, fmt);
    os_vsnprintf(tune->default_value, sizeof(tune->default_value), fmt, al);
    va_end(al);
}


int tunelist_create(tunelist_t** tunelist)
{
    xmlNodePtr exanodes_node;
    tunelist_t* tunelist_ptr;

    *tunelist = os_malloc(sizeof(struct tunelist));
    if (*tunelist == NULL)
	return -EXA_ERR_XML_INIT;
    tunelist_ptr = *tunelist;

    tunelist_ptr->result_buffer = NULL;

    /* Create XML document */
    tunelist_ptr->doc = xmlNewDoc(BAD_CAST("1.0"));
    if (tunelist_ptr->doc == NULL)
	goto error_and_free_tunelist;

    /* Create XML root document */
    exanodes_node = xmlNewNode(NULL, BAD_CAST("Exanodes"));
    if (exanodes_node == NULL)
	goto error_and_free_doc;

    xmlDocSetRootElement(tunelist_ptr->doc, exanodes_node);

    return EXA_SUCCESS;

error_and_free_doc:
    xmlFreeDoc(tunelist_ptr->doc);
error_and_free_tunelist:
    os_free(tunelist_ptr);
    *tunelist = NULL;
    return -EXA_ERR_XML_INIT;
}

int tunelist_add_tune(tunelist_t* tunelist,
		      tune_t* tune)
{
    xmlAttrPtr xmlProp;
    xmlNodePtr exanodes_param;
    xmlNodePtr root;

    EXA_ASSERT(tunelist);
    EXA_ASSERT(tunelist->doc);
    EXA_ASSERT(tunelist->doc->children);

    root = tunelist->doc->children;
    exanodes_param = xmlNewChild(root, NULL, BAD_CAST(EXA_PARAM), NULL);
    if (exanodes_param == NULL)
	goto error;

    xmlProp = xmlSetProp(exanodes_param,
			 BAD_CAST(EXA_PARAM_NAME),
			 BAD_CAST(tune->name));
    if (xmlProp == NULL)
	goto error_and_free_node;


    xmlProp = xmlSetProp(exanodes_param,
			 BAD_CAST(EXA_PARAM_DESCRIPTION),
			 BAD_CAST(tune->description));
    if (xmlProp == NULL)
	goto error_and_free_node;


    xmlProp = xmlSetProp(exanodes_param,
			 BAD_CAST(EXA_PARAM_DEFAULT),
			 BAD_CAST(tune->default_value));
    if (xmlProp == NULL)
	goto error_and_free_node;

    if (tune->nb_values == 1)
    {
	xmlProp = xmlSetProp(exanodes_param,
			     BAD_CAST(EXA_PARAM_VALUE),
			     BAD_CAST(tune->value[0]));
	if (xmlProp == NULL)
	    goto error_and_free_node;
    }
    else if (tune->nb_values > 1)
    {
	int i;

	for (i = 0; i < tune->nb_values; i++)
	{
	    xmlNodePtr value_xml = xmlNewChild(exanodes_param, NULL,
					       BAD_CAST(EXA_PARAM_VALUE_ITEM), NULL);
	    if (value_xml == NULL)
		goto error_and_free_node;

	    xmlProp = xmlSetProp(value_xml,
				 BAD_CAST(EXA_PARAM_VALUE),
				 BAD_CAST(tune->value[i]));
	    if (xmlProp == NULL)
		goto error_and_free_node;
	}
    }

    return EXA_SUCCESS;

error_and_free_node:
    xmlUnlinkNode(exanodes_param);
    xmlFreeNode(exanodes_param);

error:
    return -EXA_ERR_XML_INIT;
}

void tunelist_delete(tunelist_t* tunelist)
{
    EXA_ASSERT(tunelist);
    EXA_ASSERT(tunelist->doc);

    if (tunelist->result_buffer != NULL)
	xmlFree(tunelist->result_buffer);

    xmlFreeDoc(tunelist->doc);
    os_free(tunelist);
}

const char *tunelist_get_result(tunelist_t* tunelist)
{
    int size;

    EXA_ASSERT(tunelist);
    EXA_ASSERT(tunelist->doc);

    if (tunelist->result_buffer != NULL)
	xmlFree(tunelist->result_buffer);

    xmlDocDumpFormatMemory(tunelist->doc, &tunelist->result_buffer, &size, 1);

    return (const char *)tunelist->result_buffer;
}

