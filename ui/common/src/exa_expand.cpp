/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

/** \brief The core of expand / unexpand.
 * The specifications are in the file docs/trunk/exanodes-2-3/expand_unexpand_spec.txt
 */

#include <set>
#include <vector>
#include <map>

#include "ui/common/include/exa_expand.h"

#include <errno.h>
#include <string.h>
#include <exception>

#include "common/include/exa_constants.h"
#include "os/include/os_mem.h"
#include "os/include/os_stdio.h"
#include "os/include/strlcpy.h"

using std::string;
using std::set;
using std::vector;
using std::map;
using std::exception;

/** number_to_string
 *  Helper function : print a number to a string, with a fixed length or not
 *  (length == 0)
 */
static string number_to_string(int number, int length)
{
  char buffer[1024];
  int real_length = sprintf(buffer, "%d", number);
  string prefix = (length == 0 ? "" : string (length - real_length ,'0'));

  return prefix + string(buffer);
}

/**
 * extract_string : read a string following a regexp, till it is finished,
 *                             or is something else
 *
 * \param[in]     non_null throw an error if the returned string is null
 * \param[in]     not_end  throws an error if it reaches the end of the
 *                original string
 * \param[in]     allowed_characters list of characters the result may contain.
 * \param[in,out] string_to_read modified to the new position.
 *
 * return what was read
 */
static string extract_string(string &string_to_read,
			     string allowed_characters,
			     bool non_null, bool not_end, bool only_one)
{
  int position = string_to_read.find_first_not_of(allowed_characters, 0);
  if (not_end && (position == -1))
      throw exception();

  if (non_null && (position == 0))
    throw exception();

  if (position == -1)
    {
      position = string_to_read.size();
    }
  if (position == 0)
    {
      return "";
    }
  if (only_one)
    {
      position = 1;
    }
  string result = string_to_read.substr(0, position);
  string_to_read = string_to_read.substr(position, string_to_read.size() - position);

  return result;
}

#define NUMSTRING "0123456789"
#define ALPHASTRING ":-_.ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define ALPHANUMSTRING NUMSTRING ALPHASTRING

/** read_ALPHANUMSTRING
 *  Helper function : extract an ALPHANUMERIC string. It can be "".
 */
static string read_ALPHANUMSTRING(string &string_to_read)
{
  string result, numstring, alphastring;
  result = extract_string(string_to_read, ALPHANUMSTRING, false, false, false);

  return result;
}

/** read_ALPHANUMSTRINGNONNULL
 *  Helper function : extract an ALPHANUMERIC string. It cannot be "".
 */
static string read_ALPHANUMSTRINGNONNULL(string &string_to_read)
{
  string result, numstring, alphastring;
  result = extract_string(string_to_read, ALPHANUMSTRING, true, false, false);

  return result;
}

/** read_ITEM : read a number
 *  Helper function : extract an NUMERIC string. It cannot be "".
 */
static string read_ITEM(string &string_to_read)
{
  return extract_string(string_to_read, NUMSTRING, true, true, false);
}

/** read_EXPRESSION
 *
 * Read any string of the form "01:03-05:5/" .
 * The starting "/" is already eaten, the last one is mandatory and stops the
 * reading.
 *
 * return the list of strings once expanded.
 */
static set<string> read_EXPRESSION(string &string_to_read)
{
  set<string> result;
  string first_ITEM;
  unsigned int first_VALUE;
  bool end = false;

  first_ITEM = read_ITEM(string_to_read);
  first_VALUE = atoi(first_ITEM.c_str());

  do
    {
      string current_character = extract_string(string_to_read,
						":-/", true, false, true);
      switch (current_character[0])
	{
	case ':': /* simple item */
	  result.insert(first_ITEM);
	  first_ITEM = read_ITEM(string_to_read);
	  first_VALUE = atoi(first_ITEM.c_str());
	  break;

	case '/': /* End of EXPRESSION */
	  result.insert(first_ITEM);
	  end = true;
	  break;

	case '-': /* range */
	  string second_ITEM = read_ITEM(string_to_read);
	  unsigned int second_VALUE = atoi(second_ITEM.c_str());
	  if (first_VALUE >= second_VALUE)
              throw exception();

	  int length = 0;
          /* 0-prefixed. '0' is not prefixed. '00' is. */
          if ((first_ITEM[0] == '0' && first_ITEM.size() > 1) ||
              (second_ITEM[0] == '0' && second_ITEM.size() > 1) )
	    {
	      /* Sanity check */
	      if (first_ITEM.size() != second_ITEM.size())
                  throw exception();

	      length = first_ITEM.size();
	    }
	  for (unsigned int index = first_VALUE; index <= second_VALUE; index++)
	      result.insert(number_to_string(index, length));
	  break;
	}
    }
  while (!end);

  return result;
}

