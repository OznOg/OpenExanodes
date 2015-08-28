/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef  __EXA_EXPANDCPP_H__
#define  __EXA_EXPANDCPP_H__

#include <set>
#include <string>

std::set<std::string> exa_expand(const std::string &items) throw(std::string);
std::set<std::string> exa_unexpand(const std::string &list) throw(std::string);


#endif /* __EXA_EXPANDCPP_H__ */
