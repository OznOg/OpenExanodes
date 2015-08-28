/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By downloading, copying, installing or
 * using the software you agree to this license. If you do not agree to this license, do not download, install,
 * copy or use the software.
 *
 * Intel License Agreement
 *
 * Copyright (c) 2000, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 * the following conditions are met:
 *
 * -Redistributions of source code must retain the above copyright notice, this list of conditions and the
 *  following disclaimer.
 *
 * -Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 *  following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * -The name of Intel Corporation may not be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PARAMETERS_H_
#define _PARAMETERS_H_

#include "log/include/log.h"

#define ISCSI_PARAM_KEY_LEN         64  /* maximum <key> size (bytes) */
#define ISCSI_PARAM_MAX_LEN       4096  /* maximum <value> size (bytes) */
#define ISCSI_PARAM_MAX_TEXT_LEN  4096  /* maximum data for text command (bytes) */

#define ISCSI_PARAM_STATUS_FAILED        -1

typedef enum {
#define ISCSI_PARAM_TYPE_FIRST ISCSI_PARAM_TYPE_DECLARATIVE
        ISCSI_PARAM_TYPE_DECLARATIVE   = 453,
        ISCSI_PARAM_TYPE_NUMERICAL    ,
        ISCSI_PARAM_TYPE_NUMERICAL_Z  , /* zero represents no limit */
        ISCSI_PARAM_TYPE_BINARY_OR    ,
        ISCSI_PARAM_TYPE_BINARY_AND   ,
        ISCSI_PARAM_TYPE_LIST
#define ISCSI_PARAM_TYPE_LAST ISCSI_PARAM_TYPE_LIST
} iscsi_parameter_type;

#define ISCSI_PARAM_TYPE_IS_VALID(type) \
    ((type) >= ISCSI_PARAM_TYPE_FIRST && (type) <= ISCSI_PARAM_TYPE_LAST)

/* FIXME Most of the fields have *nothing* to do here as they're only useful
         for the negociation. The fields are here only so that negociation
         functions can do their job with a single (iSCSI) parameter... while
         they should take the iSCSI parameter *and* the negociation-related
         information. */
typedef struct iscsi_parameter
{
    iscsi_parameter_type type;            /**< type of parameter */

    char key[ISCSI_PARAM_KEY_LEN];        /**< key */
    char valid[ISCSI_PARAM_MAX_LEN];      /**< comma-separated list of valid values */
    char dflt[ISCSI_PARAM_MAX_LEN];       /**< default value */
    char value[ISCSI_PARAM_MAX_LEN];      /**< effective value */

    char offer_rx[ISCSI_PARAM_MAX_LEN];   /**< outgoing offer */
    char offer_tx[ISCSI_PARAM_MAX_LEN];   /**< incoming offer */
    char answer_tx[ISCSI_PARAM_MAX_LEN];  /**< outgoing answer */
    char answer_rx[ISCSI_PARAM_MAX_LEN];  /**< incoming answer */
    char negotiated[ISCSI_PARAM_MAX_LEN]; /**< negotiated value */

    /* FIXME: seems to be a state machine progression for the negociation,
     * an enum would be more suitable
     */
    bool is_tx_offer;                        /**< sent offer */
    bool is_rx_offer;                        /**< received offer */
    bool is_tx_answer;                       /**< sent answer */
    bool is_rx_answer;                       /**< received answer */

    struct iscsi_parameter *next;         /**< next parameter in the list */
} iscsi_parameter_t;

/**
 * Check whether a value is allowed for a given parameter.
 *
 * @param[in] param  Parameter to check against
 * @param[in] value  Value to check
 *
 * @return true if the value is allowed, false otherwise
 */
bool param_value_allowed(const iscsi_parameter_t *param, const char *value);

/**
 * Add a parameter to a list
 *
 * @param[in,out]   head    the address to the head of the list (head can be
 *                          NULL to start a new list)
 * @param[in]       type    the type of parameter to add
 * @param[in]       key     the parameters' key
 * @param[in]       dflt    the parameter's default value
 * @param[in]       valid   a list of values we handle for the parameter
 *                          Used during negociation.
 *                          Checked to be only comprise of a combination of
 *                          yes and no for the following parameters types:
 *                          - ISCSI_PARAM_TYPE_BINARY_OR
 *                          - ISCSI_PARAM_TYPE_BINARY_AND
 *
 * @return 0 if successful, -1 otherwise.
 */
int param_list_add(iscsi_parameter_t **head,
                   iscsi_parameter_type type,
                   const char *key,
                   const char *dflt,
                   const char *valid);

void __param_list_free(iscsi_parameter_t *param_list);
#define param_list_free(param_list) \
    (__param_list_free((param_list)), (param_list) = NULL)

/**
 * Return the value for the parameter matching the key
 *
 * @param[in]   head    The list in which to look
 * @param[in]   key     The key to look for
 *
 * @return the parameter's value, or NULL if it's not found.
 */
const char *param_list_get_value(const iscsi_parameter_t *head, const char *key);

/**
 * Set the value of the parameter matching the key
 *
 * @param[in]   head    The list in which to look
 * @param[in]   key     The key to look for
 * @param[in]   value   The new value
 *
 * @return 0 if success or -1 if the parameter is not found.
 */
int param_list_set_value(iscsi_parameter_t *head, const char *key, const char *value);

/**
 * Return the parameter matching key
 *
 * @param[in]   param_list   The list in which to look
 * @param[in]   key          The key to look for
 *
 * @return the parameter, or NULL if it's not found.
 */
iscsi_parameter_t *param_list_elt(const iscsi_parameter_t *param_list,
                                  const char *key);

/**
 * Check whether a parameter exists in a list
 *
 * @param[in]   param_list   The list in which to look
 * @param[in]   key          The key to look for
 *
 * @return true if it exists, false if it does not
 */
bool param_exists(const iscsi_parameter_t *param_list, const char *key);

/**
 * Compare a parameter's value with a given value.
 *
 * @param[in] param_list  The list in which to look for the parameter
 * @param[in] key         The parameter's key
 * @param[in] val         The value to compare with.
 *
 * @return true if the value and key match a parameter in the list false
 * otherwise
 */
bool param_list_value_is_equal(const iscsi_parameter_t *param_list,
                               const char *key, const char *val);

#endif
