/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file
 *  \brief This is the libconfig API for Exanodes.
 *  This library is based on the libxml2 and is used to parse and
 *  access content of the Exanodes configuration file.
 */

#include "common/include/exa_config.h"

#include <string.h>

#include "os/include/os_inttypes.h"
#include "os/include/os_mem.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"
#include "common/include/uuid.h"

#define xmlString(a) BAD_CAST(a)
#define EXA_CONF_PATHELEM_MAX 127

/*
 * Wrapper functions over os_mem macros, needed to pass them to libxml
 *
 */
#ifdef WITH_MEMTRACE
static void *
_libxml2_malloc(size_t size) {
  return(os_malloc_trace(size, "libxml2-malloc", 0));
}

static char *
_libxml2_strdup(const char * s) {
  return(os_strdup_trace(s, "libxml2-strdup", 0));
}

static void *
_libxml2_realloc(void *ptr, size_t size) {
  return(os_realloc_trace(ptr, size, "libxml2-realloc", 0));
}

static void
_libxml2_free(void *ptr) {
  os_free_trace(ptr, "libxml2-free", 0);
}
#endif

/**
 * \brief Initialize exa_config.
 * Basically, initialize the underlying libxml2  library.
 */
void xml_conf_init (void)
{
  /* Init lib xml */
#ifdef WITH_MEMTRACE
  xmlMemSetup(_libxml2_free, _libxml2_malloc,
	      _libxml2_realloc, _libxml2_strdup);
#endif

  xmlInitParser ();
  LIBXML_TEST_VERSION
}


/** \brief Initialize exa_config from a config file
 *
 * \warning: parsing error are not reported on stdout
 *
 * \param[in] file: a file name
 * \return A libxml2 handle to be passed back to libxml2 or exa_config api
 *         NULL in case of parsing error.
 */
xmlDocPtr xml_conf_init_from_file ( const char * file)
{
  xmlDocPtr doc;

  doc = xmlReadFile(file, NULL, XML_PARSE_NOBLANKS);

  return doc;
}


/** \brief Initialize exa_config_from_buf from a char *buffer
 *
 * \warning: parsing error are not reported
 *
 * \param[in] buf: a text buffer. this buffer can be freed by the caller.
 * \param[in] len: buffer len.
 * \return A libxml2 handle to be passed back to libxml2 or exa_config api.
 *         NULL in case of parsing error.
 */
xmlDocPtr xml_conf_init_from_buf(const char *buf, size_t len)
{
  return xmlReadMemory(buf, len, NULL, NULL,
                       XML_PARSE_NOBLANKS|XML_PARSE_NOERROR|XML_PARSE_NOWARNING);
}

/**
 * xmlNodeListGetString:
 * \param[in] doc:  the document
 * \param[in] list:  a Node list
 * \param[in] inLine:  should we replace entity contents or show their external form
 *
 * Build the string equivalent to the text contained in the Node list
 * made of TEXTs and ENTITY_REFs
 *
 * Returns a pointer to the string copy, the caller must free it with xmlFree().
 */
static char *
xml_xmlNodeListGetString(const xmlNode *list, int inLine)
{
  const xmlNode *node = list;
  xmlChar *ret = NULL;

  if (list == NULL)
    return (NULL);

  while (node != NULL)
    {
      if ((node->type == XML_TEXT_NODE || node->type == XML_CDATA_SECTION_NODE)
	  && inLine)
	ret = node->content;

      node = node->next;
    }
  return (char *)ret;
}


/** Return the value of the specified attribute or NULL if not
 * found. This variant of the xml_dup_prop() function does not
 * allocate memory, it returns a pointer directly inside the tree,
 * which cannot be modified.
 *
 * \param[in] node: a node
 * \param[in] name: name of the attribute to get the value of
 * \return pointer to the attribute's value
 */
const char *
xml_get_prop(const xmlNode *node, const char *name)
{
  xmlAttrPtr prop;

  if (node == NULL || name == NULL)
    return NULL;

  /* Check on the properties attached to the node */
  prop = node->properties;
  while (prop != NULL)
    {
      if (xmlStrEqual(prop->name, xmlString(name)))
	{
	  const char *ret;
	  ret = (const char *) xml_xmlNodeListGetString(prop->children, 1);
	  if (ret == NULL)
	    return "";

	  return ret;
	}
      prop = prop->next;
    }

  return NULL;
}


/**
 * Create a new XML document
 *
 * @param[in] version Version of the document
 *
 * @return A pointer to the XML document
 *
 * @note This function simply wraps the libxml2 API in order to avoid
 * casts between xmlChar* and char*
 */
