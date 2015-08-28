/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TUNELIST_COMMON_H
#define __TUNELIST_COMMON_H

#define EXA_MAXSIZE_TUNE_NAME          32
#define EXA_MAXSIZE_TUNE_VALUE         128
#define EXA_MAXSIZE_TUNE_DESCRIPTION   512

#define TUNE_EMPTY_LIST "empty list"

typedef char tune_value_t[EXA_MAXSIZE_TUNE_VALUE];

typedef struct tune_t tune_t;

typedef struct tunelist tunelist_t;


/**
 * @brief Allocate and initialize tuning structure for the first use.
 *
 * @param[in] nb_values  the number of values corresponding to the tuning element.
 *
 * @return pointer to the newly created structure or NULL in case of failure
 */
tune_t* tune_create(unsigned int nb_values);

/**
 * @brief Set the name of the tuning structure.
 *
 * @param[in] tune      the tuning structure to change.
 * @param[in] name       the new name of the tuning.
 *
 */
void tune_set_name(tune_t *tune, const char *name);

/**
 * @brief Get the name of the tuning structure.
 *
 * @param[in] tune      the tuning structure.
 *
 * @return the name of the tuning structure
 */
const char *tune_get_name(const tune_t *tune);

/**
 * @brief Set the description of the tuning structure.
 *
 * @param[in] tune      the tuning structure to change.
 * @param[in] descr     the new description of the tuning.
 *
 */
void tune_set_description(tune_t *tune, const char *descr);

/**
 * @brief Set the Nth value of the tuning structure.
 *
 * @param[in] tune      the tuning structure to change.
 * @param[in] n         the index at which to set the value.
 * @param[in] fmt       the value to set.
 *
 */
void tune_set_nth_value(tune_t *tune, unsigned int n, const char *fmt, ...)
        __attribute__ ((__format__ (__printf__, 3, 4)));

/**
 * @brief Set the default value of the tuning structure.
 *
 * @param[in] tune      the tuning structure to change.
 * @param[in] fmt       the value to set.
 *
 */
void tune_set_default_value(tune_t *tune, const char *fmt, ...)
        __attribute__ ((__format__ (__printf__, 2, 3)));

/**
 * @brief Deallocate a tuning structure
 *
 * @param[in] tune  the structure to deallocate
 */
void tune_delete(tune_t* tune);

/**
 * @brief create an handler and do subsequent allocation
 *
 * @param[out] handle   points to any tunelist_handle_t
 *
 * @return all possible allocation errors, or success
 */
int tunelist_create(tunelist_t** tunelist);

/**
 * @brief Abort all previous operations, free memory as necessary.
 *        Every create must be paired with a delete
 *
 * @param[in] tunelist handle to a tunelist created with tunelist_create
 *
 * @return None. Always success.
 */
void tunelist_delete(tunelist_t* tunelist);

/**
 * @brief Feed the XML tree with another name/value.
 *
 * @param[in] tunelist     handler created with tunelist_create
 * @param[in] tune         description of the tuning parameter
 *
 * @return all possible allocation errors, or success
 */
int tunelist_add_tune(tunelist_t* tunelist, tune_t* tune);

/**
 * @brief Create a result that can be used as an "opaque" string.
 *
 * The buffer provided by this function is owned by the object tunelist.
 * The buffer MUST NOT be freed. In addition, it MUST NOT be used after
 * 'tunelist_delete' is called or after 'tunelist_get_result' is called
 * a second time.
 *
 * @param[in]  tunelist   handle to a tunelist
 *
 * @return the pointer to the "opaque" string.
 */
const char *tunelist_get_result(tunelist_t* tunelist);

#endif
