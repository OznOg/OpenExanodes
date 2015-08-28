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

#include "target/iscsi/include/iscsi_negociation.h"

#include "common/include/exa_conversion.h"
#include "os/include/os_mem.h"
#include "os/include/os_string.h"
#include <stdio.h>

static int param_text_print(const char *text, unsigned text_len)
{
    char key[256];
    const char *ptr, *delim_ptr, *value;

    for (ptr = text; ptr - text < text_len; ptr += strlen(ptr) + 1)
    {
        /* Skip over any NULLs */
        while (!(*ptr) && ptr - text < text_len)
            ptr++;

        if (ptr - text >= text_len)
            break;

        if ((delim_ptr = strchr(ptr, '=')) == NULL)
        {
            exalog_error("delimiter \'=\' not found in token \"%s\"", ptr);
            return -1;
        }

        strncpy(key, ptr, delim_ptr - ptr);
        key[delim_ptr - ptr] = '\0';
        value = delim_ptr + 1;

        exalog_trace("\"%s\"=\"%s\"", key, value);
    }

    return 0;
}

/* XXX What is it doing here? Why not in parameters.c? */
int param_text_add(const char *key,
                   const char *value,
                   char *text,
                   int *len,
                   int offer)
{
    if (*len + strlen(key) + 1 + strlen(value) + 1 > ISCSI_PARAM_MAX_TEXT_LEN)
    {
        exalog_error("error adding key \"%s\" -- maximum text data size is %u",
                     key,
                     ISCSI_PARAM_MAX_TEXT_LEN);
        return -1;
    }

    sprintf(text + *len, "%s=%s", key, value);
    *len += strlen(text + *len) + 1;

    return 0;
}

