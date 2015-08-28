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

#include "target/iscsi/include/parameters.h"

#include "common/include/exa_conversion.h"

#include "os/include/os_mem.h"
#include "os/include/os_string.h"

#define BINARY_PARAM_VALID(param) \
    (os_strcasecmp((param), "yes,no") == 0 || os_strcasecmp((param), "no,yes") == 0 \
     || os_strcasecmp((param), "yes") == 0 || os_strcasecmp((param), "no") == 0)

/* The list is assumed to be comma-separated */
static bool __str_list_contains(const char *list, const char *value)
{
    char list2[strlen(list) + 1];
    char *ptr;
    char *item;

    os_strlcpy(list2, list, sizeof(list2));

    item = os_strtok(list2, ",", &ptr);
    while (item != NULL)
    {
        if (strcmp(item, value) == 0)
            return true;
        item = os_strtok(NULL, ",", &ptr);
    }

    return false;
}

static bool __value_allowed(iscsi_parameter_type type, const char *valid,
                            const char *value)
{
    if (value == NULL)
        return false;

    switch (type)
    {
    case ISCSI_PARAM_TYPE_DECLARATIVE:
        return true;

    case ISCSI_PARAM_TYPE_NUMERICAL:
    case ISCSI_PARAM_TYPE_NUMERICAL_Z:
        {
            uint64_t max, val;

            EXA_ASSERT(to_uint64(valid, &max) == 0);

            if (to_uint64(value, &val) < 0)
                return false;

            if (val > max)
            {
                if (type == ISCSI_PARAM_TYPE_NUMERICAL)
                    return false;
                if (max != 0) /* NUMERICAL_Z 0 means no limit */
                    return false;
            }

            return true;
        }

    case ISCSI_PARAM_TYPE_LIST:
        return __str_list_contains(valid, value);

    case ISCSI_PARAM_TYPE_BINARY_OR:
    case ISCSI_PARAM_TYPE_BINARY_AND:
        return BINARY_PARAM_VALID(value);
    }

    return false;
}

bool param_value_allowed(const iscsi_parameter_t *param, const char *value)
{
    return __value_allowed(param->type, param->valid, value);
}

static bool param_is_valid(iscsi_parameter_type type, const char *key,
                                const char *dflt, const char *accepted_values)
{
    bool result = false; /* Stupid compiler. */

    /* Check the accepted values */
    switch (type)
    {
    case ISCSI_PARAM_TYPE_DECLARATIVE:
        result = true;
        break;

    case ISCSI_PARAM_TYPE_NUMERICAL:
    case ISCSI_PARAM_TYPE_NUMERICAL_Z:
        {
            uint64_t max;
            result = to_uint64(accepted_values, &max) == 0;
        }
        break;

    case ISCSI_PARAM_TYPE_LIST:
        result = true;
        break;

    case ISCSI_PARAM_TYPE_BINARY_OR:
    case ISCSI_PARAM_TYPE_BINARY_AND:
        result = BINARY_PARAM_VALID(accepted_values);
        break;
    }

    if (!result)
        exalog_error("declaration of parameter %s: invalid accepted values '%s'",
                     key, accepted_values);

    /* Check the default against the accepted values */
    result = __value_allowed(type, accepted_values, dflt);
    if (!result)
        exalog_error("declaration of parameter %s: invalid default '%s'"
                     " (accepted values: %s)", key, dflt, accepted_values);

    return result;
}

int param_list_add(iscsi_parameter_t **head,
                   iscsi_parameter_type type,
                   const char *key,
                   const char *dflt,
                   const char *valid)
{
    iscsi_parameter_t *param;

    /* Arg check */
    EXA_ASSERT_VERBOSE(ISCSI_PARAM_TYPE_IS_VALID(type),
                       "Unknown type iscsi parameter type %d", type);

    if (!param_is_valid(type, key, dflt, valid))
        return -1;

    /* Check that the parameter doesn't already exists */
    if (*head != NULL && param_exists(*head, key))
    {
        exalog_error("Parameter %s already exists in the list of parameters", key);
        return -1;
    }

    param = os_malloc(sizeof(iscsi_parameter_t));
    if (param == NULL)
    {
        exalog_error("Failed to allocate new parameter");
        return -1;
    }

    param->type = type;
    strcpy(param->key, key);
    strcpy(param->dflt, dflt);
    strcpy(param->value, dflt);
    strcpy(param->valid, valid);
    param->is_tx_offer = false;
    param->is_rx_offer = false;
    param->is_tx_answer = false;
    param->is_rx_answer = false;

    exalog_trace("\"%s\": valid \"%s\", default \"%s\", current \"%s\"",
                param->key, param->valid, param->dflt, param->value);

    /* Add the new parameter to the list */
    param->next = *head;
    *head = param;

    return 0;
}

void __param_list_free(iscsi_parameter_t *param_list)
{
    iscsi_parameter_t *param;

    param = param_list;
    while (param != NULL)
    {
        iscsi_parameter_t *tmp;

        tmp = param;
        param = tmp->next;

        os_free(tmp);
    }
}

static iscsi_parameter_t *param_list_get(const iscsi_parameter_t *param_list, const char *key)
{
    const iscsi_parameter_t *param;

    for (param = param_list; param != NULL; param = param->next)
    {
        if (!strcmp(param->key, key))
            return (iscsi_parameter_t *)param;
    }
    return NULL;
}

bool param_exists(const iscsi_parameter_t *param_list, const char *key)
{
    return param_list_get(param_list, key) != NULL;
}

iscsi_parameter_t *param_list_elt(const iscsi_parameter_t *param_list, const char *key)
{
    iscsi_parameter_t *param = param_list_get(param_list, key);

    if (param == NULL)
        exalog_error("key \"%s\" not found in param list", key);

    return param;
}

const char *param_list_get_value(const iscsi_parameter_t *head, const char *key)
{
    const iscsi_parameter_t *ptr = param_list_elt(head, key);

    return ptr ? ptr->value : NULL;
}

/* FIXME Shouldn't this check the type of the parameter and allow/deny its
         setting depending on whether the type of value matches? */
int param_list_set_value(iscsi_parameter_t *head, const char *key, const char *value)
{
    iscsi_parameter_t *ptr = param_list_elt(head, key);

    if (ptr == NULL)
        return -1;

    os_strlcpy(ptr->value, value, sizeof(ptr->value));
    return 0;
}

bool param_list_value_is_equal(const iscsi_parameter_t *param_list,
                               const char *key, const char *val)
{
    const iscsi_parameter_t *param = param_list_elt(param_list, key);

    return param != NULL && strcmp(param->value, val) == 0;
}
