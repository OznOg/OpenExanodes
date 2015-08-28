/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "token_manager/tm_server/src/tm_err.h"

#include <stdlib.h>

const char *tm_err_str(tm_err_t err)
{
    if (!TM_ERR_IS_VALID(err))
        return NULL;

    switch (err)
    {
    case TM_ERR_NONE: return "(none)";
    case TM_ERR_TOO_MANY_TOKENS: return "Tokens number limit has been reached";
    case TM_ERR_ANOTHER_HOLDER: return "This token is held by another node";
    case TM_ERR_NOT_HOLDER: return "Client doesn't hold the token";
    case TM_ERR_NO_SUCH_TOKEN: return "Token doesn't exist";
    case TM_ERR_WRONG_FILE_MAGIC: return "Wrong file magic";
    case TM_ERR_WRONG_FILE_TOKENS_NUMBER: return "Wrong number of tokens in file";
    case TM_ERR_WRONG_FILE_FORMAT_VERSION: return "Wrong file format version";
    }

    return NULL;
}