/* old comment: Negotiate after receiving|sending an answer */
static int __param_negociation_update(iscsi_parameter_t *param, bool outgoing)
{
    char val1[ISCSI_PARAM_MAX_LEN];
    char val2[ISCSI_PARAM_MAX_LEN];

    exalog_trace(
        "negotiating %s (type %d, offer_tx \"%s\" offer_rx \"%s\" answer_tx \"%s\" answer_rx \"%s\")",
        param->key,
        param->type,
        param->offer_tx,
        param->offer_rx,
        param->answer_tx,
        param->answer_rx);

    EXA_ASSERT_VERBOSE(ISCSI_PARAM_TYPE_IS_VALID(param->type),
                       "Unknown iSCSI parameter type %d", param->type);

    switch (param->type)
    {
    case ISCSI_PARAM_TYPE_DECLARATIVE:
        if (outgoing)
            strcpy(param->negotiated, param->offer_tx);
        else
        {
            if (param->is_rx_offer)
                strcpy(param->negotiated, param->offer_rx);
            else
                strcpy(param->negotiated, param->answer_rx);
        }
        break;

    case ISCSI_PARAM_TYPE_BINARY_AND:
    case ISCSI_PARAM_TYPE_BINARY_OR:
        if (outgoing)
        {
            strcpy(val1, param->offer_rx);
            strcpy(val2, param->answer_tx);
        }
        else
        {
            strcpy(val1, param->answer_rx);
            strcpy(val2, param->offer_tx);
            /* Make sure the answer is valid */
            if (strcmp(val1, "Yes") && strcmp(val1, "No")
                && strcmp(val1, "yes") && strcmp(val1, "no")
                && strcmp(val1, "Irrelevant"))
            {
                /* Invalid value returned as answer. */
                exalog_error("Invalid answer (%s) for key (%s)", val1, param->key);
                return 0;

                /* FIXME: error swallowing coming from original windows proto
                 * This code "used to" return -1.
                 * Don't know if the modification was done by Olivier or if it comes from Intel
                 */
            }
        }
        if (param->type == ISCSI_PARAM_TYPE_BINARY_OR)
        {
            if (!strcmp(val1, "yes") || !strcmp(val2, "yes")
                || !strcmp(val1, "Yes") || !strcmp(val2, "Yes"))
                strcpy(param->negotiated, "Yes");
            else
                strcpy(param->negotiated, "No");
        }
        else
        {
            if ((!strcmp(val1, "yes") || !strcmp(val1, "Yes"))
                && (!strcmp(val2, "yes") || !strcmp(val2, "Yes")))
                strcpy(param->negotiated, "Yes");
            else
                strcpy(param->negotiated, "No");
        }
        break;

    case ISCSI_PARAM_TYPE_NUMERICAL_Z:
    case ISCSI_PARAM_TYPE_NUMERICAL:
        {
            int val1_i, val2_i, negotiated_i;

            if (outgoing)
            {
                strcpy(val1, param->offer_rx);
                strcpy(val2, param->answer_tx);
            }
            else
            {
                strcpy(val1, param->answer_rx);
                strcpy(val2, param->offer_tx);
            }
            if (to_int(val1, &val1_i) != 0)
            {
                val1_i = 0;
                exalog_error("Invalid val1 %s (%s)", val1,
                             outgoing ? "outgoing" : "incoming");
            }
            if (to_int(val2, &val2_i) != 0)
            {
                val2_i = 0;
                exalog_error("Invalid val2 %s (%s)", val2,
                             outgoing ? "outgoing" : "incoming");
            }
            if (param->type == ISCSI_PARAM_TYPE_NUMERICAL_Z)
            {
                if (val1_i == 0)
                    negotiated_i = val2_i;
                else if (val2_i == 0)
                    negotiated_i = val1_i;
                else if (val1_i > val2_i)
                    negotiated_i = val2_i;
                else
                    negotiated_i = val1_i;
            }
            else
            {
                if (val1_i > val2_i)
                    negotiated_i = val2_i;
                else
                    negotiated_i = val1_i;
            }
            sprintf(param->negotiated, "%d", negotiated_i);
        }
        break;

    case ISCSI_PARAM_TYPE_LIST:
        if (outgoing)
        {
            if (param->is_tx_offer)
            {
                exalog_error("we should not be here");      /* error - we're sending an offer */
                return -1;
            }
            else if (param->is_tx_answer)
                strcpy(val1, param->answer_tx);     /* we're sending an answer */
            else
            {
                exalog_error("unexpected error");
                return -1;
            }
        }
        else
        {
            if (param->is_rx_offer)
            {
                exalog_error("we should not be here");      /* error - we received an offer */
                return -1;
            }
            else if (param->is_rx_answer)
                strcpy(val1, param->answer_rx);     /* we received an answer */
            else
            {
                exalog_error("unexpected error");
                return -1;
            }
        }

        /* Make sure incoming or outgoing answer is valid */
        /* None, Reject, Irrelevant and NotUnderstood are valid */
        if (!strcmp(val1, "None") || !strcmp(val1, "Reject")
            || !strcmp(val1, "Irrelevant") || !strcmp(val1, "NotUnderstood"))
        {
            strcpy(param->negotiated, val1);
            break;
        }

        if (param_value_allowed(param, val1))
            strcpy(param->negotiated, val1);
        else
        {
            exalog_error("invalid value '%s' for parameter %s", val1, param->key);
            return -1;
        }
        break;
    }

    exalog_trace("negotiated \"%s\"=\"%s\"", param->key, param->negotiated);

    /* For inquiries, we don't commit the value. */

    if (param->is_tx_offer && !strcmp(param->offer_tx, "?"))
    {
        /* we're offering an inquiry */
        exalog_trace("sending an inquiry for \"%s\"", param->key);
        return 1;
    }
    else if (param->is_rx_offer && !strcmp(param->offer_rx, "?"))
    {
        /* we're receiving an inquiry */
        exalog_trace("received an inquiry for \"%s\"", param->key);
        return 1;
    }
    else if (param->is_tx_answer && !strcmp(param->offer_rx, "?"))
    {
        /* we're answering an inquiry */
        exalog_trace("answering an inquiry for \"%s\"", param->key);
        return 1;
    }
    else if (param->is_rx_answer && !strcmp(param->offer_tx, "?"))
    {
        /* we're receiving an answer for our inquiry */
        exalog_trace("received an answer for inquiry on \"%s\"",
                     param->key);
        return 1;
    }

    exalog_trace("automatically committing \"%s\"=\"%s\"",
                 param->key,
                 param->negotiated);

    strcpy(param->value, param->negotiated);

    return 1;
}

