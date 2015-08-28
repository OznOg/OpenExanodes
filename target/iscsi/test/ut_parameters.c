/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "target/iscsi/include/parameters.h"

#include "os/include/os_stdio.h"

UT_SECTION(param_value_allowed)

ut_test(null_is_not_allowed)
{
    iscsi_parameter_t *param = NULL;

    /* XXX Should test for all parameter types */
    param_list_add(&param, ISCSI_PARAM_TYPE_DECLARATIVE, "Decl", "Something", "Something");
    UT_ASSERT(!param_value_allowed(param, NULL));

    param_list_free(param);
}

ut_test(any_declarative_is_allowed_for_declarative_param)
{
    iscsi_parameter_t *param = NULL;
    char c;

    param_list_add(&param, ISCSI_PARAM_TYPE_DECLARATIVE, "Decl", "Haha", "Haha");

    for (c = 'a'; c < 'z'; c++)
    {
        char s[2] = { c, '\0' };
        UT_ASSERT(param_value_allowed(param, s));
    }

    param_list_free(param);
}

ut_test(non_numerical_values_not_allowed_for_numerical_param)
{
    iscsi_parameter_t *param = NULL;
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_NUMERICAL,
                                      ISCSI_PARAM_TYPE_NUMERICAL_Z };
    int i;

    for (i = 0; i < 2; i++)
    {
        param_list_add(&param, types[i], "Num", "1", "65536");

        UT_ASSERT(!param_value_allowed(param, NULL));
        UT_ASSERT(!param_value_allowed(param, ""));
        UT_ASSERT(!param_value_allowed(param, "  "));
        UT_ASSERT(!param_value_allowed(param, "  123"));
        UT_ASSERT(!param_value_allowed(param, "hello"));
        UT_ASSERT(!param_value_allowed(param, "3.14159265"));

        param_list_free(param);
    }
}

ut_test(negative_values_not_allowed_for_numerical_param)
{
    iscsi_parameter_t *param = NULL;
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_NUMERICAL,
                                      ISCSI_PARAM_TYPE_NUMERICAL_Z };
    int i;

    for (i = 0; i < 2; i++)
    {
        param_list_add(&param, types[i], "Num", "1", "65536");
        UT_ASSERT(!param_value_allowed(param, "-99"));
        param_list_free(param);
    }
}

ut_test(positive_values_allowed_for_numerical_param)
{
    iscsi_parameter_t *param = NULL;
    unsigned sample[] = { 0, 1, 2, 3, 5, 16, 23, 999, 65536, 123456789 };
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_NUMERICAL,
                                      ISCSI_PARAM_TYPE_NUMERICAL_Z };
    int i, j;

    for (i = 0; i < 2; i++)
    {
        param_list_add(&param, types[i], "Num", "0", "999999999");

        for (j = 0; j < sizeof(sample) / sizeof(unsigned); j++)
        {
            char s[16];

            os_snprintf(s, sizeof(s), "%u", sample[j]);
            UT_ASSERT(param_value_allowed(param, s));
        }

        UT_ASSERT(param_value_allowed(param, "+160"));

        param_list_free(param);
    }
}

ut_test(values_higher_than_numerical_param_max_not_allowed)
{
    iscsi_parameter_t *param = NULL;
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_NUMERICAL,
                                      ISCSI_PARAM_TYPE_NUMERICAL_Z };
    int i;

    for (i = 0; i < 2; i++)
    {
        param_list_add(&param, types[i], "Num", "0", "5000");
        UT_ASSERT(!param_value_allowed(param, "5001"));
        param_list_free(param);
    }
}

ut_test(no_limit_for_numerical_param_z_when_max_is_zero)
{
    iscsi_parameter_t *param = NULL;
    char max64[32];

    /* Max 0 means no limit */
    param_list_add(&param, ISCSI_PARAM_TYPE_NUMERICAL_Z, "NumZ", "5", "0");

    /* Check the highest 64 bit value */
    UT_ASSERT(os_snprintf(max64, sizeof(max64), "%"PRIu64, UINT64_MAX) < sizeof(max64));
    UT_ASSERT(param_value_allowed(param, max64));

    param_list_free(param);
}

