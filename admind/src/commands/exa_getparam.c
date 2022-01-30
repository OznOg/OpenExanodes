/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "admind/src/adm_cluster.h"
#include "admind/src/adm_node.h"
#include "admind/src/adm_nodeset.h"
#include "admind/src/adm_serialize.h"
#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"
#include "admind/src/rpc.h"
#include "admind/src/service_parameter.h"
#include "admind/src/commands/command_api.h"

#include "common/include/exa_config.h"
#include "common/include/exa_error.h"
#include "common/include/exa_names.h"
#include "os/include/os_file.h"
#include "os/include/strlcpy.h"
#include "os/include/os_stdio.h"
#include "log/include/log.h"


__export(EXA_ADM_GETPARAM) __no_param;

static int
build_response(xmlNodePtr exanodes_node)
{
  exa_service_parameter_t *current_parameter;
  int index_parameter = 0;
  char param_info[EXA_MAXSIZE_LINE + 1];

  while ((current_parameter =
	 exa_service_parameter_get_list(&index_parameter)) != NULL)
    {
      xmlNodePtr exanodes_param;
      const char* default_value;
      const char* current_value;
      default_value = adm_cluster_get_param_default(current_parameter->name);
      current_value = adm_cluster_get_param_text(current_parameter->name);
      exanodes_param = xmlNewChild(exanodes_node, NULL, BAD_CAST("param"), NULL);
      if (exanodes_param == NULL)
	return -EXA_ERR_XML_INIT;

      if (xml_set_prop(exanodes_param, EXA_PARAM_NAME, current_parameter->name)
	  != EXA_SUCCESS)
	return -EXA_ERR_XML_INIT;

      /* Set the default */
      if (xml_set_prop(exanodes_param, EXA_PARAM_DEFAULT, default_value)
	  != EXA_SUCCESS)
	return -EXA_ERR_XML_INIT;

      if (xml_set_prop(exanodes_param, EXA_PARAM_DESCRIPTION,
		   current_parameter->description) != EXA_SUCCESS)
	return -EXA_ERR_XML_INIT;

      if (xml_set_prop(exanodes_param, EXA_PARAM_VALUE, current_value) != EXA_SUCCESS)
        return -EXA_ERR_XML_INIT;

      /* Set the current value and specific constraints for this param */
      switch (current_parameter->type)
      {
        case EXA_PARAM_TYPE_INT:
        {
          if(current_parameter->max != -1)
            os_snprintf(param_info, sizeof(param_info),
                "It must be between %d and %d. The default is %s.",
                current_parameter->min, current_parameter->max, default_value);
          else
            os_snprintf(param_info, sizeof(param_info),
                "It must be %d or above. The default is %s.",
                current_parameter->min, default_value);
          break;
        }
        case EXA_PARAM_TYPE_LIST:
        {
            int i = 0;
            char str[EXA_MAXSIZE_LINE + 1];

            str[0] = 0;
            while(current_parameter->choices[i++])
            {
                char str2[EXA_MAXSIZE_LINE + 1];

                os_snprintf(str2, EXA_MAXSIZE_LINE, "%s ",
                         current_parameter->choices[i-1]);
                os_snprintf(str + strlen(str), EXA_MAXSIZE_LINE - strlen(str), "%s", str2);
            }

	    if (xml_set_prop(exanodes_param, EXA_PARAM_CHOICES, str) != EXA_SUCCESS)
                return -EXA_ERR_XML_INIT;

            if (str[0] == '\0')
                os_snprintf(param_info, sizeof(param_info),
                    "It must be a character string.");
            else
                os_snprintf(param_info, sizeof(param_info),
                    "It must be set to one of these values (the default is '%s'):%s",
                    default_value, str);
            break;
        }
        case EXA_PARAM_TYPE_BOOLEAN:
        {
            os_snprintf(param_info, sizeof(param_info),
                "It must be true or false. The default is %s.",
                default_value);
            break;
        }
        case EXA_PARAM_TYPE_NODELIST:
        {
            os_snprintf(param_info, sizeof(param_info), "It must be a list of nodes.");
            break;
        }
        case EXA_PARAM_TYPE_TEXT:
        {
	    os_snprintf(param_info, sizeof(param_info), "It must be a string. The default is '%s'.",
		     default_value);
            break;
        }
        case EXA_PARAM_TYPE_IPADDRESS:
        {
            os_snprintf(param_info, sizeof(param_info), "It must be an IP address. The default is '%s'.",
		     default_value);
            break;
        }
        default:
            break;
        }

        /* Dump information related to the parameter type into the XML doc */
        if (xml_set_prop(exanodes_param, EXA_PARAM_TYPE_INFO, param_info) != EXA_SUCCESS)
            return -EXA_ERR_XML_INIT;
    }

  return EXA_SUCCESS;
}


/*------------------------------------------------------------------------------*/
/** \brief Implements the get_param cluster command
 *
 * Recreate a list of parameters.
 *
 * \param [in] thr_nb: thread id.
 */
/*------------------------------------------------------------------------------*/
static void
cluster_getparam(int thr_nb, void *data, cl_error_desc_t *err_desc)
{
  xmlDocPtr doc = NULL;
  xmlNodePtr exanodes_node;
  int ret;

  exalog_debug("getparam");

  /* Create XML document */

  doc = xmlNewDoc(BAD_CAST("1.0"));
  if (doc == NULL)
  {
    set_error(err_desc, -EXA_ERR_XML_INIT, "Failed to create result document");
    return;
  }

  exanodes_node = xmlNewNode(NULL, BAD_CAST("Exanodes"));
  if (exanodes_node == NULL)
  {
    set_error(err_desc, -EXA_ERR_XML_INIT,
	      "Failed to create node in result document");
    xmlFreeDoc(doc);
    return;
  }

  xmlDocSetRootElement(doc, exanodes_node);


  ret = build_response(exanodes_node);

  if (ret ==  EXA_SUCCESS)
    {
      xmlChar *xmlchar_doc;
      int buf_size;

      xmlDocDumpFormatMemory(doc, &xmlchar_doc, &buf_size, 1);

      send_payload_str((char *)xmlchar_doc);

      xmlFree(xmlchar_doc);
    }

  xmlFreeDoc(doc);

  set_error(err_desc, ret, NULL);
}

/**
 * Definition of the getparam command.
 */
const AdmCommand exa_cmd_get_param = {
  .code            = EXA_ADM_GETPARAM,
  .msg             = "getparam",
  .accepted_status = ~ADMIND_NOCONFIG,
  .match_cl_uuid   = true,
  .cluster_command = cluster_getparam,
  .local_commands  = {
   { RPC_COMMAND_NULL, NULL }
  }
};