/** string_set_add_string
 *
 * Helper : Add a postfix to every string of a set of strings.
 *
 * return the old set is replaced by the new one.
 */
static void string_set_add_string(set<string> &prefix, string postfix)
{
  set<string> temp_result;
  for (set<string>::iterator index = prefix.begin();
       index != prefix.end();
       index++)
    {
      temp_result.insert(*index + postfix);
    }
  prefix = temp_result;
}

/** exa_expand
 *
 * Entry point to exa_expand.
 *
 * return the set once expanded.
 */
static set<string> exa_expand_(const string &items)
{
  string current_string = items;
  set<string> result;

  while (current_string.size() != 0)
    {
      set<string> current_node_result;
      bool node_end = false;

      /* Delete residual spaces */
      extract_string(current_string, " ", false, false, false);
      if (current_string.size() == 0)
	  break;

      /* Any string */
      current_node_result.insert(read_ALPHANUMSTRING(current_string));
      /* What else ? */
      while ((!node_end) && (current_string.size() != 0))
	{
	  string current_character = extract_string(current_string,
						    "/ ",
						    true, false, true);
	  char car = current_character[0];
	  if (car == ' ')
	    {
	      node_end = true;
	    }
	  else     /*  '/'  */
	    {
	      set<string> temp_result, org_current_string;
	      set<string> expression = read_EXPRESSION(current_string);
	      /* multiply the number of outputs by as many expressions as found */
	      for (set<string>::iterator index_expression = expression.begin();
		   index_expression != expression.end();
		   index_expression++)
		{
		  org_current_string = current_node_result;
		  string_set_add_string(org_current_string, *index_expression);
		  temp_result.insert(org_current_string.begin(),
				     org_current_string.end());
		}
	      current_node_result = temp_result;
	      /* is there another possibly null ALPHANUMSTRING after ? */
	      string_set_add_string(current_node_result,
				    read_ALPHANUMSTRING(current_string));
	    }
	}
      /* Dump to the result list */
      result.insert(current_node_result.begin(), current_node_result.end());
    }

  return result;
}


typedef vector<string> value_vector_t;

/** exa_unexpand_build_number_regexp_from_numbers
 *
 * Get a set of integers and builds a regexp, returned as a string.
 * If length is != 0, we must be 0-prefixed and fixed size.
 * If not, the list is not 0-prefixed, and can be variable-size.
 * NOTE : the set must be sorted in the increasing order.
 *
 * return the string built.
 */
static string exa_unexpand_build_number_regexp_from_numbers(set<unsigned int> &number_list, int length)
{
  string result;
  int first_number = -2, last_number = -2;
  string sequence_string;

  for (set<unsigned int>::iterator index_number = number_list.begin();
       index_number != number_list.end();
       index_number++)
    {
      int current_number = *index_number;
      string current_number_string = number_to_string(current_number, length);
      if (current_number == last_number + 1)
	{
	  sequence_string =
	    number_to_string(first_number, length)
	    + "-"
	    + current_number_string;
	}
      else
	{
	  first_number = current_number;
	  result += ((result != "" ) ? ":" : "" ) + sequence_string;
	  sequence_string = current_number_string;
	}
      last_number = current_number;
    }
  result += ((result != "" ) ? ":" : "" ) + sequence_string;

  return result;
}

/** exa_unexpand_build_number_regexp_from_strings
 *
 * Get a vector of strings and builds a regexp, returned as a string.
 * The vector can contain 2 times the same value, and 0-prefixed and
 * 0-unprefix can be mixed.
 *
 * It works this way : "integer_list" is used to sort each string according
 * to its length size, if prefixed.
 * If not prefixed, it goes to the "0" key.
 * Once mapped, we build a string upon it using by
 * exa_unexpand_build_number_regexp_from_numbers
 *
 * return the string built.
 */
