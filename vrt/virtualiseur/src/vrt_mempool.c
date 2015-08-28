/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** @file vrt_mempool.c
 *
 * @brief Implementation of generic mempools for the virtualizer,
 * which allows to allocate and free objects while staying in defined
 * memory limits.
 *
 * A memory pool is a set of objects of the same size. The number of
 * objects in a pool is defined at creation time by
 * vrt_mempool_create() and cannot be changed afterwards. A pool
 * never grows in memory: when there aren't enough objects to satisfy
 * an allocation made by vrt_mempool_object_alloc(), the calling
 * thread blocks until an object becomes available.
 *
 * Of course, to remove a memory pool, one simply needs to call
 * vrt_mempool_destroy().
 */

#include <string.h>

#include "os/include/os_mem.h"
#include "common/include/exa_nbd_list.h"


struct vrt_object_pool
{
    struct nbd_root_list root;
};


/**
 * Create a pool of memory for a maximum of object_total_count objects
 * of size object_size. All the memory is allocated in this function,
 * and all further alloc/free will only work with this preallocated
 * pool.
 *
 * @param object_size        Size of each object in the pool
 *
 * @param object_total_count Maximal number of objects that can be
 *                           allocated from the pool
 *
 * @return Pointer to the mempool
 */
struct vrt_object_pool *vrt_mempool_create (unsigned int object_size,
					    unsigned int object_total_count)
{
    struct vrt_object_pool * pool = os_malloc(sizeof(struct vrt_object_pool));
    if (pool == NULL)
	return NULL;

    if (nbd_init_root(object_total_count, object_size, &pool->root) < 0)
    {
	os_free(pool);
	return NULL;
    }
    return pool;
}

/**
 * Destroy a mempool and all its objects. This function assumes that
 * all objects of the pool have been freed.
 *
 * @param pool The mempool to destroy
 */
void vrt_mempool_destroy (struct vrt_object_pool *pool)
{
    nbd_close_root(&pool->root);
    os_free(pool);
}

/**
 * Allocate an object from a pool. If no free objects are available,
 * then this function sleeps until some objects become free. This
 * function will never return NULL. Allocated objects are set to 0.
 *
 * @param pool The mempool from which the object must be allocated
 *
 * @return Pointer to the allocated object
 */
void *vrt_mempool_object_alloc (struct vrt_object_pool *pool)
{
    void * obj = nbd_list_remove(&pool->root.free, NULL, LISTWAIT);
    EXA_ASSERT(obj != NULL);

    memset(obj, 0, pool->root.elt_size);
    return obj;
}

/**
 * Free the given object in the given pool.
 *
 * @param pool The mempool
 *
 * @param obj  The object to free
 */
void __vrt_mempool_object_free (struct vrt_object_pool *pool, void *obj)
{
    nbd_list_post(&pool->root.free, obj, -1);
}

