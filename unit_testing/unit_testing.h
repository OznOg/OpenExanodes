/*
 * Copyright 2002, 2015 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes Unit test library and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef H_UNIT_TESTING
#define H_UNIT_TESTING

#include <math.h>
#include "os/include/os_stdio.h"
#include "os/include/os_mem.h"
#include <string.h>
#ifndef WIN32
#include <assert.h>
#endif

#ifdef __cplusplus
#include <string>
#endif

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#else
# define RUNNING_ON_VALGRIND 0
#endif

#ifdef WIN32
  #define INT64_FMT  "%PRId64"
  #define UINT64_FMT "%PRIu64"
#else
  #define INT64_FMT  "%lld"
  #define UINT64_FMT "%llu"
#endif

#define UT_SECTION(name)

#define ut_test(name) \
    static void ut_testcase__##name(void)

#define __ut_lengthy
#define __ut_timeout(n)
#define __ut_signal(sig)

extern void ut_printf(const char *format, ...)
#ifndef WIN32
    __attribute__((__format__(printf, 1, 2)))
#endif
    ;

extern void ut_code_printf(const char *format, ...)
#ifndef WIN32
    __attribute__((__format__(printf, 1, 2)))
#endif
    ;

#ifndef UT_REALLY_ASSERT
#ifdef WIN32
#define UT_REALLY_ASSERT()			\
    do                                                          \
    {                                                           \
	exit(1);						\
    } while (false)
#else
#define UT_REALLY_ASSERT()                      \
    do                                                          \
    {                                                           \
        assert(0);                                              \
    } while (false)
#endif
#endif

#define UT_FAIL()                                               \
    do                                                          \
    {                                                           \
        ut_printf("Failed (%s:%d)", __FILE__, __LINE__);        \
	ut_end(__UT_RESULT_FAILED);				\
	UT_REALLY_ASSERT();					\
    } while (false)

#define UT_ASSERT(pred)							\
    do									\
    {									\
	if (!(pred))							\
	{								\
	    ut_printf("Assertion failed (%s:%d): %s",			\
		      __FILE__, __LINE__, #pred);			\
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)

/* The only difference between the two macros below is the ## operator.
 * This is because icl does not handle ## operator but gcc requires it.
 * See http://software.intel.com/en-us/articles/intel-c-compiler-error-handling-_va_args_-macro-parameter/
 */
