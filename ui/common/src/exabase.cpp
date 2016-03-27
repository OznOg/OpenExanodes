/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include "ui/common/include/exabase.h"

#include <errno.h>
#include <fstream>
#include <set>
#include <map>
#include <stdexcept>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "ui/common/include/cli_log.h"
#include "ui/common/include/config_check.h"
#include "ui/common/include/clustercache.h"
#include "ui/common/include/common_utils.h"

#include "os/include/os_dir.h"
#include "os/include/os_time.h"
#include "os/include/os_stdio.h"
#include "os/include/os_user.h"

using std::shared_ptr;
using std::exception;
using std::set;
using std::map;
using std::string;
using std::runtime_error;

/** Directory for normal users (relative to home directory). */
#ifdef WIN32
#define UI_LOCAL_LOG_DIR    "exanodes\\log"
#else
#define UI_LOCAL_LOG_DIR    ".exanodes/log"
#endif

/** Prefix of the log file */
#define UI_LOG_PREFIX       "exa_cli-"

/** Extension of the log file */
#define UI_LOG_EXT          ".log"


xmlDocPtr Exabase::parse_config_file(const std::string &config_file)
{
  struct stat statbuf;

  if (stat(config_file.c_str(), &statbuf) ||
      (!S_ISREG(statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) )
  {
    exa_cli_error("Configuration file '%s' is not found\n",
		  config_file.c_str());
    return NULL;
  }

  xmlDocPtr configDocPtr(xml_conf_init_from_file(config_file.c_str()));

  if (!configDocPtr)
    return NULL;

  ConfigCheck::normalize_param(configDocPtr);

  xmlNodePtr node_ptr;
  exa_uuid_str_t cluster_uuid_str;
  exa_uuid_t     cluster_uuid;

  /* Assign an UUID to this cluster */
  node_ptr = xml_conf_xpath_singleton(configDocPtr,
				      "/Exanodes/cluster[@name]");

  if (!node_ptr)
  {
    xmlFreeDoc(configDocPtr);
    return NULL;
  }

  /* Create the UUID */
  uuid_generate(&cluster_uuid);
  uuid2str(&cluster_uuid, cluster_uuid_str);

  xml_set_prop(node_ptr, EXA_CONF_CLUSTER_UUID, cluster_uuid_str);

  return configDocPtr;
}


Exabase::Exabase()
{
  xml_conf_init();
}


Exabase::~Exabase()
{
  xmlCleanupParser();
}

int Exabase::set_cluster_from_cache(const std::string &cluster_name,
				    string &error_msg)
{
  cluster.reset();

  try
  {
      cluster = shared_ptr<ClusterCache>(new ClusterCache(cluster_name));
  }
  catch (exception &ex)
  {
    error_msg = ex.what();
  }

  return cluster ? EXA_SUCCESS : EXA_ERR_DEFAULT;
}


int Exabase::set_cluster_from_config(const std::string &cluster_name, xmlDocPtr xml_cfg,
                                     const string &license, string &error_msg)
{
  xmlNodePtr top(xml_conf_xpath_singleton(xml_cfg,
					  "/Exanodes/cluster[@name]"));

  if (!top)
  {
    error_msg = "No cluster found in the config file";
    return EXA_ERR_DEFAULT;
  }

  shared_ptr<xmlChar> xml_cluster_name(
    xmlGetProp(top, BAD_CAST(EXA_CONF_CLUSTER_NAME)), xmlFree);
  shared_ptr<xmlChar> xml_cluster_uuidstr(
    xmlGetProp(top, BAD_CAST(EXA_CONF_CLUSTER_UUID)), xmlFree);

  if (!xml_cluster_name || !xmlStrEqual(xml_cluster_name.get(),
					BAD_CAST(cluster_name.c_str())))
  {
    error_msg = "Cluster name does not match configuration file";
    return EXA_ERR_DEFAULT;
  }

  if (!xml_cluster_uuidstr)
  {
    error_msg = "Cluster in configuration file does not have a UUID";
    return EXA_ERR_DEFAULT;
  }

  exa_uuid_t cluster_uuid;
  uuid_scan(reinterpret_cast<char*>(xml_cluster_uuidstr.get()), &cluster_uuid);

  cluster = shared_ptr<ClusterCache>(new ClusterCache(cluster_name, cluster_uuid, license));

  return update_cache_from_config(xml_cfg, error_msg);
}


string Exabase::get_cluster() const
{
  return cluster->name;
}


const exa_uuid_t &Exabase::get_cluster_uuid()
{
  return cluster->uuid;
}


const string &Exabase::get_license()
{
    return cluster->get_license();
}


void Exabase::del_cluster(void)
{
    try
    {
        cluster->remove();
    }
    catch(exception &ex)
    {
        exa_cli_error("%sERROR%s, %s", COLOR_ERROR, COLOR_NORM, ex.what());
    }

    // FIXME Why reset even if cache deletion failed?
    cluster.reset();
}

exa_error_code Exabase::update_cache_from_config(const xmlDocPtr xml_cfg,
						 string &error_msg)
{
  xmlNodePtr top(xml_conf_xpath_singleton(xml_cfg,
					  "/Exanodes/cluster[@name][@uuid]"));

  if (!top)
  {
    error_msg = "No cluster found in the config file";
    return EXA_ERR_DEFAULT;
  }

  shared_ptr<xmlChar> xml_cluster_name(
    xmlGetProp(top, BAD_CAST(EXA_CONF_CLUSTER_NAME)), xmlFree);
  shared_ptr<xmlChar> xml_cluster_uuidstr(
    xmlGetProp(top, BAD_CAST(EXA_CONF_CLUSTER_UUID)), xmlFree);

  if (!xml_cluster_name || !xml_cluster_uuidstr)
  {
    error_msg = "Missing cluster properties in the configuration file";
    return EXA_ERR_DEFAULT;
  }

  const string cluster_name(
    reinterpret_cast<char*>(xml_cluster_name.get()));
  const string cluster_uuidstr(
    reinterpret_cast<char*>(xml_cluster_uuidstr.get()));

  exa_uuid_t cluster_uuid;
  uuid_scan(cluster_uuidstr.c_str(), &cluster_uuid);

  shared_ptr<xmlNodeSet> nodeset(
    xml_conf_xpath_query(xml_cfg, string("/Exanodes/cluster[@name='"
					 + cluster_name + "'][@uuid='"
					 + cluster_uuidstr
					 + "']/node[@name]").c_str()),
    xmlXPathFreeNodeSet);

  cluster->clear_nodes();
  for (int i = 0; i < xmlXPathNodeSetGetLength(nodeset); ++i)
  {
    xmlNodePtr item = xmlXPathNodeSetItem(nodeset, i);

    shared_ptr<xmlChar> xml_nodename(
        xmlGetProp(item, BAD_CAST(EXA_CONF_NODE_NAME)), xmlFree);
    shared_ptr<xmlChar> xml_hostname(
        xmlGetProp(item, BAD_CAST(EXA_CONF_NODE_HOSTNAME)), xmlFree);

    string nodename(reinterpret_cast<char *>(xml_nodename.get()));
    string hostname(reinterpret_cast<char *>(xml_hostname.get()));

    cluster->add_node(nodename, hostname);
  }

  return config_cache_save(error_msg);
}


exa_error_code Exabase::set_config_node_add(std::string nodename,
                                            std::string hostname,
					    string &error_msg)
{
  cluster->add_node(nodename, hostname);
  return config_cache_save(error_msg);
}


// FIXME Should use nodename instead of hostname
exa_error_code Exabase::set_config_node_del(std::string hostname,
					    string &error_msg)
{
  cluster->del_node(hostname);
  return config_cache_save(error_msg);
}

exa_error_code Exabase::set_license(const std::string license,
				    std::string &error_msg)
{
  cluster->set_license(license);
  return config_cache_save(error_msg);
}



exa_error_code Exabase::config_cache_save(string &error_msg)
{
  try
  {
    cluster->save();
  }
  catch (exception &ex)
  {
    error_msg = ex.what();
    return EXA_ERR_WRITE_FILE;
  }

  return EXA_SUCCESS;
}


/**
 * Create and return the directory to store the log file.
 *
 * @return
 * - $EXANODES_LOG_DIR if it is set, or
 * - $HOME/.exanodes/log ($APPDATA\exanodes\log) else.
 */
static string create_and_get_log_dir(void)
{
    const char *env_log  = getenv("EXANODES_LOG_DIR");
    const char *env_home = os_user_get_homedir();
    string dir;
    int ret;

    if (env_log)
        dir = string(env_log);
    else if (env_home)
        dir = string(env_home) + OS_FILE_SEP + UI_LOCAL_LOG_DIR;
    else
        throw runtime_error("Neither EXANODES_LOG_DIR nor " OS_USER_HOMEDIR_VAR
                            " environment variable is set.");

    if (!os_path_is_absolute(dir.c_str()))
        throw runtime_error("The log directory must be an absolute path.");

    ret = os_dir_create_recursive(dir.c_str());
    if (ret != 0)
        throw runtime_error(string("Failed to create directory '") +
                            dir + "' (" + os_strerror(-ret) + ").");

    return dir;
}


void Exabase::log(const string &description)
{
  string message;
  string filename;

  if(!cluster)
      return;
  // FIXME Use create_dir() instead?

  filename = create_and_get_log_dir() + OS_FILE_SEP + UI_LOG_PREFIX + cluster->name + UI_LOG_EXT;

  char strtime[sizeof("YYYY-MM-DD hh:mm:ss.iii")];
  struct timeval t;
  struct tm date;
  time_t seconds;

  os_gettimeofday(&t);

  /* Warning first field of timeval is NOT a time_t (len=64bit) on windows but
   * is actually a long (len=32bit); thus passing a pointer on t.tv_sec to
   * os_localtime is wrong as the expected pointee does not have the right
   * size */
  seconds = t.tv_sec;
  os_localtime(&seconds, &date);

  os_snprintf(strtime, sizeof(strtime), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
	      1900 + date.tm_year, date.tm_mon + 1, date.tm_mday,
	      date.tm_hour, date.tm_min, date.tm_sec, t.tv_usec / 1000);

  std::ofstream flog(filename.c_str(), std::ios::app);

  message = string(strtime) + ": " + description;
  flog.write(message.c_str() , message.size());
  flog.put('\n');

  flog.close();
}

std::set<std::string> Exabase::get_hostnames() const
{
  const std::map<std::string, std::string> &nodes = cluster->get_node_map();
  std::map<std::string, std::string>::const_iterator it;
  std::set<std::string> hostnames;

  for (it = nodes.begin(); it != nodes.end(); it++)
      hostnames.insert(it->first);

  return hostnames;
}

std::set<std::string> Exabase::get_nodenames() const
{
  const std::map<std::string, std::string> &nodes = cluster->get_node_map();
  std::map<std::string, std::string>::const_iterator it;
  std::set<std::string> nodenames;

  for (it = nodes.begin(); it != nodes.end(); it++)
      nodenames.insert(it->second);

  return nodenames;
}

const std::string &Exabase::to_nodename(const std::string &hostname) const
{
    return cluster->to_nodename(hostname);
}
