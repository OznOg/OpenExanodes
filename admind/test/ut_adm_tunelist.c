/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unit_testing.h>

#include "admind/src/commands/tunelist.h"
#include "os/include/strlcpy.h"


ut_test(tunelist_classic)
{
  tunelist_t* tunelist;
  tune_t *tune;
  const char *result_buf;

  UT_ASSERT_EQUAL(0, tunelist_create(&tunelist));

  result_buf = tunelist_get_result(tunelist);
  UT_ASSERT(result_buf != NULL);

  tune = tune_create(1);
  UT_ASSERT(tune != NULL);

  /* Check our result tree */
  UT_ASSERT_EQUAL_STR("<?xml version=\"1.0\"?>\n"
		      "<Exanodes/>\n",
		      result_buf);

  tune_set_name(tune, "N1");
  tune_set_description(tune, "D1");
  tune_set_nth_value(tune, 0, "V1");
  UT_ASSERT_EQUAL(0, tunelist_add_tune(tunelist, tune));

  result_buf = tunelist_get_result(tunelist);
  UT_ASSERT(result_buf != NULL);

  /* Check our result tree */
  UT_ASSERT_EQUAL_STR("<?xml version=\"1.0\"?>\n"
		      "<Exanodes>\n"
		      "  <param name=\"N1\" description=\"D1\" default=\"\" value=\"V1\"/>\n"
		      "</Exanodes>\n",
		      result_buf);

  tune_set_name(tune, "N2");
  tune_set_description(tune, "D2");
  tune_set_nth_value(tune, 0, "V2");
  UT_ASSERT_EQUAL(0, tunelist_add_tune(tunelist, tune));

  result_buf = tunelist_get_result(tunelist);
  UT_ASSERT(result_buf != NULL);

  /* Check our result tree */
  UT_ASSERT_EQUAL_STR("<?xml version=\"1.0\"?>\n<Exanodes>\n"
		      "  <param name=\"N1\" description=\"D1\" default=\"\" value=\"V1\"/>\n"
		      "  <param name=\"N2\" description=\"D2\" default=\"\" value=\"V2\"/>\n"
		      "</Exanodes>\n",
		      result_buf);

  tune_delete(tune);
  tunelist_delete(tunelist); /* No error code to check */
}


ut_test(tunelist_several_values)
{
  tunelist_t* tunelist;
  tune_t *tune_a, *tune_b;
  const char *result_buf;

  tune_a = tune_create(5);

  UT_ASSERT(tune_a != NULL);
  UT_ASSERT_EQUAL(0, tunelist_create(&tunelist));

  tune_set_name(tune_a, "N1");
  tune_set_description(tune_a, "D1");
  tune_set_nth_value(tune_a, 0, "V0");
  tune_set_nth_value(tune_a, 1, "V1");
  tune_set_nth_value(tune_a, 2, "V2");
  tune_set_nth_value(tune_a, 3, "V3");
  tune_set_nth_value(tune_a, 4, "V4");
  UT_ASSERT_EQUAL(0, tunelist_add_tune(tunelist, tune_a));

  tune_b = tune_create(3);
  tune_set_name(tune_b, "N2");
  tune_set_description(tune_b, "D2");
  tune_set_nth_value(tune_b, 0, "V0");
  tune_set_nth_value(tune_b, 1, "V1");
  tune_set_nth_value(tune_b, 2, "w%02dt", 0);
  tune_set_default_value(tune_b, "%s %d", "toto", 2);
  UT_ASSERT_EQUAL(0, tunelist_add_tune(tunelist, tune_b));

  result_buf = tunelist_get_result(tunelist);
  UT_ASSERT(result_buf != NULL);

  /* Check our result tree */
  UT_ASSERT_EQUAL_STR("<?xml version=\"1.0\"?>\n<Exanodes>\n"
		      "  <param name=\"N1\" description=\"D1\" default=\"\">\n"
		      "    <value_item value=\"V0\"/>\n"
		      "    <value_item value=\"V1\"/>\n"
		      "    <value_item value=\"V2\"/>\n"
		      "    <value_item value=\"V3\"/>\n"
		      "    <value_item value=\"V4\"/>\n"
		      "  </param>\n"
		      "  <param name=\"N2\" description=\"D2\" default=\"toto 2\">\n"
		      "    <value_item value=\"V0\"/>\n"
		      "    <value_item value=\"V1\"/>\n"
		      "    <value_item value=\"w00t\"/>\n"
		      "  </param>\n"
		      "</Exanodes>\n",
		      result_buf);

  tune_delete(tune_a);
  tune_delete(tune_b);
  tunelist_delete(tunelist); /* No error code to check */
}
