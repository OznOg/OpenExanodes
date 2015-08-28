/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _EXA_SERVICE_PARAM_H
#define _EXA_SERVICE_PARAM_H


/** Number of choices allowed in an exa_service_parameter_t */
#define EXA_PARAM_MAX_CHOICES 16

/** Some limits in the strings */
#define EXA_MAXSIZE_PARAM_NAME 31
/* It must be at least enough for a hostlist.
   Or more, if there are any other bigger requirements. */
#define EXA_MAXSIZE_PARAM_VALUE EXA_MAXSIZE_HOSTSLIST
#define EXA_MAXSIZE_PARAM_OPERATION 32

/** parameter type, represent the type like text, int, boolean of the param
 */
typedef enum {
  EXA_PARAM_TYPE_INT,
  EXA_PARAM_TYPE_BOOLEAN,
  EXA_PARAM_TYPE_TEXT,
  EXA_PARAM_TYPE_LIST,
  EXA_PARAM_TYPE_NODELIST,
  EXA_PARAM_TYPE_IPADDRESS,
} exa_param_type_t;

/**
 * Definition of a service parameter
 */
typedef struct {

  /** The unique name of the parameter
   *  \warning 'name' is a reserved word
   */
  const char *name;

  /** Parameter type */
  exa_param_type_t type;

  /** Description */
  const char *description;

  /** List of parameters (for EXA_PARAM_TYPE_TEXT) */
  const char *choices[EXA_PARAM_MAX_CHOICES];

  /** List of parameters (for EXA_PARAM_TYPE_INT) */
  int min;
  int max;

  /** Default */
  const char *default_value;

} exa_service_parameter_t;


/*--- Service Parameter API ------------------------------------------- */

int
exa_service_parameter_check(exa_service_parameter_t *service_param,
			    const char *value);

exa_service_parameter_t *
exa_service_parameter_get(const char *param_name);

const char *
exa_service_parameter_get_default(const char *param_name);

exa_service_parameter_t *
exa_service_parameter_get_list(int *iterator);


#endif
