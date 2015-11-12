/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/**
 * @file
 *
 * XML command reception subsystem and working thread implementation.
 *
 * This file contains :
 *
 *  - the subsystem responsible for the reception of XML commands
 *  coming from the user interfaces (either text or graphical) and for
 *  the allocation of a working thread dedicated to the execution of
 *  the command. This subsystem is not responsible for sending the
 *  result of the command back to the user interface: it is the job of
 *  the working thread itself.
 *
 *  - the implementation of the working threads main function. The
 *  different functions executed by the working threads are
 *  implemented in a separate file, however.
 *
 * HOW IT WORKS:
 *
 * We accept MAX_XML_CONNECTION connection from the CLI/GUI on our
 * listening port.  Once a command is send to us, we read it and check
 * it's validity.  If valid, a working thread is selected to process
 * it. If not free, we return -EXA_ERR_ADM_BUSY.
 *
 * The working thread selection depends on the command received:
 *
 * - The working thread 1 is always used to process CLINFO commands
 * (exa_clinfo).
 *
 * - The working thread 2 is always used to process any other command.
 *
 * It means that a 2nd GETCONFIG while the WT1 is in use will return
 * an BUSY, even is the WT2 is free. In this case, the caller is not
 * disconnected, it can reissue it's command.
 *
 * When the command is processed, the answer is sent and the working
 * thread is freed.  Another command can then be sent on the same
 * connection.
 *
 */

#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "admind/src/xml_proto/xml_proto.h"
#include "admind/src/xml_proto/xml_proto_api.h"
#include "admind/src/xml_proto/xml_protocol_version.h"
#include "os/include/os_mem.h"
#include "log/include/log.h"

#include "os/include/os_stdio.h"


/**
 * Find a XML command parser using the command name
 *
 * @param[in] name  Name of the command
 *
 * @return Pointer to the xml_parser_t structure if the command exists,
 *         NULL otherwise
 */
static const xml_parser_t *
parser_find_by_name(const char *name)
{
  xml_parser_t *elt;

  for (elt = xml_parser_list; elt->cmd_code != EXA_ADM_INVALID; elt++)
  {
    if (strcmp(elt->cmd_name, name) == 0)
      return elt;
  }

  return NULL;
}

/** \brief
 * Check the content of the given doc pointer is a command with the proper params
 *
 * \param[in] XML_cmd       The command doc pointer to analyse
 * \param[in] parser        Descriptor of the parser for this very command
 * \param[in:out] err_desc  Error descriptor filled with description when returning
 * \param[out] data         see xml_command_parse for description
 * \param[out] data_size    see xml_command_parse for description
 */
static void
command_parse(xmlDocPtr XML_cmd, const xml_parser_t *parser,
              void **data, size_t *data_size, cl_error_desc_t *err_desc)
{
  xmlNodeSetPtr node_set_ptr;

  EXA_ASSERT(XML_cmd && parser && data && data_size);

  if (parser->parse == NULL)
  {
    /* if there is no parsing function set, the command cannot expect any payload */
    EXA_ASSERT(parser->parsed_params_size == 0);
    *data      = NULL;
    *data_size = 0;
    set_success(err_desc);
    return;
  }

  /* allocate memory to store de data that are about to be extracted from the
   * xml chunk we are working on. */
  *data_size = parser->parsed_params_size;
  *data      = os_malloc(*data_size);
  if (!*data)
    {
      set_error(err_desc, -ENOMEM, "Unable to allocate memory");
      return;
    }

#ifndef WITH_MEMTRACE
  memset(*data, 0, *data_size);
#endif

  parser->parse(XML_cmd, *data, err_desc);

  exalog_debug("parsing command data: ret=%d msg=%s",
               err_desc->code, err_desc->msg);

  if (err_desc->code)
  {
      os_free(*data);
      return;
  }

  /* When parser has finished, there must not remain any parameter as they are
   * removed from the tree once they are correctly parsed */
  node_set_ptr = xml_conf_xpath_query(XML_cmd, XPATH_TO_GET_PARAM);

  if (xml_conf_xpath_result_entity_count(node_set_ptr) != 0)
  {
      /* Error is given only for the first unintended param... no need to
       * bother iterating though the tree, after all, this case is not
       * supposed to happen in everyday use */
      set_error(err_desc, -EXA_ERR_INVALID_PARAM,
	        "Found extra parameter '%s' value '%s' in param list",
		 xml_get_prop(node_set_ptr->nodeTab[0], "name"),
		 xml_get_prop(node_set_ptr->nodeTab[0], "value"));
      os_free(*data);
  }

  xml_conf_xpath_free(node_set_ptr);

  return;
}

