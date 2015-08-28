/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>

#include "csupd/test/sup_helpers.h"

#include "os/include/os_inttypes.h"
#include "os/include/os_error.h"

#include <stdlib.h>
#include <stdio.h>

/**
 * Helper function for reading a given <node id, mship> file.
 *
 * \param[in] filename        Name of file to read
 * \param[in] check_expected  Whether to check for expected mships
 *
 * \return 0 if successfull, negative error code otherwise
 */
static int
__read_file(const char *filename, bool check_expected)
{
    char path[512];
    unsigned num_nodes;
    exa_nodeset_t *mships, *expected;
    FILE *f;
    char *srcdir = getenv("srcdir");
    int r;

    sprintf(path, "%s/mship_files/%s", srcdir ? srcdir : ".", filename);
    f = fopen(path, "rt");
    if (f == NULL)
    {
        printf("failed opening file '%s'\n", path);
        r = -errno;
    }
    else
    {
        r = read_mship_set(f, &num_nodes, &mships,
                           check_expected ? &expected : NULL);
        if (r < 0)
            return r;

        free(mships);
        if (check_expected)
            free(expected);

        fclose(f);
    }

    return r;
}

/**
 * Read a well-formed <node id, mship> file.
 */
ut_test(read_wellformed_file)
{
    UT_ASSERT(__read_file("wellformed.txt", false) == 0);
}

/**
 * Read a <node id, mship> file with a node appearing more than once.
 */
ut_test(read_dupnode_file)
{
    UT_ASSERT(__read_file("dupnode.txt", false) == -EEXIST);
}

/**
 * Read a <node id, mship> file with an invalid membership.
 */
ut_test(read_badmship_file)
{
    UT_ASSERT(__read_file("badmship.txt", false) == -EINVAL);
}

/**
 * Read a <node id, mship> file with too long a membership.
 */
ut_test(read_toolong_file)
{
    UT_ASSERT(__read_file("toolong.txt", false) == -E2BIG);
}

/**
 * Read a <node id, mship> file with a membership not containing
 * the sender node.
 */
ut_test(read_impossible_file)
{
    UT_ASSERT(__read_file("impossible.txt", false) == -EILSEQ);
}

/**
 * Read a <node id, mship> file with a missing expected
 * membership.
 */
ut_test(read_noexpected_file)
{
    UT_ASSERT(__read_file("noexpected.txt", true) == -ESRCH);
}

/**
 * The expected membership is incoherent (without itself).
 */
ut_test(read_incoherent_mship_file)
{
    UT_ASSERT(__read_file("incoherent_expect.txt", true) == -EILSEQ);
}

/**
 * The expected membership is incoherent (with unreachable nodes).
 */
ut_test(read_incoherent_mship_file2)
{
    UT_ASSERT(__read_file("incoherent_expect2.txt", true) == -EPROTO);
}