static string exa_unexpand_build_number_regexp_from_strings(value_vector_t number_list)
{
  string result;
  typedef map< int, set<unsigned int> > integer_list_t;
  integer_list_t integer_list;
  /* 2 modes of execution : either 0-prefixed or un-0-prefixed.
   Everything starting with "0" is 0-prefixed, EXCEPT the string "0",
   which is considered not to be 0-prefixed.
  */
  for (unsigned int index_string = 0;
       index_string != number_list.size();
       index_string++)
    {
      int zero_prefixed = 0;
      string current_string = number_list[index_string];
      unsigned int value = atoi(current_string.c_str());
      if (value == 0) /* Then is this more than 1 in length ? */
	{
	  if (current_string.length() != 1)
	    {
	      zero_prefixed = current_string.length();
	    }
	}
      else
	{
	  if (current_string.find_first_not_of("0") != 0)
	    {
	      zero_prefixed = current_string.length();
	    }
	}
      integer_list[zero_prefixed].insert(value);
    }
  /* For each number of zero, and same digit number, build a regexp */
  for (integer_list_t::iterator index_map = integer_list.begin();
       index_map != integer_list.end();
       index_map++)
    {
      string index_level_string =
	exa_unexpand_build_number_regexp_from_numbers(index_map->second,
						      index_map->first);
      result += ((result == "") ? "" : ":") + index_level_string;
    }

  return string("/") + result + string("/");
}



typedef set<value_vector_t> value_vector_list_t;
typedef map<value_vector_t, value_vector_list_t > map_radix_t;
typedef value_vector_t radix_value_decomposition_t[2];

/** extract_radixes_values
 *
 * Cut a string into 2 lists : one for radixes, one for values.
 * If it finishes with a value, add 2 values in the lists.
 * the argument must be a non-null string with ALPHANUMERICAL only.
 *
 * return fill the 2 lists.
 */
static void extract_radixes_values(const string &str,
				       radix_value_decomposition_t &lists)
{
  unsigned int index = 0;
  int current_list = 0;

  lists[0].push_back(string(""));
  lists[1].push_back(string(""));

  while (index != str.length())
    {
      char current_char = str[index++];
      int is_number = ((int)string(NUMSTRING).find(current_char)) == -1 ? 0:1;

      if (current_list != is_number)
	{
	  current_list++;
	  if (current_list > 1)
	    {
	      lists[0].push_back(string(""));
	      lists[1].push_back(string(""));
	      current_list = 0;
	    }
	}
      lists[current_list].back() += current_char;
    }
  /* SUBTILITY : to manage names with an ALPHA terminal (sam12v3a), we add
     a generic ("","") to other NON-ALPHA names and we will ignore the last
     numerical values which therefore will always be "". */
  if (lists[1].back() != "")
    {
      lists[0].push_back(string(""));
      lists[1].push_back(string(""));
    }
}

/** extract_lists_with_same_values
 *
 * Given a reference vector, a level "exclude_level", and a list of vector,
 * fill two lists :
 * - hose which have at least one different value for any other level than
 *   "exclude_level" ("different")
 * - and the values inside "exclude_level" when all others level match.
 *
 * return "same_level_values_for_regexp" and "different" are filled.
 */
static void extract_lists_with_same_values(int exclude_level,
				    value_vector_t &value,
				    value_vector_list_t &values,
				    value_vector_list_t &different,
				    value_vector_t &same_level_values_for_regexp)
{
  for (value_vector_list_t::iterator index_comparator = values.begin();
       index_comparator != values.end();
       index_comparator++)
    {
      value_vector_t for_comparison = *index_comparator;
      bool has_same_other_levels = true;

      for (unsigned int index_compare_level = 0;
	   /* Forget the last level, according to "SUBTILITY" upper */
	   index_compare_level < value.size() - 1;
	   index_compare_level++)
	{
	  if ((exclude_level != (int)index_compare_level) &&
	      (value[index_compare_level] != for_comparison[index_compare_level]))
	    {
	      has_same_other_levels = false;
	      break;
	    }
	}
      if (has_same_other_levels)
	{
	  same_level_values_for_regexp.push_back(for_comparison[exclude_level]);
	}
      else
	different.insert(for_comparison);
    }
}

