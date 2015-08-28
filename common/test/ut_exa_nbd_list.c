/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <unit_testing.h>
#include "common/include/exa_nbd_list.h"

UT_SECTION(nbd_list)

ut_setup()
{
}

ut_cleanup()
{
}

struct elt {
   char data[7];
   char stuff[3];
};

ut_test(create_and_delete_root_list)
{
    struct nbd_root_list root_list;

    int err = nbd_init_root(/*nb_elt*/ 37, sizeof(struct elt), &root_list);
    UT_ASSERT_EQUAL(0, err);

    nbd_close_root(&root_list);
}

ut_test(create_and_delete_root_list_and_list)
{
    struct nbd_root_list root_list;
    struct nbd_list list;

    int err = nbd_init_root(/*nb_elt*/ 37, sizeof(struct elt), &root_list);
    UT_ASSERT_EQUAL(0, err);

    err = nbd_init_list(&root_list, &list);
    UT_ASSERT_EQUAL(0, err);

    nbd_close_list(&list);
    nbd_close_root(&root_list);
}

ut_test(write_in_all_elements_of_list)
{
    struct nbd_root_list root_list;
    struct nbd_list list;
    const int nb_elts = 37;
    int i;

    int err = nbd_init_root(nb_elts, sizeof(struct elt), &root_list);
    UT_ASSERT_EQUAL(0, err);

    err = nbd_init_list(&root_list, &list);
    UT_ASSERT_EQUAL(0, err);

    for (i = 0; i < nb_elts; i++)
    {
        int idx;
        struct elt *elt = nbd_list_remove(&root_list.free, &idx, LISTWAIT);
        memset(elt, 0xEE, sizeof(*elt));
        nbd_list_post(&list, elt, -1);
    }

    nbd_close_list(&list);
    nbd_close_root(&root_list);
}