xmlDocPtr
xml_new_doc(const char *version)
{
  return xmlNewDoc(xmlString(version));
}

/**
 * Create a new node in a XML document
 *
 * @param[in] doc     The document in which the node should be added
 *
 * @param[in] ns      The namespace (can be NULL)
 *
 * @param[in] name    Name of the new node
 *
 * @param[in] content Contents of the new node (can be NULL)
 *
 * @return A pointer to the new node
 *
 * @note This function simply wraps the libxml2 API in order to avoid
 * casts between xmlChar* and char*
 */
xmlNodePtr
xml_new_doc_node(xmlDocPtr doc, xmlNsPtr ns, const char *name,
		 const char *content)
{
  return xmlNewDocNode(doc, ns, xmlString(name), xmlString(content));
}

/**
 * Create a new node as a child of an existing node
 *
 * @param[in] parent  The parent node of the node to create
 *
 * @param[in] ns      The namespace (can be NULL)
 *
 * @param[in] name    Name of the new node
 *
 * @param[in] content Contents of the new node (can be NULL)
 *
 * @return A pointer to the new node
 *
 * @note This function simply wraps the libxml2 API in order to avoid
 * casts between xmlChar* and char*
 */
xmlNodePtr
xml_new_child(xmlNodePtr parent, xmlNsPtr ns, const char *name,
              const char *content)
{
  return xmlNewChild(parent, ns, xmlString(name), xmlString(content));
}

static xmlNodeSetPtr
xml_conf_xpath_query_no_fmt(xmlDoc *tree, const char *xpath)
{
  xmlXPathContextPtr xPCtx;
  xmlXPathObjectPtr result;
  xmlNodeSetPtr nodeSet;

  xPCtx = xmlXPathNewContext(tree);

  if (xPCtx == NULL)
    return NULL;

  result = xmlXPathEvalExpression((const xmlChar *)xpath, xPCtx);

  xmlXPathFreeContext (xPCtx);

  if (!result)
    return NULL;

  if (result->type != XPATH_NODESET)
    {
      xmlXPathFreeObject(result);
      return NULL;
    }

  nodeSet = result->nodesetval;

  /* NULL it now to avoid xmlXPathFreeContext to free it */
  result->nodesetval = NULL;

  if (nodeSet == NULL)
    {
      xmlXPathFreeObject(result);
      return NULL;
    }

  if (xmlXPathNodeSetIsEmpty(nodeSet))
    {
      xmlXPathFreeObject(result);
      xmlXPathFreeNodeSet(nodeSet);
      return NULL;
    }

  xmlXPathFreeObject(result);

  return nodeSet;
}




bool xml_conf_xpath_predicate(const xmlDocPtr tree, const char *xpath)
{
  xmlXPathContextPtr xPCtx;
  xmlXPathObjectPtr result;
  int res;

  xPCtx = xmlXPathNewContext(tree);
  if (xPCtx == NULL)
      return false;

  result = xmlXPathEvalExpression((const xmlChar *)xpath, xPCtx);

  if (!result)
      return false;

  res = xmlXPathEvalPredicate (xPCtx, result);

  xmlXPathFreeObject(result);
  xmlXPathFreeContext (xPCtx);

  return (bool) res;
}


/**\brief Performs an XPath query on the given DOM tree, and returns
 * the set of nodes that matches the query.
 *
 * \param[in] tree The DOM tree containing the configuration
 *
 * \param[in] fmt Format string for the XPath query
 *
 * \param[in] ... Arguments for the XPath query
 *
 * \result An set of XML nodes. The number of nodes can be found using
 *         xml_conf_xpath_entity_count(). The n-th node can be get using
 *         xml_conf_xpath_result_entity_count(). And the set must be freed
 *         using xml_conf_xpath_free() after use.
 */
xmlNodeSetPtr
xml_conf_xpath_query(const xmlDocPtr tree, const char *fmt, ...)
{
  char xpath[EXA_CONF_PATHELEM_MAX+1];
  va_list ap;
  int ret;

  va_start(ap, fmt);
  ret = os_vsnprintf(xpath, sizeof(xpath), fmt, ap);
  va_end(ap);

  if (ret < 0)
    return NULL;

  return xml_conf_xpath_query_no_fmt(tree, xpath);
}


/** \brief Get the number of xml entities in the given xmlNodeSetPtr
 *
 * \param[in] nodeSet A libxml2 xmlNodeSetPtr as returned by xml_conf_xpath_query()
 *
 * \return The number of xml entities in the nodeSet
 */