#ifdef WIN32
#define UT_ASSERT_VERBOSE(pred, fmt, ...)				\
    do									\
    {									\
	if (!(pred))							\
	{								\
	    ut_printf("Assertion '%s' failed (%s:%d): " fmt,		\
		      #pred, __FILE__, __LINE__, __VA_ARGS__);	        \
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)
#else /* WIN32 */
#define UT_ASSERT_VERBOSE(pred, fmt, ...)                               \
    do                                                                  \
    {                                                                   \
        if (!(pred))                                                    \
        {                                                               \
            ut_printf("Assertion '%s' failed (%s:%d): " fmt,            \
                      #pred, __FILE__, __LINE__, ## __VA_ARGS__);       \
            ut_end(__UT_RESULT_FAILED);                                 \
            UT_REALLY_ASSERT();                                         \
        }                                                               \
    } while (false)
#endif /* WIN32 */

#define UT_ASSERT_EQUAL(expected, expr)					\
    do									\
    {									\
	long double __ut_ipart;					\
	long double __ut_ld_expected;				\
	long double __ut_ld_expr;				\
	bool __ut_expr_intg, __ut_expected_intg, __ut_expr_posv, __ut_expected_posv;	\
	/* For the very unlikely case when long double mantissa is inferior to 64 bits. \
	   Assuming a 10 bytes size is not stricly speaking enough to deduce that \
	   the mantissa is encoded on 64 bits but it seems to be a general rule. At least \
	   most common platforms should fullfil this requirement. */	\
        if (sizeof(long double) < 10) {					\
            UT_ASSERT(expected == expr);                                \
            break;                                                      \
        }                                                               \
	/* Valgrind does not implement the same mantissa size than a real CPU, \
	 * so casting to double may cause the assertion to fail when it \
	 * shouldn't */ \
	if (RUNNING_ON_VALGRIND) {                                      \
            UT_ASSERT(expected == expr);                                \
            break;                                                      \
        }                                                               \
	__ut_ld_expected = (long double)(expected);		\
	__ut_ld_expr = (long double)(expr);			\
	if (__ut_ld_expected != __ut_ld_expr)				\
	{								\
            char *__ut_tmp = NULL;							\
            size_t __ut_mem_size = 0;                                                   \
	    __ut_expr_intg = (modfl (__ut_ld_expr, &__ut_ipart) == 0.0);		\
	    __ut_expected_intg = (modfl (__ut_ld_expected, &__ut_ipart) == 0.0);	\
	    __ut_expr_posv = (__ut_ld_expr >= 0.0);					\
	    __ut_expected_posv = (__ut_ld_expected >= 0.0);				\
	    __ut_mem_size = os_snprintf(__ut_tmp, 0, "Assertion failed (%s:%d): expected '%s' got '%s'", \
		    __FILE__, __LINE__,					\
		    __ut_expected_intg ? (__ut_expected_posv ? UINT64_FMT : INT64_FMT) : "%Lg", \
		    __ut_expr_intg ? (__ut_expr_posv ? UINT64_FMT : INT64_FMT) : "%Lg") + 1;	\
            __ut_tmp = (char *)os_malloc(__ut_mem_size);  /* cast necessary for c++ code */     \
	    os_snprintf(__ut_tmp, __ut_mem_size, "Assertion failed (%s:%d): expected '%s' got '%s'", \
		    __FILE__, __LINE__,					\
		    __ut_expected_intg ? (__ut_expected_posv ? UINT64_FMT : INT64_FMT) : "%Lg", \
		    __ut_expr_intg ? (__ut_expr_posv ? UINT64_FMT : INT64_FMT) : "%Lg");	\
	    if (__ut_expected_intg)						\
		if (__ut_expected_posv)					\
		    if (__ut_expr_intg)					\
			if (__ut_expr_posv)					\
			    ut_printf(__ut_tmp, (unsigned long long int)(__ut_ld_expected), (unsigned long long int)(__ut_ld_expr)); \
			else						\
			    ut_printf(__ut_tmp, (unsigned long long int)(__ut_ld_expected), (long long int)(__ut_ld_expr)); \
		    else						\
			ut_printf(__ut_tmp, (unsigned long long int)(__ut_ld_expected), (long double)(__ut_ld_expr)); \
		else							\
		    if (__ut_expr_intg)					\
			if (__ut_expr_posv)					\
			    ut_printf(__ut_tmp, (long long int)(__ut_ld_expected), (unsigned long long int)(__ut_ld_expr)); \
			else						\
			    ut_printf(__ut_tmp, (long long int)(__ut_ld_expected), (long long int)(__ut_ld_expr)); \
		    else						\
			ut_printf(__ut_tmp, (long long int)(__ut_ld_expected), (long double)(__ut_ld_expr)); \
	    else							\
		if (__ut_expr_intg)						\
		    if (__ut_expr_posv)					\
			ut_printf(__ut_tmp, (long double)(__ut_ld_expected), (unsigned long long int)(__ut_ld_expr)); \
		    else						\
			ut_printf(__ut_tmp, (long double)(__ut_ld_expected), (long long int)(__ut_ld_expr)); \
		else							\
		    ut_printf(__ut_tmp, (long double)(__ut_ld_expected), (long double)(__ut_ld_expr)); \
            os_free(__ut_tmp);                                          \
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)


#ifdef __cplusplus

#define UT_ASSERT_EQUAL_STR(expected, expr)				\
    do									\
    {									\
	std::string __ut_s1(expected);					\
        std::string __ut_s2(expr);					\
	if (__ut_s1 != __ut_s2)						\
	{								\
	    ut_printf("Assertion failed (%s:%d): expected '%s', got '%s'", \
		      __FILE__, __LINE__, __ut_s1.c_str(), __ut_s2.c_str()); \
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)

#define UT_ASSERT_NOT_EQUAL_STR(expected, expr)				\
    do									\
    {									\
	std::string __ut_s1(expected);					\
        std::string __ut_s2(expr);					\
	if (__ut_s1 == __ut_s2)						\
	{								\
	    ut_printf("Assertion failed (%s:%d): unexpected '%s', got '%s'", \
		      __FILE__, __LINE__, __ut_s1.c_str(), __ut_s2.c_str()); \
	    ut_end(__UT_RESULT_FAILED);                                 \
	    UT_REALLY_ASSERT();						\
	}                                                               \
    } while (false)

#else

#define UT_ASSERT_EQUAL_STR(expected, expr)				\
    do									\
    {									\
	if (strcmp((expected), (expr)))					\
	{								\
	    ut_printf("Assertion failed (%s:%d): expected '%s', got '%s'", \
		      __FILE__, __LINE__, (expected), (expr));		\
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)

#define UT_ASSERT_NOT_EQUAL_STR(expected, expr)				\
    do									\
    {									\
	if (!strcmp((expected), (expr)))					\
	{								\
	    ut_printf("Assertion failed (%s:%d): unexpected '%s', got '%s'", \
		      __FILE__, __LINE__, (expected), (expr));		\
	    ut_end(__UT_RESULT_FAILED);					\
	    UT_REALLY_ASSERT();						\
	}								\
    } while (false)

#endif

#endif /* H_UNIT_TESTING */
