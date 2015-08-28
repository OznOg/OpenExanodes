/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXABASE_H__
#define  __EXABASE_H__

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <libxml/tree.h>

#include "common/include/exa_error.h"
#include <set>

class ClusterCache;
struct exa_uuid;


class Exabase: private boost::noncopyable
{
public:
  static xmlDocPtr parse_config_file(const std::string &config_file);

  Exabase();
  virtual ~Exabase();

  /* To use this class, you need first to initialize the cache with
   * one of the set_cluster_from_* methods. */
  int set_cluster_from_config(const std::string &_clusterName, xmlDocPtr xml_cfg,
                              const std::string &license, std::string &error_msg);
  int set_cluster_from_cache(const std::string &clusterName, std::string &error_msg);

  /* Updates the list of nodes in the cache using the provided XML
   * configuration tree. If it does not return EXA_SUCCESS, a
   * description of the error will be put in "error_msg". */
  exa_error_code update_cache_from_config(const xmlDocPtr configDocPtr,
					  std::string &error_msg);

  /* Methods to add or remove a node from the cache file. */
  exa_error_code set_config_node_add(std::string nodename, std::string hostname,
				     std::string &error_msg);
  exa_error_code set_config_node_del(std::string nodename,
				     std::string &error_msg);

  std::string get_cluster() const;
  const exa_uuid &get_cluster_uuid();
  const std::string &get_license(void);
  exa_error_code set_license(const std::string license,
                             std::string &error_msg);

  /* This method log a description in the cluster log file. It will throw an
   * exception if there is a problem. */
  void log(const std::string &description);

  /* Deletes the current cluster's cache file. When it returns, the
   * Exabase object is invalid. */
  void del_cluster();

  std::set<std::string> get_hostnames() const;
  std::set<std::string> get_nodenames() const;

  const std::string &to_nodename(const std::string &hostname) const;

private:
  boost::shared_ptr<ClusterCache> cluster;

  exa_error_code config_cache_save(std::string &error_msg);

};


#endif /* __EXABASE_H__ */