/** exa_unexpand
 *
 * Entry point to exa_unexpand.
 *
 * Algorithm:
 *  - cut the string in substrings separated by spaces.
 *  - cut each string in subsets of values (integer), radixes (alpha strings).
 *     example : xen75v1  -> radix "xen" value "75" radix "v" value "1"
 *  - build a map of strings, ordered by radix, where elements are lists of
 *    vector of values.
 *     example : xen75v1 xen75v2 xen76v1 sam10 sam11
 *               -> (("xen" "v") -> ( [75 1] [75 2] [76 1])
 *                   ("sam"    ) -> ( [10] [11]))
 *  - for each list of radixes, try to find associations. For this:
 *     - loop over each vector of values for the same key (== same radix,
 *       example ["xen" "v"] ). Now consider the list of values.
 *     - for each level, starting from the last one ("level"s are the depth of
 *       the list of values. example: [75 1] = 2 levels)
 *       - build a list of values (we call it "same") that have the same values
 *         except for the "level" for which we are trying to build a regexp.
 *         Example : For level 1 in list [75 1] [75 2] [76 1], with reference
 *                   [75 1] will return a list with 2 values: [75 1] [75 2].
 *         This list is stored inside the variable "same". Others are stored in
 *         the variable "different".
 *       - If the length of this list is >= 1, (at least one, because it contains
 *         the reference)
 *         * take the list of strings formed by each value in the level considered
 *           (example for [75 1] [75 2] -> [1 2])
 *         * build a regexp from "same" ( example "/1-2/")
 *         * shrink all the strings that matched (those which we used to build
 *           "same")
 *       - start back from the beginning, there may be other ways to shrink now
 *         that this list is built.
 *  - Return the set of values built using their radix/values list.
 *
 * return the set once expanded.
 */
static std::set<std::string> exa_unexpand_(const string &original_string)
{
  std::set<std::string> result;
  map_radix_t radix_list;
  set<string> original_list;
  string copy = original_string;

  /* Cut the original string in sub strings separated by " " */
  do
    {
      extract_string(copy," ", false, false, false);
      original_list.insert(read_ALPHANUMSTRINGNONNULL(copy));
      extract_string(copy," ", false, false, false);
    }
  while (copy.size());

  /* Cut each string and put it in the right list according to
     the key==list of intermediate strings */
  for (set<string>::iterator i_str = original_list.begin();
       i_str != original_list.end();
       i_str++)
    {
      string str = *i_str;
      radix_value_decomposition_t lists;
      extract_radixes_values(str, lists);
      radix_list[lists[0]].insert(lists[1]);
    }

  /* For the list of same radixes, (try to) create an intelligent regexp. */
  for (map_radix_t::iterator index_radix = radix_list.begin();
       index_radix!= radix_list.end();
       index_radix++)
    {
      value_vector_t key = index_radix->first;
      value_vector_list_t &values = index_radix->second;
      for (int level = key.size()-2; level >=0; level--)
	/* start from the end */
	/* and forget the last level, according to SUBTILITY upper */
	{
	  value_vector_list_t::iterator index_values = values.begin();
	  do
	    {
	      value_vector_list_t different;
	      value_vector_t same_level_values_for_regexp;
	      value_vector_t value = *index_values; /* this is the reference vector */
	      extract_lists_with_same_values(level, value,
					     values,
					     different,
					     same_level_values_for_regexp);
	      /* How many are equals ? If less than 1 (only the original) then
		 it's useless to build a regexp. Otherwise, go for a regexp ! */
	      if (same_level_values_for_regexp.size() > 1)
		{
		  value[level] = exa_unexpand_build_number_regexp_from_strings(same_level_values_for_regexp);
		  different.insert(value);
		  values = different;
		  /* sorry, the list has been twisted, we'll start again from the beginning,
		     at the cost of a little overhead */
		  index_values = values.begin();
		}
	      else /* Well, pass your way */
		{
		  index_values++;
		}
	    }
	  while (index_values != values.end());
	}
    }

  /* Build the final set */
  for (map_radix_t::iterator index_radix = radix_list.begin();
       index_radix!= radix_list.end();
       index_radix++)
    {
      value_vector_t key = index_radix->first;
      value_vector_list_t &values = index_radix->second;
      value_vector_list_t::iterator index_values = values.begin();
      while (index_values != values.end())
	{
	  value_vector_t value = *index_values;
	  string intermediate_result;
	  for (unsigned int level = 0; level != value.size(); level++)
	    {
	      intermediate_result += key[level] + value[level];
	    }
	  index_values++;
	  result.insert(intermediate_result);
	}
    }

  return result;
}


set<string> exa_expand(const string& items)
{
    try
    {
        return exa_expand_(items);
    }
    catch (...)
    {
        throw string("Unable to parse: ").append(items);
    }
}


std::set<std::string> exa_unexpand(const std::string &list)
{
    try
    {
        return exa_unexpand_(list);
    }
    catch (...)
    {
        throw string("Unable to unexpand: ").append(list);
    }
}

