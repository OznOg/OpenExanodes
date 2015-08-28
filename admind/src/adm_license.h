/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __ADM_LICENSE_H
#define __ADM_LICENSE_H

#include <time.h>

#include <sys/types.h>

#include "common/include/exa_version.h"
#include "common/include/uuid.h"

#include "config/exa_version.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_file.h"
#include "admind/src/adm_error.h"

/** Name of the license file */
#define ADM_LICENSE_FILE   "license"

/** Grace period (in days) during which Exanodes is still usable even if the
 * license is expired */
#define ADM_LICENSE_GRACE_PERIOD 30

/** A license */
struct adm_license;
typedef struct adm_license adm_license_t;

/** License status */
typedef enum {
    ADM_LICENSE_STATUS_NONE = 0,   /**< No license available */
    ADM_LICENSE_STATUS_EXPIRED,    /**< The license is expired */
    ADM_LICENSE_STATUS_GRACE,      /**< The license is expired but we are still
				    *   in the "grace period"
				    */
    ADM_LICENSE_STATUS_EVALUATION, /**< Using an evaluation license not expired */
    ADM_LICENSE_STATUS_OK          /**< Using a registred license not expired */
} adm_license_status_t;

#define ADM_LICENSE_STATUS__FIRST  ADM_LICENSE_STATUS_NONE
#define ADM_LICENSE_STATUS__LAST   ADM_LICENSE_STATUS_OK

#define ADM_LICENSE_STATUS_IS_VALID(status) \
    ((status) >= ADM_LICENSE_STATUS__FIRST && (status) <= ADM_LICENSE_STATUS__LAST)

/**
 * Get the string representation of a status.
 *
 * This function is thread safe and there is no need to duplicate the
 * returned string, even in a single thread.
 *
 * @param[in] status  License status
 *
 * @return String if the status is valid, NULL otherwise
 */
const char *adm_license_status_str(adm_license_status_t status);

/**
 * Get the string representation of the license type.
 *
 * This function is thread safe and there is no need to duplicate the
 * returned string, even in a single thread.
 *
 * @param[in] license  License
 *
 * @return String if the license is valid, NULL otherwise
 */
const char *adm_license_type_str(adm_license_t *license);

/**
 * Initialization of the license module.
 * *Must* be called before using any other function from this module.
 *
 * @return EXA_SUCCESS or a negative error code
 */
int adm_license_static_init(void);

/**
 * Deserialize (parse) a license from a file.
 *
 * @param[in]  filename    File to parse
 * @param[out] error_desc  Error descriptor
 *
 * @return license if the file was successfully read (which means the license
 *         is valid), and NULL otherwise
 */
adm_license_t *adm_deserialize_license(const char *filename,
                                       cl_error_desc_t *error_desc);

/**
 * Check whether the license allows the specified number of nodes.
 *
 * @param[in]  license     License to check against
 * @param[in]  nb_nodes    Number of nodes
 * @param[out] error_desc  Error descriptor
 *
 * @return true if this many nodes is allowed, false otherwise
 */
bool adm_license_nb_nodes_ok(const adm_license_t *license, uint32_t nb_nodes,
                             cl_error_desc_t *error_desc);

/**
 * Check whether the license allows the specified size of storage to be created.
 *
 * @param[in]  license     License to check against
 * @param[in]  size        Size
 * @param[out] error_desc  Error descriptor
 *
 * @return true if this size is allowed, false otherwise
 */
bool adm_license_size_ok(const adm_license_t *license, uint64_t size,
                         cl_error_desc_t *error_desc);

/**
 * Install a license.
 *
 * The license is saved to a file.
 *
 * @param[in]  data        Data to be used as license,
 *                         validated during installation
 * @param[in]  size        Size of data, in bytes
 * @param[out] error_desc  Error descriptor
 *
 * @return license if successfully installed, NULL otherwise
 */
adm_license_t *adm_license_install(const char *data, size_t size,
                                   cl_error_desc_t *error_desc);

