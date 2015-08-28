/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/*
 * DOCUMENT
 *
 * This module implements the formulas from
 * "Spare chunk pool - Principle and guarantees".
 *
 * NOTATION
 *
 * The notation used throughout this module is as close as possible to
 * the one used in the document.
 *
 * Most notably:
 *
 *    E: effective number of chunks
 *    n: number of spofs
 *    f: number of spof failures to tolerate
 *    w: slot width, in chunks
 */

#include "vrt/assembly/include/assembly_prediction.h"
#include "common/include/exa_assert.h"
#include "common/include/exa_math.h"

#include <stdlib.h>
#include <string.h> /* for memcpy() */

/* Comparison function used to sort numbers (of chunks) in
   increasing order. */
static int chunk_count_gt(const void *a, const void *b)
{
    uint64_t c1 = *(uint64_t *)a;
    uint64_t c2 = *(uint64_t *)b;

    if (c1 < c2)
        return -1;
    if (c1 > c2)
        return +1;

    return 0;
}

/**
 * Sort per-spof chunk numbers in increasing order.
 *
 * @param[in,out] spof_chunks  Array of per-spof number of chunks
 * @param[in]     num_spofs    Number of spofs
 */
static void spof_chunks_sort(uint64_t *spof_chunks, unsigned num_spofs)
{
    qsort(spof_chunks, num_spofs, sizeof(uint64_t), chunk_count_gt);
}

/**
 * Effective number of chunks.
 *
 * @param[in] i            Rank (index) of the spof in spof_chunks
 * @param[in] n            Number of spofs
 * @param[in] w            Width of the slots to assemble
 * @param[in] spof_chunks  Number of chunks for each spof used for the
 *                         assembly; must be sorted in increasing order
 *
 * @return Number of chunks that could effectively be used for the assembly
 *         for one spof
 */
static uint64_t E(uint64_t i, uint64_t n, uint64_t w, const uint64_t *spof_chunks)
{
    EXA_ASSERT(i < n);

    if (i <= n - w)
        return spof_chunks[i];

    if (i < n)
    {
        uint64_t j;
        uint64_t num;
        uint64_t thingy;

        num = 0;
        for (j = 0; j < i; j++)
            num += E(j, n, w, spof_chunks);

        thingy = num / (i - n + w);

        return MIN(spof_chunks[i], thingy);
    }

    /* Shouldn't happen (cf assertion at beginning) */
    return 0;
}

uint64_t assembly_predict_max_slots_without_sparing(uint64_t n, uint64_t w,
                                                    const uint64_t *spof_chunks)
{
    uint64_t _spof_chunks[n];
    uint64_t num;
    uint64_t i;

    if (w > n)
        return 0;

    memcpy(_spof_chunks, spof_chunks, sizeof(_spof_chunks));
    spof_chunks_sort(_spof_chunks, n);

    num = 0;
    for (i = 0; i < n; i++)
        num += E(i, n, w, _spof_chunks);

    return num / w;
}

/* Maximum number of slots - assembly *with* sparing */
static uint64_t __s_max(uint64_t n, uint64_t f, uint64_t w,
                        const uint64_t *spof_chunks)
{
    uint64_t num;
    uint64_t i;

    num = 0;
    for (i = 0; i < n - f; i++)
        num += E(i, n - f, w, spof_chunks);

    return num / w;
}

static uint64_t __r_last(uint64_t n, uint64_t f, uint64_t w, uint64_t s_max,
                         uint64_t c_max, uint64_t r_max, const uint64_t *spof_chunks)
{
    uint64_t num;
    unsigned i;
    uint64_t s_last_col = s_max - (c_max - 1) * r_max;

    num = 0;
    for (i = 0; i < n - f; i++)
        num += E(i, n - f, w * c_max, spof_chunks);

    return MIN(num / (w * c_max), s_last_col);
}

uint64_t assembly_predict_max_slots_reserved_with_last(uint64_t n, uint64_t f,
                                                       uint64_t w,
                                                       const uint64_t *spof_chunks)
{
    uint64_t _spof_chunks[n];
    uint64_t r_max, s_max, c_max;
    uint64_t r_last;

    if (f > n || w > n)
        return 0;

    memcpy(_spof_chunks, spof_chunks, sizeof(_spof_chunks));
    spof_chunks_sort(_spof_chunks, n);

    r_max = E(n - f, n, w + f, _spof_chunks);
    s_max = __s_max(n, f, w, _spof_chunks);
    c_max = quotient_ceil64(s_max, r_max);
    r_last = __r_last(n, f, w, s_max, c_max, r_max, _spof_chunks);

    return r_max * (c_max - 1) + r_last;
}

static uint64_t __r_full(uint64_t n, uint64_t f, uint64_t w, uint64_t c_max,
                         const uint64_t *spof_chunks)
{
    uint64_t num, denum;
    uint64_t i;

    num = 0;
    for (i = 0; i < n; i++)
        num += E(i, n, w * (c_max - 1) + f, spof_chunks);

    denum = w * (c_max - 1) + f;

    return num / denum;
}

static uint64_t __s_extra(uint64_t n, uint64_t f, uint64_t w, uint64_t c_max,
                          uint64_t r_full, const uint64_t *spof_chunks)
{
    uint64_t i;
    uint64_t o_extra;

    o_extra = 0;
    for (i = 0; i < n; i++)
    {
        uint64_t efficiency_threshold = E(i, n, w * (c_max - 1) + f, spof_chunks) + 1;
        o_extra += MIN(efficiency_threshold, spof_chunks[i]);
    }
    o_extra -= r_full * (w * (c_max - 1) + f);

    if (o_extra >= w + f)
        return (o_extra - f) / w;
    else
        return 0;
}

uint64_t assembly_predict_max_slots_reserved_without_last(uint64_t n, uint64_t f,
                                                          uint64_t w,
                                                          const uint64_t *spof_chunks)
{
    uint64_t _spof_chunks[n];
    uint64_t r_max, s_max, c_max;
    uint64_t r_full, s_extra;

    if (f > n || w > n)
        return 0;

    memcpy(_spof_chunks, spof_chunks, sizeof(_spof_chunks));
    spof_chunks_sort(_spof_chunks, n);

    r_max = E(n - f, n, w + f, _spof_chunks);
    s_max = __s_max(n, f, w, _spof_chunks);
    c_max = quotient_ceil64(s_max, r_max);
    r_full = __r_full(n, f, w, c_max, _spof_chunks);
    s_extra = __s_extra(n, f, w, c_max, r_full, _spof_chunks);

    return r_full * (c_max - 1) + s_extra;
}
