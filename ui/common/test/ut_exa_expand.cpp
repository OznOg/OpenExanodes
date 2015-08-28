/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include <unit_testing.h>

#include <string>
#include <set>

#include "common/include/exa_constants.h"
#include "os/include/os_inttypes.h"
#include "os/include/os_string.h"
#include "ui/common/include/exa_expand.h"
#include "ui/common/include/common_utils.h"

using std::string;
using std::set;


static set<string> test_expansion(std::size_t expected_nb_items, const string &nodes)
{
    try
    {
        ut_printf("Check '%s' expansion returns %zu item", nodes.c_str(), expected_nb_items);
        set<string> list(exa_expand(nodes));
        UT_ASSERT_EQUAL(expected_nb_items, list.size());
        return list;
    }
    catch (...)
    {
        UT_FAIL();
    }
}


static void test_unexpansion(std::size_t expected_nb_items, const set<string>& list)
{
    try
    {
        string tmp(strjoin(" ", list));
        ut_printf("Check '%s' unexpansion returns %zu item", tmp.c_str(), expected_nb_items);
        UT_ASSERT_EQUAL(expected_nb_items, exa_unexpand(tmp).size());
    }
    catch (...)
    {
        UT_FAIL();
    }
}


static void test_expansion_exception(const string &nodes)
{
    try
    {
        ut_printf("Check '%s' expansion throws an exception", nodes.c_str());
        exa_expand(nodes);
        UT_FAIL();
    }
    catch (const string& thrown)
    {
        /* The message thrown must contain the expression that we couldn't expand */
        UT_ASSERT(thrown.find(nodes) != std::string::npos);
    }
    catch (...)
    {
        UT_FAIL();
    }
}


static void test_unexpansion_exception(const string &str)
{
    try
    {
        exa_unexpand(str);
        UT_FAIL();
    }
    catch (const string& thrown)
    {
        /* The message thrown must contain the expression that we couldn't unexpand */
        UT_ASSERT(thrown.find(str) != std::string::npos);
    }
    catch (...)
    {
        UT_FAIL();
    }
}


struct exa_expand_compare_lt
{
  bool operator() (const string &item1, const string &item2) const
  {
    return os_strverscmp(item1.c_str(), item2.c_str()) < 0;
  }
};

typedef set<string, exa_expand_compare_lt> set_sorted_t;


/**
 * @brief Success cases.
 */
ut_test(expression_expansion)
{
    set<string> tmp;
    set<string>::iterator it;

    tmp = test_expansion(1, "s1");
    test_unexpansion(1, tmp);

    /* Try a longer list. */
    tmp = test_expansion(31, "o185i/211-212:214-215:217-221:223-224:226-227:229:231-232:234-235:237-242:244:246-247:249-251:255/");
    test_unexpansion(1, tmp);

    tmp = test_expansion(5, "sam1v1v/1-3/ sam1v2v/1-2/");
    test_unexpansion(2, tmp);

    tmp = test_expansion(14, "o1i/234-235:237-242:244:246-247:249-251/");
    test_unexpansion(1, tmp);

    tmp = test_expansion(129, "o1i/1:2:3:4:5:6:7:8:9:10:11:12:13:14:15:16:17:18:19:20:21:22:23:24:25:26:27:28:29:30:31:32:33:34:35:36:37:38:39:40:41:42:43:44:45:46:47:48:49:50:51:52:53:54:55:56:57:58:59:60:61:62:63:64:65:66:67:68:69:70:71:72:73:74:75:76:77:78:79:80:81:82:83:84:85:86:87:88:89:90:91:92:93:94:95:96:97:98:99:100:101:102:103:104:105:106:107:108:109:110:111:112:113:114:115:116:117:118:119:120:121:122:123:124:125:126:127:128:129/");
    test_unexpansion(1, tmp);

    tmp = test_expansion(3, "sam/00-02/");
    for (it = tmp.begin(); it != tmp.end(); it++)
    {
        UT_ASSERT((it->compare("sam00") == 0)
                ||(it->compare("sam01") == 0)
                ||(it->compare("sam02") == 0));
    }
    test_unexpansion(1, tmp);

    tmp = test_expansion(3, "oli/1-3/");
    for (it = tmp.begin(); it != tmp.end(); it++)
    {
        UT_ASSERT((it->compare("oli1") == 0)
                ||(it->compare("oli2") == 0)
                ||(it->compare("oli3") == 0));
    }

    tmp = test_expansion(11, "oli/0-10/");
    for (it = tmp.begin(); it != tmp.end(); it++)
    {
        UT_ASSERT((it->compare("oli0") == 0)
                ||(it->compare("oli1") == 0)
                ||(it->compare("oli2") == 0)
                ||(it->compare("oli3") == 0)
                ||(it->compare("oli4") == 0)
                ||(it->compare("oli5") == 0)
                ||(it->compare("oli6") == 0)
                ||(it->compare("oli7") == 0)
                ||(it->compare("oli8") == 0)
                ||(it->compare("oli9") == 0)
                ||(it->compare("oli10") == 0));
    }

    tmp = test_expansion(11, "oli/00-10/              ");
    for (it = tmp.begin(); it != tmp.end(); it++)
    {
        UT_ASSERT((it->compare("oli00") == 0)
                ||(it->compare("oli01") == 0)
                ||(it->compare("oli02") == 0)
                ||(it->compare("oli03") == 0)
                ||(it->compare("oli04") == 0)
                ||(it->compare("oli05") == 0)
                ||(it->compare("oli06") == 0)
                ||(it->compare("oli07") == 0)
                ||(it->compare("oli08") == 0)
                ||(it->compare("oli09") == 0)
                ||(it->compare("oli10") == 0));
    }
}


