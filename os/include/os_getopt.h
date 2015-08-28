/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef OS_GETOPT_H
#define OS_GETOPT_H

#ifndef WIN32
# include <unistd.h>
# include <getopt.h>
#define os_getopt getopt
#define os_getopt_long getopt_long
#else

#ifdef __cplusplus
extern "C" {
#endif

extern int optind;              /* index of first non-option in argv      */
extern int optopt;              /* single option character, as parsed     */
extern int opterr;              /* flag to enable built-in diagnostics... */
                                /* (user may set to zero, to suppress)    */

extern char *optarg;            /* pointer to argument of current option  */

struct option           /* specification for a long form option...      */
{
  const char *name;             /* option name, without leading hyphens */
  int         has_arg;          /* does it take an argument?            */
  int        *flag;             /* where to save its status, or NULL    */
  int         val;              /* its associated status value          */
};

enum                    /* permitted values for its `has_arg' field...  */
{
  no_argument = 0,              /* option never takes an argument       */
  required_argument,            /* option always requires an argument   */
  optional_argument             /* option may take an argument          */
};

/**
 * parse the argument line array of a process (the argv parameter of main())
 * The function follows the linux implementation, see man 3 getopt for
 * full details and explainations
 *
 * @param argc        number of arguments.
 *
 * @param argv        the array of arguments.
 *
 * @param optstring   string describing the switches awaited.
 *
 * @return   the parameter parsed or -1 when no more parameter is available.
 *
 * @os_replace{Linux, getopt}
 */
int os_getopt(int argc, char *const argv[],
              const char *optstring);

/**
 * The long option version of the getopt function (see linux man 3 getopt_long
 * for more details.)
 *
 * @param argc        number of arguments.
 *
 * @param argv        the array of arguments.
 *
 * @param optstring   string describing the switches awaited.
 *
 * @param longopts    an array describing the long options that can be awaited
 *                    and their corresponding short option if any.
 *
 * @param longindex   if not NULL, points to the corresponding longopt array
 *                    option index.
 *
 * @return   the char corresponding to the parsed option or -1 when no more
 *           parameter is available in argv.
 *
 * @os_replace{Linux, getopt_long}
 */
int os_getopt_long(int argc, char *const argv[],
                   const char *optstring,
	           const struct option *longopts, int *longindex);

#ifdef __cplusplus
}
#endif

#endif /* WIN32 */

#endif /* OS_GETOPT_H */