int xml_conf_xpath_result_entity_count(xmlNodeSetPtr nodeSet)
{
  return xmlXPathNodeSetGetLength(nodeSet);
}


/** \brief Return the Nth entity from the given libxml2 xmlNodePtr
 *
 * \param[in] nodeSet A libxml2 xmlNodeSetPtr as returned by
 *                    xml_conf_xpath_query()
 *
 * \param[in] nth    Index of the element to return

 * \return a libxml2 xmlNodePtr. Use xmlGetProp() or xml_get_prop() to
 * access the properties of this xmlnode.
 */
xmlNodePtr xml_conf_xpath_result_entity_get(xmlNodeSetPtr nodeSet, int nth)
{
  return xmlXPathNodeSetItem(nodeSet, nth);
}


/**\brief Free the result of an XPath query
 *
 * \param[in] nodeSet Result of an XPath query
 */
void
__xml_conf_xpath_free(xmlNodeSetPtr nodeSet)
{
  xmlXPathFreeNodeSet(nodeSet);
}


/**\brief Performs an XPath query on the given DOM tree, and returns
 * the node that matches the query. If 0 or more than 1 node match the
 * query, NULL is returned.
 *
 * \param[in] tree The DOM tree containing the configuration
 *
 * \param[in] fmt Format string for the XPath query
 *
 * \param[in] ... Arguments for the XPath query
 *
 * \result An XML node if exactly one node matches the query, NULL
 * otherwise.
 */
xmlNodePtr
xml_conf_xpath_singleton(xmlDoc *tree, const char *fmt, ...)
{
  char xpath[EXA_CONF_PATHELEM_MAX+1];
  va_list ap;
  int ret;
  xmlNodeSetPtr nodeset;
  xmlNodePtr node;

  va_start(ap, fmt);
  ret = os_vsnprintf (xpath, sizeof(xpath), fmt, ap);
  va_end(ap);

  if (ret < 0)
    return NULL;

  nodeset = xml_conf_xpath_query_no_fmt(tree, xpath);
  if (xml_conf_xpath_result_entity_count(nodeset) != 1)
    return NULL;

  node = xml_conf_xpath_result_entity_get(nodeset, 0);
  xml_conf_xpath_free(nodeset);

  return node;
}


/**
 * Set a property of a XML node
 *
 * @param[in] node The XML node
 *
 * @param[in] name Name of the property to set
 *
 * @param[in] value Value of the property
 *
 * @return EXA_SUCCESS or an error code
 *
 * @note This function simply wraps the libxml2 API in order to avoid
 * casts between xmlChar* and char*
 */
int xml_set_prop(xmlNodePtr xml_node, const char *name, const char *value)
{
  xmlAttrPtr attr;

  EXA_ASSERT(value);

  attr = xmlSetProp(xml_node, BAD_CAST(name), BAD_CAST(value));

  if (attr == NULL)
    return -EXA_ERR_XML_ADD;

  return EXA_SUCCESS;
}


int xml_set_prop_bool(xmlNodePtr xml_node, const char *name, int value)
{
  return xml_set_prop(xml_node, name, value ? ADMIND_PROP_TRUE : ADMIND_PROP_FALSE);
}


int xml_set_prop_ok(xmlNodePtr xml_node, const char *name, int value)
{
  return xml_set_prop(xml_node, name, value ? ADMIND_PROP_OK : ADMIND_PROP_NOK);
}


int xml_set_prop_u64(xmlNodePtr xml_node, const char *name, uint64_t value)
{
  char str[MAXLEN_UINT64 + 1];

  os_snprintf(str, sizeof(str), "%" PRIu64, value);

  return xml_set_prop(xml_node, name, str);
}


int xml_set_prop_uuid(xmlNodePtr xml_node, const char *name, const exa_uuid_t *value)
{
  char str[UUID_STR_LEN + 1];

  uuid2str(value, str);

  return xml_set_prop(xml_node, name, str);
}


xmlNodePtr xml_get_child(xmlNodePtr parent, const char *node_name,
		     const char *prop_name, const char *prop_value)
{
    xmlNodePtr cur;

    for (cur = parent->xmlChildrenNode; cur != NULL; cur = cur->next)
    {
        xmlChar *value = xmlGetProp(cur, (const xmlChar *)prop_name);
        int got_it = !xmlStrcmp(cur->name, (const xmlChar *)node_name)
                     && !xmlStrcmp(value, (const xmlChar *)prop_value);
        xmlFree(value);
        if (got_it)
            return cur;
    }

  return NULL;
}
