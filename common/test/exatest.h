/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \file exatest.h
 *  \brief This is the include file for the unit test system
 *
 */

#ifndef _EXATEST_LIB_H
#define _EXATEST_LIB_H

int  exatest_status();
void exatest_init(const char *title);
void exatest_summary(const char *title);
void exatest_title(const char *title, const char *subtible);
void exatest_subtitle(const char *title, const char *subtible);
void exatest_result(const char *test_title, int expected, int got);


#endif
