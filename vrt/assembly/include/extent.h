/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __EXTENT_H__
#define __EXTENT_H__

#include "vrt/common/include/vrt_stream.h"
#include "os/include/os_inttypes.h"

typedef struct extent extent_t;
struct extent
{
    extent_t *next;
    uint64_t start;
    uint64_t end;
};

typedef struct
{
    uint64_t start;
    uint64_t end;
} flat_extent_t;

/**
 * Get the number of values in an extent list
 *
 * @param[in]   extent  The extent
 *
 * @return the number of values contained in the extent list
 */
uint64_t extent_list_get_num_values(const extent_t *extent);

/**
 * Count the number of extents in an extent list
 *
 * @param[in]   extent_list The list to count
 *
 * @return a positive integer (or zero if the list is NULL)
 */
uint32_t extent_list_count(const extent_t *extent_list);

/**
 * Append a value to a list of extents
 *
 * @param[in,out] extent_list   The existing list to append to, or NULL to create
 *                              a new list
 * @param[in]     value         The value to add
 *
 * @return    The pointer to the updated and consolidated extent list.
 */
extent_t *extent_list_add_value(extent_t *extent_list, uint64_t value);

/**
 * Remove a value from a list of extents
 *
 * @param[in,out] extent_list   The existing list to append to, or NULL to create
 *                              a new list
 * @param[in]     value         The value to remove
 *
 * @return    The pointer to the updated and consolidated extent list.
 */
extent_t *extent_list_remove_value(extent_t *extent_list, uint64_t value);

/**
 * Free an extent list.
 * Do not use this function directly. Instead, use macro extent_list_free().
 *
 * @param[in,out] extent_list   The list to free
 */
void __extent_list_free(extent_t *extent_list);
#define extent_list_free(list) (__extent_list_free(list), list = NULL)

int extent_list_serialize(const extent_t *extent_list, stream_t *stream);
int extent_list_deserialize(extent_t **extent_list, stream_t *stream);

/**
 * Checks whether an extent list contains a given value.
 *
 * @param[in] extent_list   The list to look into
 * @param[in] value         The value to look for
 *
 * @return true if the value is found, false otherwise.
 */
bool extent_list_contains_value(const extent_t *extent_list, uint64_t value);

typedef const extent_t extent_iter_t;
#define extent_iter_init(list, iter) do { iter = list; } while(0)
#define extent_list_iter_for_each(iter, value) \
    for (; iter != NULL; iter = iter->next) \
        for (value = iter->start; value <= iter->end; value++)

#endif /* __EXTENT_H__ */
