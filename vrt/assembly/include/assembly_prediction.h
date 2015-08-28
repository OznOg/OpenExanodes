/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef ASSEMBLY_PREDICTION_H
#define ASSEMBLY_PREDICTION_H

#include "os/include/os_inttypes.h"

/**
 * Maximum number of slots that can be assembled without sparing.
 *
 * @param[in] n            Number of spofs
 * @param[in] w            Slot width, in chunks
 * @param[in] spof_chunks  Array of per-spof number of chunks
 *
 * NOTES:
 *   - The array need not be sorted.
 *   - All chunk numbers must be non-zero.
 *
 * @return maximum number of slots if successful, 0 otherwise
 */
uint64_t assembly_predict_max_slots_without_sparing(uint64_t n, uint64_t w,
                                                    const uint64_t *spof_chunks);

/**
 * Maximum number of slots that can be assembled with sparing, in the
 * case of shared reservation and with an assembly on all columns, including
 * the last one (see "Spare chunk pool - Principle and guarantees").
 *
 * @param[in] n            Number of spofs
 * @param[in] f            Number of spof failures to tolerate
 * @param[in] w            Slot width, in chunks
 * @param[in] spof_chunks  Array of per-spof number of chunks
 *
 * NOTES:
 *   - The array need not be sorted.
 *   - All chunk numbers must be non-zero.
 *
 * @return maximum number of slots if successful, 0 otherwise
 */
uint64_t assembly_predict_max_slots_reserved_with_last(uint64_t n, uint64_t f,
                                                       uint64_t w,
                                                       const uint64_t *spof_chunks);

/**
 * Maximum number of slots that can be assembled with sparing, in the
 * case of shared reservation and with an assembly on all columns but
 * the last one (see "Spare chunk pool - Principle and guarantees").
 *
 * @param[in] n            Number of spofs
 * @param[in] f            Number of spof failures to tolerate
 * @param[in] w            Slot width, in chunks
 * @param[in] spof_chunks  Array of per-spof number of chunks
 *
 * NOTES:
 *   - The array need not be sorted.
 *   - All chunk numbers must be non-zero.
 *
 * @return maximum number of slots if successful, 0 otherwise
 */
uint64_t assembly_predict_max_slots_reserved_without_last(uint64_t n, uint64_t f,
                                                          uint64_t w,
                                                          const uint64_t *spof_chunks);

#endif /* ASSEMBLY_PREDICTION_H */
