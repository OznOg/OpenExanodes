/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "ui/common/include/clustercache.h"

#include <errno.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"
#include "config/pkg_cache_dir.h"
#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_user.h"

using std::endl;
using std::ifstream;
using std::ofstream;
using std::runtime_error;
using std::map;
using std::string;
using boost::lexical_cast;

#include <vector>
using std::vector;

/** Directory for normal users (relative to home directory). */
#ifdef WIN32
#define UI_LOCAL_CACHE_DIR    "exanodes\\cache"
#else
#define UI_LOCAL_CACHE_DIR    ".exanodes/cache"
#endif

/** Extension of the node cache files */
#define UI_NODES_EXT          ".nodes"

/** Extension of the license file */
#define UI_LICENSE_EXT        ".license"

/**
 * Create and return the directory to store the cache file.
 *
 * @return
 * - $EXANODES_CACHE_DIR if it is set, or
 * - $HOME/.exanodes/cache ($APPDATA\exanodes\cache) else.
 */
static string create_and_get_cache_dir(void)
{
    const char *env_cache = getenv("EXANODES_CACHE_DIR");
    const char *env_home  = os_user_get_homedir();
    string dir;
    int ret;

    if (env_cache)
        dir = string(env_cache);
    else if (env_home)
        dir = string(env_home) + OS_FILE_SEP + UI_LOCAL_CACHE_DIR;
    else
        throw runtime_error("Neither EXANODES_CACHE_DIR nor " OS_USER_HOMEDIR_VAR
                            " environment variable is set.");

    if (!os_path_is_absolute(dir.c_str()))
        throw runtime_error("The cache directory must be an absolute path.");

    ret = os_dir_create_recursive(dir.c_str());
    if (ret != 0)
        throw runtime_error(string("Failed to create directory '") +
                            dir + "' (" + os_strerror(-ret) + ").");

    return dir;
}


void ClusterCache::remove()
{
  string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_NODES_EXT);
  unlink(filename.c_str());
  remove_license();
}


/**
 * Parse a cluster cache file.
 *
 * @param[in]  name  Name of the cluster
 * @param[out] nodes Resulting <nodename,hostname> map
 *
 * @return uuid of cluster if successful, throws a runtime error otherwise
 */
static exa_uuid_t parse_cache_file(const string &name,
				   map<string, string> &nodes)
{
  string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_NODES_EXT);
  ifstream cache_file(filename.c_str());

  if (!cache_file.good())
    throw runtime_error("could not open cache file " + filename
			+ ".\nIf the cluster exists,"
                        + " you can reconnect to it using: exa_clreconnect "
                        + name + " --node <one hostname>.");

  string uuidstr;

  if (!getline(cache_file, uuidstr))
    throw runtime_error("unexpected end of file reading cache file " + filename);

  if (uuidstr.find(':') == string::npos)
    throw runtime_error("deprecated cache file " + filename);

  exa_uuid_t uuid;

  if (uuid_scan(uuidstr.c_str(), &uuid) != 0)
    throw runtime_error("failed to parse UUID from cache file " + filename);

  string node_line;

  while (getline(cache_file, node_line))
  {
    if (node_line.empty())
	continue;

    vector<string> elems;
    boost::split(elems, node_line, boost::algorithm::is_any_of(" "));
    if (elems.size() != 2)
	throw runtime_error("invalid node line: '" + node_line + "'");

    string node(elems[0]);
    string host(elems[1]);

    nodes[host] = node;
  }

  if (nodes.empty())
      throw runtime_error("no nodes in cache file " + filename);

  return uuid;
}

ClusterCache::ClusterCache(const string &_name):
    nodes(),
    name(_name),
    uuid(parse_cache_file(_name, nodes)),
    license(load_license())
{
}

void ClusterCache::add_node(const std::string &nodename,
                            const std::string &hostname)
{
    if (nodename == "")
        throw runtime_error("invalid nodename");

    if (hostname == "")
        throw runtime_error("invalid hostname");

    nodes[hostname] = nodename;
}

void ClusterCache::del_node(const std::string &nodename)
{
    nodes.erase(nodename);
}

void ClusterCache::clear_nodes()
{
    nodes.clear();
}

const std::string &ClusterCache::to_nodename(const std::string &hostname)
{
    return nodes[hostname];
}

const std::map<std::string, std::string> &ClusterCache::get_node_map() const
{
    return nodes;
}

void ClusterCache::save() const
{
  string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_NODES_EXT);
  bool same_uuid(false);

  try
  {
    ClusterCache cache(name);

    same_uuid = uuid_is_equal(&cache.uuid, &uuid);
  }
  catch (...)
  {
    /* Means that the cache file doesn't exist or is invalid. */
    same_uuid = true;
  }

  if (!same_uuid)
    throw runtime_error("existing cache file with different UUID for "
                        "cluster " + name + "\n"
			"If this cluster's nodes cache no more represents one of your Exanodes cluster,\n"
			"please delete it and run again this command.\n"
			"If this cluster's nodes cache represents an existing Exanodes cluster,\n"
			"please use another name to create your new cluster.\n"
			"The cluster node cache is: " + filename + "\n");

  string tmp_filename(filename + ".tmp");
  ofstream cache_file(tmp_filename.c_str());

  exa_uuid_str_t uuidstr;
  uuid2str(&uuid, uuidstr);

  cache_file << uuidstr << endl;

  map<string, string>::const_iterator it;

  for (it = nodes.begin(); it != nodes.end(); ++it)
      cache_file << it->second << " " << it->first << endl;

  if (!cache_file.good())
  {
    unlink(tmp_filename.c_str());
    throw runtime_error("Failed to save temporary cache file " + tmp_filename);
  }

  cache_file.close();

  if (os_file_rename(tmp_filename.c_str(), filename.c_str()) != 0)
    throw runtime_error("Failed to save cache file " + filename);

  save_license();
}


void ClusterCache::dump() const
{
    map<string, string>::const_iterator it;
    exa_uuid_str_t uuid_str;
    uuid2str(&uuid, uuid_str);

    std::cerr << "BEGIN DUMP CACHE " << endl;
    std::cerr << "Name: " << name << std::endl;
    std::cerr << " UUID: " << uuid_str << std::endl;
    for (it = nodes.begin(); it != nodes.end(); it++)
	std::cerr << "Node: " << it->second << " Host: " << it->first << std::endl;
    std::cerr << "END DUMP CACHE" << std::endl;
}


string ClusterCache::load_license()
{
    string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_LICENSE_EXT);
    std::ifstream license_file(filename.c_str(), std::ios::in);
    string buffer;

    if(!license_file.fail())
    {
        string line;

        while(getline(license_file, line))
            buffer += line + "\n";

        license_file.close();
    }

    return buffer;
}


void ClusterCache::save_license() const
{
    /* Do not save an empty license file when not using licenses. */
    if (license.empty())
        return;

    string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_LICENSE_EXT);
    std::ofstream license_file(filename.c_str());

    license_file << license;

    if (!license_file.good())
    {
        unlink(filename.c_str());
        throw runtime_error("Failed to save license file " + filename);
    }
}


void ClusterCache::set_license(const std::string new_license)
{
    license = new_license;
}


void ClusterCache::remove_license()
{
    string filename(create_and_get_cache_dir() + OS_FILE_SEP + name + UI_LICENSE_EXT);
    int ret;

    ret = unlink(filename.c_str());
  /* Discard error if the license file does not exist because Exanodes
   * can be used without license. */
    if (ret == -1 && errno != ENOENT)
        throw runtime_error("Failed to delete license file " + filename);
}


const std::string &ClusterCache::get_license()
{
    return license;
}