static int __prepare_answer_text(iscsi_parameter_t *param, const char *value,
                                 char *text_out, int *text_len_out)
{
    const char *p1, *p2, *p3, *p4;
    int offer_i, answer_i, max_i;

    /* Answer with current value if this is an inquiry (<key>=?) */
    if (!strcmp(value, "?"))
    {
        exalog_trace("got inquiry for param \"%s\"", param->key);
        strcpy(param->answer_tx, param->value);
        goto add_answer;
    }

    /* Generate answer according to the parameter type */
    EXA_ASSERT_VERBOSE(ISCSI_PARAM_TYPE_IS_VALID(param->type),
                       "Unknown type iscsi parameter type %d", param->type);

    switch (param->type)
    {
    case ISCSI_PARAM_TYPE_BINARY_AND:
    case ISCSI_PARAM_TYPE_BINARY_OR:
        if (strcmp(value, "yes") && strcmp(value, "no") &&
            strcmp(value, "Yes") && strcmp(value, "No"))
        {
            exalog_error("\"%s\" is not a valid binary value", value);
            strcpy(param->answer_tx, "Reject");
            goto add_answer;
        }

        if (strchr(param->valid, ',') != NULL)
            strcpy(param->answer_tx, value); /* we accept both yes and no, so answer w/ their offer */
        else
            strcpy(param->answer_tx, param->valid); /* answer with the only value we support */

        break;

    case ISCSI_PARAM_TYPE_LIST:
        /* Find the first valid offer that we support */
        for (p1 = p2 = param->offer_rx; p2; p1 = p2 + 1)
        {
            char offer[ISCSI_PARAM_MAX_LEN];

            if ((p2 = strchr(p1, ',')))
            {
                strncpy(offer, p1, p2 - p1);
                offer[p2 - p1] = '\0';
            }
            else
                strcpy(offer, p1);

            /* FIXME: this block must be turned into a validation subfunction */
            if (strlen(param->valid))
            {
                for (p3 = p4 = param->valid; p4; p3 = p4 + 1)
                {
                    char valid[ISCSI_PARAM_MAX_LEN];

                    if ((p4 = strchr(p3, ',')))
                    {
                        strncpy(valid, p3, p4 - p3);
                        valid[p4 - p3] = '\0';
                    }
                    else
                        strcpy(valid, p3);

                    if (!strcmp(valid, offer))
                    {
                        strcpy(param->answer_tx, offer);
                        goto add_answer;
                    }
                }
            }
            else
            {
                exalog_trace(
                    "Valid list empty. Answering with first in offer list");
                strcpy(param->answer_tx, offer);
                goto add_answer;
            }
            exalog_trace(
                "\"%s\" is not a valid offer for key \"%s\" (must choose from \"%s\")",
                offer,
                param->key,
                param->valid);
        }
        exalog_trace(
            "No Valid offers: \"%s\" is added as value for key \"%s\")",
            "Reject",
            param->key);
        strcpy(param->answer_tx, "Reject");
        break;

    case ISCSI_PARAM_TYPE_NUMERICAL_Z:
    case ISCSI_PARAM_TYPE_NUMERICAL:
        if (to_int(param->offer_rx, &offer_i) != 0)
        {
            offer_i = 0;
            exalog_error("Invalid offer_rx %s in param %s", param->offer_rx, param->key);
        }
        if (to_int(param->valid, &max_i) != 0)
        {
            max_i = 0;
            exalog_error("Invalid valid %s in param %s", param->valid, param->key);
        }
        if (param->type == ISCSI_PARAM_TYPE_NUMERICAL_Z)
        {
            if (max_i == 0)
                answer_i = offer_i; /* we support anything, so return whatever they offered */
            else if (offer_i == 0)
                answer_i = max_i;   /* return only what we can support */
            else if (offer_i > max_i)
                answer_i = max_i;   /* we are the lower of the two */
            else
                answer_i = offer_i; /* they are the lower of the two */
        }
        else
        {
            if (offer_i > max_i)
                answer_i = max_i;   /* we are the lower of the two */
            else
                answer_i = offer_i; /* they are the lower of the two */
        }
        sprintf(param->answer_tx, "%d", answer_i);
        goto add_answer;

    case ISCSI_PARAM_TYPE_DECLARATIVE:
        return 1;
    }

add_answer:

    if (param_text_add(param->key, param->answer_tx, text_out, text_len_out, 0) != 0)
        return -1;

    exalog_trace("answering \"%s\"=\"%s\"", param->key, param->answer_tx);
    return 1;
}

/**
 * Treatment loop body of param_text_parse()
 *
 * @return 1 if the loop must continue (-> former 'continue' or 'goto next')
 *         0 if the loop has successfully complete (-> former error swallowing 'return 0')
 *         or a negative error code (mainly -1)
 */
