/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef TM_ERR_H
#define TM_ERR_H

typedef enum
{
#define TM_ERR__FIRST   TM_ERR_NONE
    TM_ERR_NONE = 0,
    TM_ERR_TOO_MANY_TOKENS,
    TM_ERR_ANOTHER_HOLDER,
    TM_ERR_NOT_HOLDER,
    TM_ERR_NO_SUCH_TOKEN,
    TM_ERR_WRONG_FILE_MAGIC,
    TM_ERR_WRONG_FILE_TOKENS_NUMBER,
    TM_ERR_WRONG_FILE_FORMAT_VERSION
#define TM_ERR__LAST    TM_ERR_WRONG_FILE_FORMAT_VERSION
} tm_err_t;

#define TM_ERR_IS_VALID(err)                                                  \
    ((err) >= TM_ERR__FIRST && (err) <= TM_ERR__LAST)

/**
 * Get the string representation of an error.
 *
 * @param[in] err  Positive error code.
 *
 * @return error string or NULL if the error is unknown
 */
const char *tm_err_str(tm_err_t err);

#endif /* TM_ERR_H */
