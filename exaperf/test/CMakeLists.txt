#
# Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
# reserved and protected by French, UK, U.S. and other countries' copyright laws.
# This file is part of Exanodes project and is subject to the terms
# and conditions defined in the LICENSE file which is present in the root
# directory of the project.
#

include(UnitTest)

################# extern_test ###############
add_executable(extern_test
  extern_test_1.c
  extern_test_2.c)

target_link_libraries(extern_test
  exaperf
  exa_os
  exa_common_user)

################# ut_exaperf_config ###############
add_unit_test(ut_exaperf_config)

target_link_libraries(ut_exaperf_config
  exaperf
  exa_os)

################# ut_exaperf_filter ###############
add_unit_test(ut_exaperf_filter)

target_link_libraries(ut_exaperf_filter
  exaperf)

################# ut_exaperf_tools ###############
add_unit_test(ut_exaperf_tools)

target_link_libraries(ut_exaperf_tools
  exaperf)

################# ut_exaperf_stat ###############
add_unit_test(ut_exaperf_stats)

target_link_libraries(ut_exaperf_stats
  exaperf
  exa_os)

################# ut_exaperf_sample ###############
add_unit_test(ut_exaperf_sample)

target_link_libraries(ut_exaperf_sample
  exaperf
  exa_os)

################# ut_exaperf_sensor_template ###############
add_unit_test(ut_exaperf_sensor_template)

target_link_libraries(ut_exaperf_sensor_template
  exaperf
  exa_os
  exa_common_user)

################# ut_exaperf_sensor ###############
add_unit_test(ut_exaperf_sensor)

target_link_libraries(ut_exaperf_sensor
  exaperf
  exa_os
  exa_common_user)

################# ut_exaperf_core ###############
add_unit_test(ut_exaperf_core)

target_link_libraries(ut_exaperf_core
  exaperf
  exa_os
  exa_common_user)

################ ut_exaperf_counter ######################
add_unit_test(ut_exaperf_counter)

target_link_libraries(ut_exaperf_counter
  exaperf
  exa_os
  exa_common_user)

################ ut_exaperf_duration ######################
add_unit_test(ut_exaperf_duration)

target_link_libraries(ut_exaperf_duration
  exaperf
  exa_os
  exa_common_user)

################ ut_exaperf_repart ######################
add_unit_test(ut_exaperf_repart)

target_link_libraries(ut_exaperf_repart
  exaperf
  exa_os
  exa_common_user)

################ ut_exaperf ######################
add_unit_test(ut_exaperf)

target_link_libraries(ut_exaperf
  exaperf
  exa_os
  exa_common_user)

################ ut_exaperf_flush ######################
add_unit_test(ut_exaperf_flush)

target_link_libraries(ut_exaperf_flush
  exaperf
  exa_os
  exa_common_user)