/**
 * Uninstall the license.
 *
 * The license is freed and the license file is deleted.
 *
 * @param[in, out] license  License to uninstall.
 */
void adm_license_uninstall(adm_license_t *license);

/**
 * Allocate a license with the specified settings.
 *
 * Does not perform any validity check on the settings.
 *
 * @param[in] licensee       Name of licensee
 * @param[in] uuid           License UUID
 * @param[in] expiry         Expiration date
 * @param[in] major_version  The major version of Exanodes the license is for
 * @param[in] max_nodes      The maximum number of nodes allowed by the license
 *
 * @return license if successfully allocated, NULL otherwise
 */
adm_license_t *adm_license_new(const char *licensee, const exa_uuid_t *uuid,
                               struct tm expiry, const exa_version_t major_version,
                               uint32_t max_nodes, bool is_eval);

/**
 * Free a license.
 *
 * @param[in,out] license  License to be freed
 */
void adm_license_delete(adm_license_t *license);

/**
 * Get the expiry status of a license.
 *
 * @param[in] license  License to get the status of
 *
 * @return a license status
 */
adm_license_status_t adm_license_get_status(const adm_license_t *license);

/**
 * Get the evaluation status of a license.
 *
 * @param[in] license  License to get the evaluation status of
 *
 * @return true if the license is an evaluation license, false if it is full
 */
bool adm_license_is_eval(const adm_license_t *license);

/**
 * Get the HA status of a license.
 *
 * @param[in] license  License to get the evaluation status of
 *
 * @return true if the license includes HA, false if it doesn't
 */
bool adm_license_has_ha(const adm_license_t *license);

/**
 * Get the maximum logical size the license allows.
 *
 * @param[in]  license     License to check against
 *
 * @return the maximum size allowed (in KB)
 */
uint64_t adm_license_get_max_size(const adm_license_t *license);

/**
 * Get the number of remaining days before expiry.
 *
 * Gives the number of days before the expiration date or before the end of
 * the grace period, depending on the arguments.
 *
 * @param[in] license       The license
 * @param[in] grace_period  Whether to take the grace period into account
 *
 * @return the number of days remaining before expiry, 0 if already expired
 */
unsigned int adm_license_get_remaining_days(const adm_license_t *license,
                                            bool grace_period);

/**
 * Get a license's UUID.
 *
 * @param[in] license  License to get the UUID of. Cannot be NULL.
 *
 * @return the license's UUID
 */
const exa_uuid_t *adm_license_get_uuid(const adm_license_t *license);

/**
 * Create a license by parsing an XML buffer.
 *
 * NOTE: Meant for unit testing purposes only. Do not use in production code.
 *
 * @param[in] buffer  XML buffer
 * @param[in] len     Length of buffer (not the size!)
 *
 * @return license if successfully parsed, NULL otherwise
 */
adm_license_t *adm_license_new_from_xml(const char *buffer, size_t len);

/**
 * Check whether the license matches the running product.
 *
 * Or, if you prefer: tells whether this very program (product and version)
 * is allowed to run by the specified license.
 *
 * @param[in]  license     License to check against
 * @param[out] error_desc  Error descriptor
 *
 * @return true if the product is allowed to run, false otherwise
 */
bool adm_license_matches_self(const adm_license_t *license,
                              cl_error_desc_t *error_desc);

/**
 * Return the uncyphered text from a cyphered MIME signed buffer.
 * NOTE: the ownership of the return buffer is lost by this function;
 * thus the returned string MUST be freed by the caller by calling os_free().
 *
 * @param[in]  cypher      cyphered data
 * @param[in]  len         cyphered data len
 * @param[out] error_desc  Error descriptor
 *
 * @return '\0' terminated buffer of the uncyphered text
 */
char *adm_license_uncypher_data(char *cypher, size_t len,
                                cl_error_desc_t *error_desc);

#endif /* __ADM_LICENSE_H */
