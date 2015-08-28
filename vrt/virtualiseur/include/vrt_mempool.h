/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#ifndef __VRT_MEMPOOL_H__
#define __VRT_MEMPOOL_H__

struct vrt_object_pool;

struct vrt_object_pool *vrt_mempool_create (unsigned int object_size,
					    unsigned int object_total_count);
void vrt_mempool_destroy (struct vrt_object_pool *pool);
void *vrt_mempool_object_alloc (struct vrt_object_pool *pool);
void __vrt_mempool_object_free (struct vrt_object_pool *pool, void *obj);
#define vrt_mempool_object_free(pool,obj) \
    (__vrt_mempool_object_free(pool, obj), obj = NULL)

#endif /* __VRT_MEMPOOL_H__ */
