/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/checksum.h"
#include "common/include/exa_assert.h"

checksum_t exa_checksum(const void *buffer, size_t size)
{
    checksum_context_t ctx;

    checksum_reset(&ctx);
    checksum_feed(&ctx, buffer, size);

    return checksum_get_value(&ctx);
}

void checksum_reset(checksum_context_t *ctx)
{
    ctx->total_size = 0;
    ctx->latched_byte = 0;
    ctx->latched = false;
    ctx->sum = 0;
}

static uint32_t __chksum_pair(uint32_t sum, uint8_t a, uint8_t b)
{
    uint16_t word16 = a + (b << 8);
    return sum + (uint32_t)word16;
}

static uint32_t __chksum_array(uint32_t sum, const uint8_t *buf, size_t size)
{
    int i;
    uint32_t new_sum = sum;

    if (size % 2 != 0)
        size--;

    for (i = 0; i < size; i += 2)
        new_sum = __chksum_pair(new_sum, buf[i], buf[i + 1]);

    return new_sum;
}

void checksum_feed(checksum_context_t *ctx, const void *buffer, size_t size)
{
    const uint8_t *byte_buf = buffer;

    if (buffer == NULL || size == 0)
        return;

    if (ctx->latched)
    {
        ctx->sum = __chksum_pair(ctx->sum, ctx->latched_byte, byte_buf[0]);
        if (size % 2 == 0)
        {
            ctx->sum = __chksum_array(ctx->sum, byte_buf + 1, size - 2);
            ctx->latched_byte = byte_buf[size - 1];
            ctx->latched = true;
        }
        else
        {
            ctx->sum = __chksum_array(ctx->sum, byte_buf + 1, size - 1);
            ctx->latched = false;
        }
    }
    else
    {
        if (size % 2 == 0)
        {
            ctx->sum = __chksum_array(ctx->sum, byte_buf, size);
            ctx->latched = false;
        }
        else
        {
            ctx->sum = __chksum_array(ctx->sum, byte_buf, size - 1);
            ctx->latched_byte = byte_buf[size - 1];
            ctx->latched = true;
        }
    }

    ctx->total_size += size;
}

checksum_t checksum_get_value(const checksum_context_t *ctx)
{
    uint32_t sum;

    if (ctx->total_size == 0)
        return 0;

    if (ctx->total_size % 2 != 0)
    {
        /* Add a padding zero */
        EXA_ASSERT(ctx->latched);
        sum = __chksum_pair(ctx->sum, ctx->latched_byte, 0);
    }
    else
        sum = ctx->sum;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    sum = ~sum;

    return (checksum_t)sum;
}

size_t checksum_get_size(const checksum_context_t *ctx)
{
    return ctx->total_size;
}