/**
 * \brief get a xml tree describing a command and parse it.
 * This is a helper function for xml_command_parse, it take the same input but
 * __xml_command_parse takes a valid xmlDocPtr as param. Function is successful
 * if err_desc.code == EXA_SUCCESS
 *
 * \param[in]  doc_cmd_work  The xmldoc describing a command
 * \param[out] cmd_code      The adm_command_code corresponding to the command
 *                           received.
 * \param[out] cluster_uuid  Uuid of the cluster the CLI is trying to work with.
 * \param[out] data          The payload parsed from the xml chunk. The buffer is
 *                           allocated with os_malloc. In case this function is
 *                           successful, the caller has the ownership on the newly
 *                           allocated buffer and is responsible for freeing it
 *                           when not useful anymore.
 * \param[out] data_size     Size of the amount of data extracted from xml
 * \param[out] err_desc      Description of the error encountered, if any.
 */
static void
__xml_command_parse(xmlDocPtr doc_cmd_work, adm_command_code_t *cmd_code,
                    exa_uuid_t *cluster_uuid, void **data, size_t *data_size,
		    cl_error_desc_t *err_desc)
{
  xmlNodePtr cmd_ptr, cl_nptr;
  const char *cmd_proto_str, *cmd_name;
  const xml_parser_t *parser;

  EXA_ASSERT(cmd_code);
  EXA_ASSERT(data);

  /* Extract the command */
  cmd_ptr = xml_conf_xpath_singleton(doc_cmd_work, "/Admind/command");
  if (!cmd_ptr)
  {
    set_error(err_desc, -EXA_ERR_CMD_PARSING, "Invalid command format.");
    return;
  }

  /* Check the protocol_release is provided */
  cmd_proto_str = xml_get_prop(cmd_ptr->parent, "protocol_release");

  if (!cmd_proto_str)
  {
    set_error(err_desc, -EXA_ERR_CMD_PARSING,
	      "Failed to parse protocol release.");
    return;
  }

  /* Check the protocol_release matches our calculated one */
  if (strcmp(XML_PROTOCOL_VERSION, cmd_proto_str))
    {
      set_error(err_desc, -EXA_ERR_BAD_PROTOCOL,
	        "Command has a protocol release of '%s' ; expected '%s'.",
		cmd_proto_str, XML_PROTOCOL_VERSION);
      return;
    }

  /* Check the command now */
  cmd_name = xml_get_prop(cmd_ptr, "name");
  if (!cmd_name)
  {
    set_error(err_desc, -EXA_ERR_CMD_PARSING,
	      "Failed to extract the command name.");
    return;
  }

  parser = parser_find_by_name(cmd_name);
  if (!parser)
  {
    set_error(err_desc, -EXA_ERR_CMD_PARSING,
	      "No command correspond to name '%s'.", cmd_name);
    return;
  }

  exalog_debug("Command %s", parser->cmd_name);

  uuid_zero(cluster_uuid);
  cl_nptr = xml_conf_xpath_singleton(doc_cmd_work, "/Admind/cluster");
    /* Careful, the cluster tag is "optional" for some commands
     * so we do not report error if it is not there
     * FIXME is it really a correct way to do ? */
  if (cl_nptr)
  {
    const char *cl_uuid_str = xml_get_prop(cl_nptr, "uuid");
    /* if the tag cluster is present, the cluster uuid is mandatory */
    if (!cl_uuid_str)
    {
      set_error(err_desc, -EXA_ERR_CMD_PARSING, "No cluster uuid provided.");
      return;
    }

    if (uuid_scan(cl_uuid_str, cluster_uuid))
    {
      set_error(err_desc, -EXA_ERR_UUID, NULL);
      return;
    }
  }

  /* Command meta informations were correctly extracted, now parse the command
   * specific fields */
  command_parse(doc_cmd_work, parser, data, data_size, err_desc);

  *cmd_code = parser->cmd_code;

  return;
}

/**
 * \brief get an xml command and parse it.
 *
 * \param[in]  buffer    the buffer containing command
 * \param[out] cmd_code  the command code corresponding to the command received.
 * \param[out] cluster_uuid uuid of the cluster the CLI is trying to work with.
 * \param[out] data      the payload parsed from the xml chunk. This is a buffer
 *             allocated with os_malloc so the caller must free it once it
 *             doesn't need it anymore.
 *             In case this function returns an error no malloc was done (no
 *             call to free is needed).
 * \param[out] data_size    size of the amount of data extracted from xml
 * \param[out] err_desc   Description of the error encountered, if any.
 */