ut_test(value_not_in_list_param_is_not_allowed)
{
    iscsi_parameter_t *param = NULL;

    param_list_add(&param, ISCSI_PARAM_TYPE_LIST, "List", "Ping", "Ping,Pong");
    UT_ASSERT(!param_value_allowed(param, "Blam"));

    param_list_free(param);
}

ut_test(not_yesno_not_allowed_for_binary_param)
{
    iscsi_parameter_t *param = NULL;
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_BINARY_OR,
                                      ISCSI_PARAM_TYPE_BINARY_AND };
    int i;

    for (i = 0; i < 2; i ++)
    {
        param_list_add(&param, types[i], "boolean", "yes", "yes,no");
        UT_ASSERT(!param_value_allowed(param, "hello"));
        param_list_free(param);
    }
}

ut_test(yes_and_no_allowed_for_binary_param_regardless_of_case)
{
    iscsi_parameter_t *param = NULL;
    iscsi_parameter_type types[2] = { ISCSI_PARAM_TYPE_BINARY_OR,
                                      ISCSI_PARAM_TYPE_BINARY_AND };
    int i;

    for (i = 0; i < 2; i ++)
    {
        param_list_add(&param, types[i], "boolean", "yes", "yes,no");

        UT_ASSERT(param_value_allowed(param, "yes"));
        UT_ASSERT(param_value_allowed(param, "Yes"));
        UT_ASSERT(param_value_allowed(param, "YES"));

        UT_ASSERT(param_value_allowed(param, "no"));
        UT_ASSERT(param_value_allowed(param, "No"));
        UT_ASSERT(param_value_allowed(param, "NO"));

        param_list_free(param);
    }
}

UT_SECTION(PARAM_LIST)

ut_test(param_list_add_null_allocs_list)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "value", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    param_list_free(list);
}

ut_test(param_list_add_to_list)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "value", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);
    UT_ASSERT(list->next == NULL);

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key2", "value", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list->next != NULL);

    param_list_free(list);
}

ut_test(param_list_add_twice_fails)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "value", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);
    UT_ASSERT(list->next == NULL);

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "value2", "");
    UT_ASSERT_EQUAL(-1, r);
    UT_ASSERT(list->next == NULL);

    param_list_free(list);
}

ut_test(param_list_add_invalid_binary_or)
{
    iscsi_parameter_t *list = NULL;
    int r;

    /* Valid not comprised of a combination of yes/no. */
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key2", "value", "caca");
    UT_ASSERT_EQUAL(-1, r);
    UT_ASSERT(list == NULL);

    param_list_free(list);
}

ut_test(param_list_add_valid_binary_or)
{
    iscsi_parameter_t *list = NULL;
    int r;

    /* Valid comprised of a combination of yes/no. */
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key2", "Yes", "Yes,No");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key3", "Yes", "No,Yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key4", "No", "No");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key5", "Yes", "yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key6", "Yes", "yes,no");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key7", "Yes", "no,yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_OR, "key8", "No", "no");
    UT_ASSERT_EQUAL(0, r);

    param_list_free(list);
}

ut_test(param_list_add_invalid_binary_and)
{
    iscsi_parameter_t *list = NULL;
    int r;

    /* Valid not comprised of a combination of yes/no. */
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key2", "value", "caca");
    UT_ASSERT_EQUAL(-1, r);
    UT_ASSERT(list == NULL);

    param_list_free(list);
}

ut_test(param_list_add_valid_binary_and)
{
    iscsi_parameter_t *list = NULL;
    int r;

    /* Valid comprised of a combination of yes/no. */
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key2", "Yes", "Yes,No");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key3", "Yes", "No,Yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key4", "No", "No");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key5", "Yes", "yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key6", "Yes", "yes,no");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key7", "Yes", "no,yes");
    UT_ASSERT_EQUAL(0, r);
    r = param_list_add(&list, ISCSI_PARAM_TYPE_BINARY_AND, "key8", "No", "no");
    UT_ASSERT_EQUAL(0, r);

    param_list_free(list);
}

UT_SECTION(PARAM_LIST_VAL)

ut_test(param_list_get_value_gets_value_correctly)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "a", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT_EQUAL_STR("a", param_list_get_value(list, "key1"));
}

