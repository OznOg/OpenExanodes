/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __TYPE_CLUSTERED__H
#define __TYPE_CLUSTERED__H

#include "admind/services/fs/generic_fs.h"
#include "admind/src/admind.h"

struct adm_fs;

/* FIXME: These declaration should probably be made static and moved
 * inside type_clustered.c. */

exa_error_code prepare_clustered_fs(int thr_nb,const char* fstype,const exa_nodeset_t* node_up);
exa_error_code unload_clustered_fs(int thr_nb,const char* fstype);
exa_error_code clustered_check_before_start(int thr_nb, fs_data_t* fs);
exa_error_code clustered_start_fs(int thr_nb, const exa_nodeset_t* nodes,
				  const exa_nodeset_t* node_set_read_only,
				  fs_data_t* fs, exa_nodeset_t* stop_succeeded,
				  int recovery);
exa_error_code clustered_stop_fs(int thr_nb, const exa_nodeset_t* nodes,
                                 fs_data_t *fs, bool force,
                                 adm_goal_change_t goal_change,
                                 exa_nodeset_t* stop_succeeded);
exa_error_code clustered_check_fs(int thr_nb, fs_data_t* fs, const char* optional_parameter,
				  exa_nodeid_t node_where_to_check, bool repair);
exa_error_code clustered_specific_fs_recovery(int thr_nb, fs_data_t* fs);
int            is_clustered_started(const char* fstype);

#endif
