/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "target/iscsi/include/iscsi_negociation.h"
#include "target/iscsi/include/parameters.h"

static bool help_text_is_equal(const char *text_1, const char *text_2,
                               unsigned int nb_items)
{
    unsigned int i;
    const char *curr_1 = text_1;
    const char *curr_2 = text_2;

    for (i = 0; i < nb_items; i++)
    {
        unsigned int current_size = strlen(curr_1) + 1;

        if (strcmp(curr_1, curr_2) != 0)
            return false;

        curr_1 += current_size;
        curr_2 += current_size;
    }

    return true;
}

static void help_text_print(const char *text, unsigned int nb_items)
{
    unsigned int i;
    const char *curr = text;

    for (i = 0; i < nb_items; i++)
    {
        unsigned int current_size = strlen(curr) + 1;

        ut_printf("  item %d: '%s'", i, curr);

        curr += current_size;
    }
}

UT_SECTION(TREAT_INCOMING_TEXT)

ut_test(prepare_response_on_answer_declarative)
{
    iscsi_parameter_t *list = NULL;
    int r = 0;
    char text_out[ISCSI_PARAM_MAX_TEXT_LEN];
    char text_in[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=?\0key2=?\0key3=?\0key4=?\0";
    char text_exp[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=val1\0key2=val2\0key3=val3\0key4=val4\0";
    int text_len_out;

    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "val1", "");
    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key2", "val2", "");
    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key3", "val3", "");
    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key4", "val4", "");
    UT_ASSERT(list != NULL);

    ut_printf("Text in is:");
    help_text_print(text_in, 4);

    r = param_text_parse(list, text_in, sizeof(text_in),
                         text_out, &text_len_out,
                         false);

    ut_printf("Text out is:");
    help_text_print(text_out, 4);

    UT_ASSERT(r == 0);
    UT_ASSERT(help_text_is_equal(text_exp, text_out, 4));

    param_list_free(list);
}

ut_test(prepare_response_on_answer_numerical)
{
    iscsi_parameter_t *list = NULL;
    int r = 0;
    char text_out[ISCSI_PARAM_MAX_TEXT_LEN];
    char text_in[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=?\0key2=?\0key3=?\0key4=?\0";
    char text_exp[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=1\0key2=2\0key3=3\0key4=4\0";
    int text_len_out;

    param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "key1", "1", "10");
    param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "key2", "2", "10");
    param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "key3", "3", "10");
    param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "key4", "4", "4");
    UT_ASSERT(list != NULL);

    ut_printf("Text in is:");
    help_text_print(text_in, 4);

    r = param_text_parse(list, text_in, sizeof(text_in),
                         text_out, &text_len_out,
                         false);

    ut_printf("Text out is:");
    help_text_print(text_out, 4);

    UT_ASSERT(r == 0);
    UT_ASSERT(help_text_is_equal(text_exp, text_out, 4));

    param_list_free(list);
}

ut_test(prepare_response_on_proposal)
{
    iscsi_parameter_t *list = NULL;
    int r = 0;
    char text_in[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=val2\0key2=val2\0";
    char text_exp[ISCSI_PARAM_MAX_TEXT_LEN] = "key1=val2\0key2=val1\0";
    char text_out[ISCSI_PARAM_MAX_TEXT_LEN];
    int text_len_out;

    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "val1", "val1,val2");
    param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key2", "val1", "val1");
    UT_ASSERT(list != NULL);

    ut_printf("Text in is:");
    help_text_print(text_in, 4);

    r = param_text_parse(list, text_in, sizeof(text_in),
                         text_out, &text_len_out,
                         false);

    ut_printf("Text out is:");
    help_text_print(text_out, 4);

    UT_ASSERT(r == 0);
    ut_printf("THIS TEST CHECKS THAT THE CURRENT BEHAVIOUR IS WRONG:");
    UT_ASSERT(!help_text_is_equal(text_exp, text_out, 4));

    param_list_free(list);
}
