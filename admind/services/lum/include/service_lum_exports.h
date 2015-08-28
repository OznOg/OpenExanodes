/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef SERVICE_LUM_EXPORTS_H
#define SERVICE_LUM_EXPORTS_H

#include "admind/services/lum/include/adm_export.h"
#include "lum/export/include/export.h"
#include "target/iscsi/include/lun.h"
#include "common/include/uuid.h"

#define IQN_FILTER_DEFAULT_POLICY IQN_FILTER_ACCEPT

#define EXPORTS_VERSION_DEFAULT 1

/**
 * @brief Parse the exports XML from buffer.
 *
 * @param[in] contents  the XML string to parse
 *
 * @return EXA_SUCCESS if the operation succeeded, a negative error
 * code otherwise.
 */
exa_error_code lum_exports_parse_from_xml(const char *contents);

/**
 * @brief Dump the export to an XML buffer.
 *
 * @param[out] err  the possible error code
 *
 * @return A null terminated string containing the XML tree of the
 * exports, or NULL on error.
 */
char *lum_exports_to_xml(exa_error_code *err);

/**
 * @brief Get the Nth export.
 *
 * @param[in] n     The number of the export to get
 *
 * @return The export if found, NULL otherwise.
 */
const adm_export_t *lum_exports_get_nth_export(int n);

/**
 * @brief Get the number of loaded exports.
 *
 * @return A positive number or zero.
 */
int lum_exports_get_number(void);

/**
 * @brief Get the version number of the exports table.
 *
 * @return A positive number.
 */
uint64_t lum_exports_get_version(void);

/**
 * @brief Set the version number of the exports table to an
 * arbitrary number.
 *
 * @param[in] version 	The version to use
 *
 */
void lum_exports_set_version(int version);

/**
 * @brief Insert an export in the exports table.
 * The caller loses ownership of the export
 * pointer and must not free it afterwards.
 *
 * @param[in] export	The export to use;
 *
 * @return EXA_SUCCESS if the insertion succeeded, -E2BIG if the exports table
 *         is full.
 */
int lum_exports_insert_export(adm_export_t *export);

#define lum_exports_for_each_export(export_num, export) \
  for((export_num) = 0, (export) = lum_exports_get_nth_export(0); \
      (export_num) < lum_exports_get_number(); \
      (export_num)++, (export) = lum_exports_get_nth_export(export_num) \
  )

#endif /* SERVICE_LUM_EXPORTS_H */
