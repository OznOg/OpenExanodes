/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __CLUSTERCACHE_H__
#define __CLUSTERCACHE_H__

#include <map>
#include <string>

#include "common/include/uuid.h"


/**
 * Cluster cache.
 *
 * The methods that change the list of nodes do NOT automatically save
 * the file, you have to call the save() method explicitly.
 */
class ClusterCache
{
    std::map<std::string, std::string> nodes; /**< <Nodename, hostname> map */

public:
    const std::string name;  /**< Cluster name */
    const exa_uuid_t uuid;   /**< Cluster UUID */

    /**
     * Build a cache from an existing cache file.
     * Throws a runtime_error exception on error.
     *
     * @param[in] name  Cluster name
     */
    explicit ClusterCache(const std::string &_name);

    /**
     * Build a en empty cache.
     *
     * @param[in] name  Cluster name
     * @param[in] uuid  Cluster UUID
     * @param[in] 
     */
    explicit ClusterCache(const std::string &_name, const exa_uuid_t &_uuid,
                          const std::string &_license):
        name(_name),
        uuid(_uuid),
        license(_license)
    {
    }

    /**
     * Add a node to the cache.
     * If the node is already in the cache, it is overwritten.
     *
     * @param[in] nodename  Name (alias) of the node
     * @param[in] hostname  Hostname of the node
     */
    void add_node(const std::string &nodename, const std::string &hostname);

    /**
     * Delete a node from the cache.
     * Silently ignores the deletion of an unknown node.
     *
     * @param[in] nodename  Name (alias) of the node to delete
     */
    void del_node(const std::string &nodename);

    /**
     * Delete all nodes from the cache.
     */
    void clear_nodes(void);

    /**
     * Gives the nodename corresponding to a hostname.
     *
     * @param[in] hostname  hostname corresponding to the node
     *
     * @return nodename if node exists or "" if no such node
     * FIXME this should probably throw an exception in place of returning
     *       an empty string.
     */
    const std::string &to_nodename(const std::string &hostname);

    /** Get the node definitions.
     *
     * XXX Replace with an iterator? Or two accessors, one for nodenames and
     * one for hostnames?
     *
     * @return <nodename, hostname> map
     */
    const std::map<std::string, std::string> &get_node_map(void) const;

    /**
     * Save the cluster cache.
     *
     * Throws a runtime exception if the cache already exists but has a
     * different uuid or if the saving fails.
     */
    void save(void) const;
    /**
     * Dump the contents of the cache to stderr.
     * Meant for debugging purposes only.
     * XXX Remove?
     */
    void dump(void) const;

    /**
      * Get the license of the cluster.
      */
    const std::string &get_license(void);

    /**
     * Set the license of the cluster.
     *
     * @param[in] new_license   The new license string
     */
    void set_license(const std::string new_license);

    void remove(void);

private:
    std::string license;   /**< Cluster license */
    std::string load_license(void);
    void save_license(void) const;
    void remove_license(void);
};


#endif /* __CLUSTERCACHE_H__ */
