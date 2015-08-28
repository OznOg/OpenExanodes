/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "rdev/include/exa_rdev.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_error.h"

#include <stdlib.h>
#include <errno.h> /* For ENOTTY */

int exa_rdev_make_request(rdev_op_t op, unsigned long long start, int size,
			  void *buffer, exa_rdev_handle_t *handle)
{
    int ret = 0;
    int err = EXA_SUCCESS;
    void *nbd_private = NULL;

    EXA_ASSERT(RDEV_OP_VALID(op));

    /* FIXME: It seems that this stuff check the alignement on 512 bytes.
     *        A macro would be welcome.
     */
    if ((start & 511) != 0)
	return -RDEV_ERR_INVALID_OFFSET;

    if ((size & 511) != 0)
	return -RDEV_ERR_INVALID_SIZE;

    while (size > 0 && err == 0)
    {
        int size_for_req = size > EXA_RDEV_READ_WRITE_FRAGMENT ?
                           EXA_RDEV_READ_WRITE_FRAGMENT : size;
	nbd_private = NULL;
        /* Be carefull the 'nbd_private' pointer can be modified */
	ret = exa_rdev_make_request_new(op, &nbd_private, BYTES_TO_SECTORS(start),
					BYTES_TO_SECTORS(size_for_req), buffer, handle);
	switch (ret)
	{
	case RDEV_REQUEST_NONE_ENDED:
	case RDEV_REQUEST_ALL_ENDED:
	case RDEV_REQUEST_END_OK:
	    buffer = (char *)buffer + size_for_req;
	    start += size_for_req;
	    size  -= size_for_req;
	    break;

	case RDEV_REQUEST_END_ERROR:
	    err = ret;
	    break;

	case RDEV_REQUEST_NOT_ENOUGH_FREE_REQ:
	    ret = exa_rdev_wait_one_request(&nbd_private, handle);
	    if (ret == RDEV_REQUEST_END_ERROR)
		err = ret;
	    break;

	default:
	    /* unknown error so abort */
	    err = ret;
	    break;
	}
    }

    if (err == -RDEV_ERR_NOT_OPEN || err == -ENOTTY)
	return err;

    while (ret != RDEV_REQUEST_ALL_ENDED)
    {
	ret = exa_rdev_wait_one_request(&nbd_private, handle);
	if (ret == RDEV_REQUEST_END_ERROR || ret < 0)
	    err = ret;
    }

    return err;
}

/* FIXME The state should be an out parameter (and the value returned would
 * then be only 0 or negative error code) */
int exa_rdev_test(exa_rdev_handle_t *handle, void *buffer, int size)
{
    int last_error = exa_rdev_get_last_error(handle);

    /* If a request has recently been carried out on this device, then
     * return status of that request
     */
    switch (last_error)
    {
    case RDEV_REQUEST_END_OK:
        return EXA_RDEV_STATUS_OK;

    case RDEV_REQUEST_END_ERROR:
        return EXA_RDEV_STATUS_FAIL;

    case -RDEV_ERR_UNKNOWN:
        /* make one synchronous request to see status of device */
        if (exa_rdev_make_request(RDEV_OP_READ, 0, size, buffer, handle) == EXA_SUCCESS)
            return EXA_RDEV_STATUS_OK;
        else
            return EXA_RDEV_STATUS_FAIL;

    default:
        return last_error;
    }
}