void
xml_command_parse(const char *buffer, adm_command_code_t *cmd_code,
                  exa_uuid_t *cluster_uuid, void **data, size_t *data_size,
		  cl_error_desc_t *err_desc)
{
  xmlDocPtr doc_cmd_work;

  /* Try to get a xml tree from the buffer */
  doc_cmd_work = xml_conf_init_from_buf(buffer, strlen(buffer));
  if (!doc_cmd_work)
    {
      set_error(err_desc, -EXA_ERR_CMD_PARSING, "Invalid XML");
      return;
    }

  __xml_command_parse(doc_cmd_work, cmd_code, cluster_uuid,
                      data, data_size, err_desc);

  xmlFreeDoc(doc_cmd_work);

  return;
}

/**
 * tells if a buffer contains enougth data to be parsed.
 * If the xml chunk is considered as partial, the fucntion returns true.
 * Everytime the incomming changes, this function sould be called.
 * The buffer param MUST be a valid string ie terminating by '\0'
 *
 * @param[in] buffer   the buffer to check
 *
 * return true or false
 */
bool
xml_buffer_is_partial(const char *buffer)
{
  return strstr(buffer, "</Admind>") ? false : true;
}

/**
 * \brief xml_inprogress
 * format a xml in progress buffer. This must not allocate.
 *
 * @param[in:out] buf      buffer for writing the in_progress message.
 * @param[in]  buf_size    size of the buffer the caller gives.
 * @param[in]  src_node_name The node which created this in progress
 * @param[in]  description a string describing the current step
 * @param[in]  err_desc    the error descriptor linked to the description
 *
 * @return     void
 */
void
xml_inprogress(char *buf, size_t buf_size, const char *src_node_name,
               const char *description, const cl_error_desc_t *err_desc)
{
  int ret;

  ret = os_snprintf(buf, buf_size,
		    "<?xml version=\"1.0\"?>\n"
		    "<Admind protocol_release=\"" XML_PROTOCOL_VERSION "\">\n"
		    "<result type=\"%s\">\n"
		    "<error description=\"%s\" code=\"%d\" message=\"%s\" node=\"%s\"/>\n"
		    "</result>\n"
		    "</Admind>\n",
		    "IN PROGRESS",
		    description,
		    err_desc->code,
		    err_desc->msg,
		    src_node_name);

  /* FIXME What can we do in case of error ? */
  EXA_ASSERT(ret < buf_size);
}

/** \brief
 * Return a progressive payload buffer formated for the XML protocol.
 *
 * \param str  string that will be added to the result
 *
 * returns a pointer on a newly allocated string. Caller must perform os_free
 * once he finished using it.
 */
char *
xml_payload_str_new(const char *str)
{
    xmlDoc *doc = xml_new_doc("1.0");
    xmlNode *node;
    xmlChar *dump;
    int dump_size;
    char *result;

    doc->children = xml_new_doc_node(doc, NULL, "Admind", NULL);
    xml_set_prop(doc->children, "protocol_release", XML_PROTOCOL_VERSION);

    node = xml_new_child(doc->children, NULL, "result", NULL);
    xml_set_prop(node, "type", "PAYLOAD");

    node = xml_new_child(node, NULL, "string", NULL);
    xml_set_prop(node, "value", str);

    xmlDocDumpMemory(doc, &dump, &dump_size);

    result = os_malloc(dump_size + 1);
    strlcpy(result, (char *)dump, dump_size);
    xmlFree(dump);
    xmlFreeDoc(doc);

    return result;
}

/** \brief
 * Return a buffer formated for the XML protocol.
 *
 * \param err_desc description of the error to return
 *
 * returns a pointer on a newly allocated string. Caller must perform os_free
 * once he finished using it.
 */
char *
xml_command_end(const cl_error_desc_t *err_desc)
{
    xmlDoc *doc = xml_new_doc("1.0");
    xmlNode *node;
    xmlChar *dump;
    int dump_size;
    char *result;
    char err_code_buf[32];

    doc->children = xml_new_doc_node(doc, NULL, "Admind", NULL);
    xml_set_prop(doc->children, "protocol_release", XML_PROTOCOL_VERSION);

    node = xml_new_child(doc->children, NULL, "result", NULL);
    xml_set_prop(node, "type", "DONE");

    node = xml_new_child(node, NULL, "error", NULL);
    os_snprintf(err_code_buf, sizeof(err_code_buf), "%" PRId32, err_desc->code);
    xml_set_prop(node, "code", err_code_buf);
    xml_set_prop(node, "message", err_desc->msg);

    xmlDocDumpMemory(doc, &dump, &dump_size);

    result = os_malloc(dump_size + 1);
    strlcpy(result, (char *)dump, dump_size);
    xmlFree(dump);
    xmlFreeDoc(doc);

    return result;
}

