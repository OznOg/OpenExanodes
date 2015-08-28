/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/assembly/include/extent.h"
#include "os/include/os_error.h"
#include "os/include/os_mem.h"
#include "common/include/exa_assert.h"

uint64_t extent_list_get_num_values(const extent_t *list)
{
    const extent_t *cur;
    uint64_t count = 0;

    for (cur = list; cur != NULL; cur = cur->next)
        count += cur->end - cur->start + 1;

    return count;
}

uint32_t extent_list_count(const extent_t *extent_list)
{
    uint32_t i = 0;
    while (extent_list != NULL)
    {
        i++;
        extent_list = extent_list->next;
    }
    return i;
}

static extent_t *extent_new(uint64_t start, uint64_t end)
{
    extent_t *cur_extent;

    cur_extent = os_malloc(sizeof(extent_t));
    EXA_ASSERT (cur_extent != NULL);

    cur_extent->next = NULL;
    cur_extent->start = start;
    cur_extent->end = end;
    return cur_extent;
}

extent_t *extent_list_add_value(extent_t *extent_list, uint64_t value)
{
    extent_t *cur;
    extent_t *prev = NULL;
    extent_t *next = NULL;
    bool inserted = false;

    /* If the list is NULL, we'll create one */
    if (extent_list == NULL)
        return extent_new(value, value);

    /* Is the value before the first extent? */
    if (extent_list->start > 0 && value < extent_list->start - 1)
    {
        extent_t *new = extent_new(value, value);
        new->next = extent_list;
        return new;
    }

    for (cur = extent_list; cur; cur = cur->next)
    {
        next = cur->next;
        if (value >= cur->start && value <= cur->end)
        {
            /* The value is already in one of the extents */
            return extent_list;
        }
        else if (value == cur->start - 1)
        {
            /* The value is just before one of the extents, let's expand it */
            cur->start--;
            inserted = true;
            break;
        }
        else if (value == cur->end + 1)
        {
            /* The value is just after one of the extents, let's expand it */
            cur->end++;
            inserted = true;
            break;
        }
        else if (value > cur->end)
        {
            /* Can this value fit between this extent and the next one,
             * without having to expand the next one? */
            if (next && next->start > value + 1)
            {
                extent_t *new = extent_new(value, value);
                cur->next = new;
                new->next = next;

                return extent_list;
            }
        }
        prev = cur;
    }

    if (!inserted)
    {
        /* We didn't find an extent to extend, so append a new one. */
        extent_t *new = extent_new(value, value);
        EXA_ASSERT(prev != NULL);
        prev->next = new;
    }
    else
    {
        /* Can we merge the previous extent? */
        if (prev != NULL && prev->end >= cur->start - 1)
        {
            prev->end = cur->end;
            prev->next = cur->next;
            os_free(cur);
        }
        /* Can we merge the next extent? */
        else if (next != NULL && next->start - 1 <= cur->end)
        {
            cur->end = next->end;
            cur->next = next->next;
            os_free(next);
        }
    }

    return extent_list;
}

extent_t *extent_list_remove_value(extent_t *extent_list, uint64_t value)
{
    extent_t *cur;
    extent_t *prev = NULL;
    extent_t *next = NULL;

    for (cur = extent_list; cur; cur = cur->next)
    {
        next = cur->next;

        if (value == cur->start || value == cur->end)
        {
            /* Shrink the extent by one */
            if (value == cur->start)
                cur->start++;
            else
                cur->end--;

            /* If the extent is now empty, remove it */
            if (cur->start > cur->end)
            {
                if (prev == NULL)
                {
                    os_free(cur);
                    return next;
                }
                else
                {
                    prev->next = next;
                    os_free(cur);
                    return extent_list;
                }
            }
        }
        else if (value > cur->start && value < cur->end)
        {
            /* Separate the extent into two */
            extent_t *new = extent_new(value + 1, cur->end);

            cur->end = value - 1;
            /* and plug it in the list */
            cur->next = new;
            new->next = next;
            return extent_list;
        }
        prev = cur;
    }

    /* We found nothing to do, the value wasn't in the list */
    return extent_list;
}

void __extent_list_free(extent_t *extent_list)
{
    while (extent_list)
    {
        extent_t *next = extent_list->next;
        os_free(extent_list);
        extent_list = next;
    }
}

static int extent_serialize(const extent_t *extent, stream_t *stream)
{
    flat_extent_t fe;
    int w;

    fe.start = extent->start;
    fe.end = extent->end;

    w = stream_write(stream, &fe, sizeof(fe));
    if (w < 0)
        return w;
    else if (w != sizeof(fe))
        return -EIO;

    return 0;
}

static int extent_deserialize(extent_t **extent, stream_t *stream)
{
    flat_extent_t fe;
    int r;

    r = stream_read(stream, &fe, sizeof(fe));
    if (r < 0)
        return r;
    else if (r != sizeof(fe))
        return -EIO;

    *extent = os_malloc(sizeof(extent_t));
    if (*extent == NULL)
        return -ENOMEM;

    (*extent)->start = fe.start;
    (*extent)->end = fe.end;
    (*extent)->next = NULL;

    return 0;
}

int extent_list_serialize(const extent_t *extent_list, stream_t *stream)
{
    const extent_t *extent;
    uint32_t n;
    int w;

    n = extent_list_count(extent_list);

    w = stream_write(stream, &n, sizeof(n));
    if (w < 0)
        return w;
    else if (w != sizeof(n))
        return -EIO;

    for (extent = extent_list; extent != NULL; extent = extent->next)
    {
        int err = extent_serialize(extent, stream);
        if (err != 0)
            return err;
    }

    return 0;
}

int extent_list_deserialize(extent_t **extent_list, stream_t *stream)
{
    extent_t *extent_last;
    uint32_t n, i;
    int r, err = 0;

    *extent_list = extent_last = NULL;

    r = stream_read(stream, &n, sizeof(n));
    if (r < 0)
        return r;
    else if (r != sizeof(n))
        return -EIO;

    for (i = 0; i < n; i++)
    {
        extent_t *extent;

        err = extent_deserialize(&extent, stream);
        if (err != 0)
            goto done;

        if (*extent_list == NULL)
            *extent_list = extent;
        else if (extent_last != NULL)
            extent_last->next = extent;

        extent_last = extent;
    }

done:
    if (err != 0)
        extent_list_free(*extent_list);

    return err;
}

bool extent_list_contains_value(const extent_t *extent_list, uint64_t value)
{
    const extent_t *cur;

    for (cur = extent_list; cur != NULL; cur = cur->next)
        if (value >= cur->start && value <= cur->end)
            return true;

    return false;
}