static int __treat_incoming_text(iscsi_parameter_t *head,
                                 const char *key,
                                 const char *value,
                                 char *text_out,
                                 int *text_len_out,
                                 bool outgoing)
{
    iscsi_parameter_t *param;

    /* Find key in param list */
    for (param = head; param != NULL; param = param->next)
        if (strcmp(param->key, key) == 0)
            break;

    if (param == NULL)
    {
        if (!outgoing)
        {
            if (param_text_add(key, "NotUnderstood", text_out,
                               text_len_out, 0) != 0)
                return -1;
        }
        else
            exalog_trace("ignoring \"%s\"", key);

        return 1;
    }

    if (strlen(value) > ISCSI_PARAM_MAX_LEN)
    {
        exalog_error("iSCSI parameter value too long: %" PRIzu ".", strlen(value));
        return -1;
    }

    /* FIXME: It really looks like we need some enum */
    /* sending an answer to the initiator offer */
    if (outgoing && param->is_rx_offer)
    {
        param->is_tx_answer = true;   /* sending an answer */
        strcpy(param->answer_tx, value);
        exalog_trace("sending answer \"%s\"=\"%s\" for offer \"%s\"",
                     param->key,
                     param->answer_tx,
                     param->offer_rx);

        return __param_negociation_update(param, outgoing);
    }
    /* sending an offer to the initiator */
    else if (outgoing)
    {
        param->is_tx_offer = true;    /* sending an offer */
        param->is_rx_offer = false;    /* reset */
        strcpy(param->offer_tx, value);
        exalog_trace("sending offer \"%s\"=\"%s\"",
                     param->key,
                     param->offer_tx);
        if (param->type == ISCSI_PARAM_TYPE_DECLARATIVE)
            return __param_negociation_update(param, outgoing);
        else
            return 1;
    }
    /* receiving an answer from the initiator (corresponding to a target offer) */
    else if (param->is_tx_offer)
    {
        param->is_rx_answer = true;   /* received an answer */
        param->is_tx_offer = false;    /* reset */
        strcpy(param->answer_rx, value);
        exalog_trace("received answer \"%s\"=\"%s\" for offer \"%s\"",
                     param->key,
                     param->answer_rx,
                     param->offer_tx);

        return __param_negociation_update(param, outgoing);
    }
    /* receiving an offer from the initiator */
    else
    {
        param->is_rx_offer = true;    /* received an offer */
        strcpy(param->offer_rx, value);
        exalog_trace("received offer \"%s\"=\"%s\"", param->key, param->offer_rx);

        /* Answer the offer if it is an inquiry or the type is not DECLARATIVE */
        if (strcmp(param->offer_rx, "?") == 0
            || param->type != ISCSI_PARAM_TYPE_DECLARATIVE)
            return __prepare_answer_text(param, value, text_out, text_len_out);

        return __param_negociation_update(param, outgoing);
    }

    /* not possible to reach this point */
    EXA_ASSERT(false);

    return -1;
}

int param_text_parse(iscsi_parameter_t *head,
                     const char *text_in,
                     int text_len_in,
                     char *text_out,
                     int *text_len_out,
                     bool outgoing)
{
    const char *ptr, *delim_ptr;

    /* Whether incoming or outgoing, some of the params might be offers and some answers.  Incoming */
    /* text has the potential for creating outgoing text - and this will happen when the incoming */
    /* text has offers that need an answer. */

    exalog_trace("=== BEGIN %s ===", outgoing ? "OUTGOING" : "INCOMING");
    param_text_print(text_in, text_len_in);
    exalog_trace("=== END %s ===", outgoing ? "OUTGOING" : "INCOMING");

    if (!outgoing)
        *text_len_out = 0;

    exalog_trace("**************************************************");
    exalog_trace("*              PARAMETERS NEGOTIATED             *");
    exalog_trace("*                                                *");

    for (ptr = text_in; ptr - text_in < text_len_in; ptr += strlen(ptr) + 1)
    {
        char key[ISCSI_PARAM_KEY_LEN];
        const char *value = NULL;
        int ret;

        /* Skip over any NULLs */
        while (!(*ptr) && ptr - text_in < text_len_in)
             ptr++;

        if (ptr - text_in >= text_len_in)
             break;

        /* Extract <key>=<value> token from text_in */
        if ((delim_ptr = strchr(ptr, '=')) == NULL)
            exalog_error("delimiter \'=\' not found in token \"%s\"", ptr);
        else
        {
            if (delim_ptr - ptr >= ISCSI_PARAM_KEY_LEN - 1)
            {
                if (!outgoing)
                {
                    char *tmp_key;

                    /* FIXME I a not sure it is a good idea to use a string
                     * bigger than ISCSI_PARAM_KEY_LEN even for an error answer
                     */
                    /* FIXME Missing +1 for terminal '\0'... or do we *not*
                             want the terminal null?? */
                    tmp_key = os_malloc(delim_ptr - ptr);
                    /* FIXME What if the allocation fails? */
                    if (tmp_key != NULL)
                    {
                        strncpy(tmp_key, ptr, delim_ptr - ptr);
                        tmp_key[delim_ptr - ptr] = '\0';
                        if (param_text_add(tmp_key, "NotUnderstood", text_out,
                                           text_len_out, 0) != 0)
                        {
                            os_free(tmp_key);
                            return -1;
                        }
                    }

                    os_free(tmp_key);
                }
                else
                    exalog_trace("ignoring \"%s\"", key);

                continue;
            }
            strncpy(key, ptr, delim_ptr - ptr);
            key[delim_ptr - ptr] = '\0';
            value = delim_ptr + 1;
        }

        ret = __treat_incoming_text(head, key, value,
                                    text_out, text_len_out,
                                    outgoing);
        if (ret <= 0)
            return ret;
    }

    exalog_trace("**************************************************");

    return 0;
}