ut_test(param_list_get_value_get_unexistant_value_returns_NULL)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "a", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT(param_list_get_value(list, "key2") == NULL);
}

ut_test(param_list_get_value_NULL_list_returns_NULL)
{
    UT_ASSERT(NULL == param_list_get_value(NULL, "key1"));
}

ut_test(param_list_get_value_get_value_out_of_bounds_returns_NULL)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "a", "");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT_EQUAL_STR("a", param_list_get_value(list, "key1"));
}

UT_SECTION(PARAM_GET)

ut_test(param_exists_returns_correct_value)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key2", "Blah", "");
    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key3", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT_EQUAL(true, param_exists(list, "key1"));
    UT_ASSERT_EQUAL(true, param_exists(list, "key2"));
    UT_ASSERT_EQUAL(true, param_exists(list, "key3"));
    UT_ASSERT_EQUAL(false, param_exists(list, "key4"));
    UT_ASSERT_EQUAL(false, param_exists(list, "key5"));
    UT_ASSERT_EQUAL(false, param_exists(list, "key6"));

    param_list_free(list);
}

ut_test(param_list_elt_returns_correct_param)
{
    iscsi_parameter_t *list = NULL;
    iscsi_parameter_t *param = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key2", "Blah", "");
    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key3", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    param = param_list_elt(list, "key2");
    UT_ASSERT(param != NULL);

    UT_ASSERT_EQUAL_STR("key2", param->key);
    UT_ASSERT_EQUAL_STR("Blah", param->value);

    param_list_free(list);
}

ut_test(param_list_elt_wrong_key_returns_NULL)
{
    iscsi_parameter_t *list = NULL;
    iscsi_parameter_t *param = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    param = param_list_elt(list, "key2");
    UT_ASSERT(param == NULL);

    param_list_free(list);
}

ut_test(param_list_elt_NULL_list_returns_NULL)
{
    UT_ASSERT(NULL == param_list_elt(NULL, "key"));
}

UT_SECTION(PARAM_LIST_SET_VALUE)

ut_test(setting_value_of_unknown_key_fails)
{
    iscsi_parameter_t *list = NULL;

    UT_ASSERT_EQUAL(0, param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "one", "123", "123"));
    UT_ASSERT_EQUAL(0, param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "two", "456", "456"));

    UT_ASSERT_EQUAL(-1, param_list_set_value(list, "unknown_key", "hahaha"));

    param_list_free(list);
}

ut_test(setting_value_of_known_key_succeeds)
{
    iscsi_parameter_t *list = NULL;
    const iscsi_parameter_t *one, *two;

    UT_ASSERT_EQUAL(0, param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "one", "123", "123"));
    UT_ASSERT_EQUAL(0, param_list_add(&list, ISCSI_PARAM_TYPE_NUMERICAL, "two", "456", "456"));

    UT_ASSERT_EQUAL(0, param_list_set_value(list, "one", "hahaha_1"));
    UT_ASSERT_EQUAL(0, param_list_set_value(list, "two", "hahaha_2"));

    one = param_list_elt(list, "one");
    two = param_list_elt(list, "two");
    UT_ASSERT_EQUAL_STR("hahaha_1", one->value);
    UT_ASSERT_EQUAL_STR("hahaha_2", two->value);

    param_list_free(list);
}

UT_SECTION(PARAM_LIST_VALUE_IS_EQUAL)

ut_test(param_list_value_is_equal_returns_true_if_values_are_equal)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT(param_list_value_is_equal(list, "key1", "Yes"));
}

ut_test(param_list_value_is_equal_returns_false_if_values_differ)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT(!param_list_value_is_equal(list, "key1", "No"));
}

ut_test(param_list_value_is_equal_returns_false_when_param_is_not_found)
{
    iscsi_parameter_t *list = NULL;
    int r;

    r = param_list_add(&list, ISCSI_PARAM_TYPE_DECLARATIVE, "key1", "Yes", "Yes");
    UT_ASSERT_EQUAL(0, r);
    UT_ASSERT(list != NULL);

    UT_ASSERT(!param_list_value_is_equal(list, "key2", "No"));
    UT_ASSERT(!param_list_value_is_equal(NULL, "key1", "No"));
}