/**
 * @brief Invalid syntax tests.
 */
ut_test(expansion_exception)
{
    test_expansion_exception("/1-2///1");
    test_expansion_exception("sam/::/");
    test_expansion_exception("sam/--/");
    test_expansion_exception("sam/6-/");
    test_expansion_exception("sam/-6/");
    test_expansion_exception("sam/6-7");
    test_expansion_exception("sam/7-8-/");
    test_expansion_exception("sam/7-8-/");
    test_expansion_exception("sam/7-8:/");
    test_expansion_exception("sam//");
    test_expansion_exception("sam/1-01/");
    test_expansion_exception("sam/01-1/");

    /* TODO add more test cases */
    test_unexpansion_exception("\255");
}


ut_test(list_sort)
{
    /* Check the set is sorted */
    string to_expand("3 0 2 sam0 sam2 sam0a sam1 sam0b sam1a xen76v1 xen75v1");
    set_sorted_t sorted;
    set<string> to_sort = exa_expand(to_expand);
    string result;

    for (set<string>::iterator index_set = to_sort.begin();
         index_set != to_sort.end(); index_set++)
        sorted.insert(*index_set);

    for (set_sorted_t::iterator index_set = sorted.begin();
         index_set != sorted.end(); index_set++)
        result += " " + *index_set;

    UT_ASSERT_EQUAL_STR(" 0 2 3 sam0 sam0a sam0b sam1 sam1a sam2 xen75v1 xen76v1", result.c_str());
}


/**
 * Check the presence of one element and then erase it from the set
 * @param[in][out]  str_set         the string set
 * @param[in]       element_value   the value of the element
 */
void extract_element(set<string>& str_set, const string& element_value)
{
    set<string>::iterator it;
    it = str_set.find(element_value);
    if (it == str_set.end())
    {
        ut_printf("ERROR: Can not extract element '%s'",
                  element_value.c_str());
        UT_FAIL();
    }

    str_set.erase(it);
}


ut_test(bug_4143)
{
    {
        string to_expand("vmware/70:71:73/v1 vmware/71:73/v2 vmware71v3");
        set<string> str_set = exa_expand(to_expand);

        extract_element(str_set, string("vmware71v3"));
        extract_element(str_set, string("vmware71v2"));
        extract_element(str_set, string("vmware73v2"));
        extract_element(str_set, string("vmware70v1"));
        extract_element(str_set, string("vmware71v1"));
        extract_element(str_set, string("vmware73v1"));

        UT_ASSERT_EQUAL(0, str_set.size());
    }

    {
        string to_expand("vmware71v3 vmware/70:71:73/v1 vmware/71:73/v2");
        set<string> str_set = exa_expand(to_expand);

        extract_element(str_set, string("vmware71v3"));
        extract_element(str_set, string("vmware70v1"));
        extract_element(str_set, string("vmware71v1"));
        extract_element(str_set, string("vmware73v1"));
        extract_element(str_set, string("vmware71v2"));
        extract_element(str_set, string("vmware73v2"));

        UT_ASSERT_EQUAL(0, str_set.size());
    }
}

