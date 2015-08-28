/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


/** \file exatest_lib.c
 *  \brief This is the libconfig API for Exanodes.
 *  This library is based on the libxml2 and is used to parsed and access
 *  content of the Exanodes configuration file.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int nb_error = 0;
static int nb_test  = 0;

/** Return 1 is there was an error since the last exatest_init
 *
 *
 */
int exatest_status() {

  return (nb_error>0?1:0);

}

/** Initialise the lib to run a test.
 *  \param title A name for this test
 *
 *  The test counters are resetted
 */

void exatest_init(const char *title) {

  printf("\n");
  printf("================================================================================\n");
  printf("==== %-70s ====\n", title);
  printf("================================================================================\n");
  printf("\n");
  nb_error = 0;
  nb_test  = 0;
}

/** Display a title for this test
 *
 *
 */
void exatest_title(const char *title, const char *subtible) {

  printf("\n");
  printf("--------------------------------------------------------------------------------\n");
  printf("%s %s\n", title, subtible);
  printf("--------------------------------------------------------------------------------\n");
}

/** Display a subtitle for this test
 *
 *
 */
void exatest_subtitle(const char *title, const char *subtible) {

  printf("\n");
  printf("**** %s %s ****\n", title, subtible);
}

/** Display a summary for this test
 *
 *  Call it at the end og your test, or before a new exatest_init
 *
 */
void exatest_summary(const char *title) {
  printf("\nSUMMARY: TEST_NAME = %40s %d/%d %s\n\n", title, nb_error, nb_test, (nb_error?"NOK":"OK"));
}

/** Compare an expected value and a 'got' value
 *
 *  Increment the test counters in concequence
 *
 */

void exatest_result(const char *test_title, int expected, int got)
{

  nb_test++;

  if(expected == got)
  {
    printf("%76s %s\n", test_title, "OK");
  }
  else
  {
    printf("%76s %s   exp=%d got=%d\n", test_title, "NOK", expected, got);
    nb_error++;
  }
}
