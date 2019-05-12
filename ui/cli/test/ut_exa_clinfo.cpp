/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include <unit_testing.h>
#include "ui/cli/src/exa_clinfo.h"
#include "ui/common/include/cli_log.h"
#include "common/include/exa_config.h"
#include "common/include/exa_constants.h"
#include <memory>
#include <set>
#include <string>
#include <sstream>


using std::istringstream;
using std::string;

/**
 * Purpose:
 *   The goal of this function is to verify that
 *   the output to stdout matches what is expected
 *   in term of the match of each word on each line.
 *   What it does not verify is the match of the alignment of words.
 *
 * Implementation:
 *   This function compares 2 strings in the following way:
 *     It compares each line extracted from the 2 strings in the following way:
 *       It compares each word extracted from the 2 lines.
 *
 * Example:
 * __________ Expected __________|___________ Actually got ___________
 *   VOLUMES:                    |  VOLUMES:
 *   dg_test:vl_test 10.0G       |  dg_test:vl_test             10.0G SHARED   /dev/exa/dg_test/vl_test
 *   iSCSI SHARED | LUN 123      |  EXPORTED     sam60
 *   EXPORTED sam60              |
 * ______________________________|____________________________________
 *
 * We MUST have assertion failed ('', 'SHARED')
 *
 * @param[in] _expected     The expected string
 * @param[in] _got          The actually got string
 *
 */
static void assert_equal_output(const string& _expected,
                                const string& _got)
{
    istringstream expected(_expected);
    istringstream got(_got);

    while (! (expected.eof() && got.eof()))
    {
        string expected_buf;
        std::getline(expected, expected_buf);
        istringstream expected_line(expected_buf);

        string got_buf;
        std::getline(got, got_buf);
        istringstream got_line(got_buf);

        while (! (expected_line.eof() && got_line.eof()))
        {
            char expected_word[EXA_MAXSIZE_LINE];
            memset(expected_word, 0, sizeof(expected_word));
            expected_line >> expected_word;

            char got_word[EXA_MAXSIZE_LINE];
            memset(got_word, 0, sizeof(got_word));
            got_line >> got_word;

            UT_ASSERT_EQUAL_STR(expected_word, got_word);
        }
    }
}


/**
 * A dummy class allows testing of the
 * protected functions like exa_clinfo::exa_display_volumes_status
 */
class __exa_clinfo : public exa_clinfo
{
public:
    using exa_clinfo::exa_display_volumes_status;
#ifdef WITH_FS
    using exa_clinfo::exa_display_fs_status;
#endif
};


/**
 * This stream replaces the stdout stream
 * for the output of exa_clinfo::exa_display_volumes_status
 */
extern std::stringstream __stdout;


typedef
void (exa_clinfo::*display_func_t)(std::shared_ptr<xmlDoc>,
        std::set<xmlNodePtr, exa_clinfo::exa_cmp_xmlnode_lt> &);

/**
 * With a given XML input, checks the output to stdout
 */
static void test_display_info(const std::string& expected_output,
                              const std::string& xml_input,
                              display_func_t display_func)
{
    // Build XML doc
    std::shared_ptr<xmlDoc> config(
        xmlReadMemory(
        xml_input.c_str(), xml_input.size(),
        NULL, NULL, XML_PARSE_NOBLANKS | XML_PARSE_NOERROR | XML_PARSE_NOWARNING));

    UT_ASSERT(config);
    UT_ASSERT(config->children);
    UT_ASSERT(config->children->name);
    UT_ASSERT(xmlStrEqual(config->children->name, BAD_CAST("Exanodes")));

    // Prepare the node set to test exa_display_volumes_status
    std::set<xmlNodePtr, exa_clinfo::exa_cmp_xmlnode_lt> nodelist;
    {
        xmlNodePtr nodeptr;
        int i;

        xmlNodeSetPtr nodesSet =
            xml_conf_xpath_query (config.get(), "//cluster/node");

        xml_conf_xpath_result_for_each(nodesSet, nodeptr, i)
            nodelist.insert(nodeptr);
    }

    // Create the command
    char* argv[] =
    {
        const_cast<char*>("exa_clinfo"),
        const_cast<char*>("cl_test")
    };
    int argc = sizeof(argv)/sizeof(char*);

    __exa_clinfo cmd;

    cmd.parse(argc, argv);

    // Clear __stdout to keep only output of exa_display_volumes_status
    __stdout.str("");

    (cmd.*display_func)(config, nodelist);

    printf("================================ STDOUT ================================\n");
    printf("%s", __stdout.str().c_str());
    printf("========================================================================\n");

    assert_equal_output(expected_output, __stdout.str());
}


ut_test(display_volume_status_all_started)
{
    const string sample(
"<?xml version=\"1.0\"?>\n"
"<Exanodes>\n"
  "<cluster name=\"cl_test\" uuid=\"F68A9FC0:0C3C97AA:72B85198:EA27263C\" in_recovery=\"FALSE\" license_expired=\"FALSE\">\n"
    "<node name=\"sam60\" hostname=\"sam60\">\n"
      "<disk uuid=\"A0B73F19:08662509:ED07AEF7:253036E2\" path=\"/dev/sdb\" status=\"UP\" size=\"300088631296\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
  "</cluster>\n"
  "<diskgroup transaction=\"COMMITTED\" name=\"dg_test\" layout=\"sstriping\" goal=\"STARTED\" nb_volumes=\"1\" status=\"OK\" tainted=\"FALSE\" rebuilding=\"FALSE\" size_used=\"10737418240\" usable_capacity=\"298231791616\" nb_spare=\"0\" nb_spare_available=\"0\" slot_width=\"1\" su_size=\"1024\" chunk_size=\"262144\" nb_disks=\"1\" administrable=\"TRUE\">\n"
    "<logical>\n"
      "<volume name=\"vl_test\" accessmode=\"SHARED\" transaction=\"COMMITTED\" size=\"10737418240\""
	 " goal_started=\"sam60 sam66 sam67\" status_stopped=\"sam66\" status_started=\"sam60 sam66 sam67\" >\n"
	 "<export method=\"iSCSI\" id_type=\"LUN\" id_value=\"123\"/>\n"
      "</volume>\n"
    "</logical>\n"
  "</diskgroup>\n"
"</Exanodes>"
);

    const string expected(
"VOLUMES:\n"
"dg_test:vl_test 10.0G SHARED\n"
"iSCSI | LUN 123\n"
"EXPORTED sam60 sam66 sam67\n"
);

    test_display_info(expected, sample,
                      &__exa_clinfo::exa_display_volumes_status);
}

ut_test(display_volume_status_will_export)
{
    const string sample(
"<?xml version=\"1.0\"?>\n"
"<Exanodes>\n"
  "<cluster name=\"cl_test\" uuid=\"CEFF2D90:32DEDC69:0D4D4C70:457ABF07\" in_recovery=\"FALSE\" license_expired=\"FALSE\">\n"
    "<node name=\"sam60\" hostname=\"sam60\">\n"
      "<disk uuid=\"33C5D6C1:05528FAC:BF6CB085:63DD9744\" path=\"/dev/sdb\" status=\"UP\" size=\"300088631296\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
    "<node name=\"sam66\" hostname=\"sam66\">\n"
      "<disk uuid=\"F7C42DCE:0E3715EB:E4EDDE4D:6C7C798B\" status=\"DOWN\"/>\n"
    "</node>\n"
    "<node name=\"sam67\" hostname=\"sam67\">\n"
      "<disk uuid=\"1F803E18:06F29FE8:9C34EFA5:E165CEB2\" path=\"/dev/sdb\" status=\"UP\" size=\"79997902848\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
  "</cluster>\n"
  "<diskgroup transaction=\"COMMITTED\" name=\"dg_test\" layout=\"rainX\" goal=\"STARTED\" nb_volumes=\"1\" status=\"DEGRADED\" tainted=\"FALSE\" rebuilding=\"FALSE\" size_used=\"10871635968\" usable_capacity=\"118380036096\" nb_spare=\"0\" nb_spare_available=\"0\" slot_width=\"3\" su_size=\"1024\" chunk_size=\"262144\" dirty_zone_size=\"32768\" blended_stripes=\"FALSE\" nb_disks=\"3\" administrable=\"TRUE\">\n"
    "<logical>\n"
      "<volume name=\"vl_test\" accessmode=\"SHARED\" transaction=\"COMMITTED\" size=\"10737418240\""
	 " goal_started=\"sam60 sam66 sam67\" status_stopped=\"sam66\" status_started=\"sam60 sam67\" >\n"
	 "<export method=\"bdev\" id_type=\"\" id_value=\"/dev/exa/dg_test/vl_test\" />\n"
      "</volume>\n"
    "</logical>\n"
  "</diskgroup>\n"
"</Exanodes>"
);

    const string expected(
"VOLUMES:\n"
"dg_test:vl_test 10.0G SHARED\n"
"bdev | /dev/exa/dg_test/vl_test\n"
"EXPORTED sam60 sam67\n"
"WILL EXPORT sam66\n"
);

    test_display_info(expected, sample,
                      &__exa_clinfo::exa_display_volumes_status);
}


#ifdef WITH_FS
static const std::string xml_clinfo_with_fs(
"<?xml version=\"1.0\"?>\n"
"<Exanodes>\n"
  "<cluster name=\"cl_test\" uuid=\"280A3E2F:552CD22A:8DE80835:81E9C14D\" in_recovery=\"FALSE\" license_expired=\"FALSE\">\n"
    "<node name=\"sam60\" hostname=\"sam60\">\n"
      "<disk uuid=\"56D8567B:6ECA5EA5:EC57F141:63A64496\" path=\"/dev/sdb\" status=\"UP\" size=\"300088631296\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
    "<node name=\"sam66\" hostname=\"sam66\">\n"
      "<disk uuid=\"CF7A8D21:4856A4F5:8F836BEF:55A61D5D\" path=\"/dev/sdb\" status=\"UP\" size=\"79997902848\" throughput_write=\"0\" throughput_read=\"4\"/>\n"
    "</node>\n"
    "<node name=\"sam67\" hostname=\"sam67\">\n"
      "<disk uuid=\"6DDC58D8:C8D3E5A6:A386E260:5FDD83DE\" path=\"/dev/sdb\" status=\"UP\" size=\"79997902848\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
  "</cluster>\n"
  "<diskgroup transaction=\"COMMITTED\" name=\"dg_test\" layout=\"sstriping\" goal=\"STARTED\" nb_volumes=\"1\" status=\"OK\" tainted=\"FALSE\" rebuilding=\"FALSE\" size_used=\"1073741824\" usable_capacity=\"79188459520\" nb_spare=\"0\" nb_spare_available=\"0\" slot_width=\"1\" su_size=\"1024\" chunk_size=\"262144\" nb_disks=\"1\" administrable=\"TRUE\">\n"
    "<logical>\n"
      "<volume name=\"fs_test\" accessmode=\"SHARED\" transaction=\"COMMITTED\" size=\"1073741824\""
        " goal_started=\"sam60\" status_stopped=\"sam66 sam67\" status_in_use=\"sam60\" >\n"
	 "<export method=\"bdev\" id_type=\"\" id_value=\"/dev/exa/dg_test/fs_test\" />\n"
        "<fs handle_sfs=\"OK\" transaction=\"COMMITTED\" type=\"sfs\" mountpoint=\"/mnt/fs_test\" goal_started=\"sam60 sam66 sam67\" status_mounted=\"sam60\"/>\n"
      "</volume>\n"
    "</logical>\n"
  "</diskgroup>\n"
"</Exanodes>"
);


/**
 * Regression test
 */
ut_test(display_volume_status_fs)
{
    const string expected(
"VOLUMES:\n"
" dg_test:fs_test             1.0G  SHARED\n"
" bdev | /dev/exa/dg_test/fs_test\n"
"                IN USE       sam60\n"
"\n"
);
    test_display_info(expected, xml_clinfo_with_fs,
                      &__exa_clinfo::exa_display_volumes_status);
}


/**
 * Regression test
 */
ut_test(display_fs_status)
{
    const string expected(
"FILE SYSTEMS                 SIZE  USED  AVAIL TYPE     OPTIONS MOUNTPOINT\n"
" dg_test:fs_test             (NO INFO)         sfs              /mnt/fs_test\n"
"                MOUNTED      sam60\n"
"\n"
);
    test_display_info(expected, xml_clinfo_with_fs,
                      &__exa_clinfo::exa_display_fs_status);
}
#endif


ut_test(display_volume_status_all_stopped)
{
    const string sample(
"<?xml version=\"1.0\"?>\n"
"<Exanodes>\n"
  "<cluster name=\"cl_test\" uuid=\"F68A9FC0:0C3C97AA:72B85198:EA27263C\" in_recovery=\"FALSE\" license_expired=\"FALSE\">\n"
    "<node name=\"sam60\" hostname=\"sam60\">\n"
      "<disk uuid=\"A0B73F19:08662509:ED07AEF7:253036E2\" path=\"/dev/sdb\" status=\"UP\" size=\"300088631296\" throughput_write=\"0\" throughput_read=\"0\"/>\n"
    "</node>\n"
  "</cluster>\n"
  "<diskgroup transaction=\"COMMITTED\" name=\"dg_test\" layout=\"sstriping\" goal=\"STARTED\" nb_volumes=\"1\" status=\"OK\" tainted=\"FALSE\" rebuilding=\"FALSE\" size_used=\"10737418240\" usable_capacity=\"298231791616\" nb_spare=\"0\" nb_spare_available=\"0\" slot_width=\"1\" su_size=\"1024\" chunk_size=\"262144\" nb_disks=\"1\" administrable=\"TRUE\">\n"
    "<logical>\n"
       "<volume name=\"vl_test\" accessmode=\"SHARED\" transaction=\"COMMITTED\" size=\"10737418240\""
       " goal_stopped=\"sam60\" status_stopped=\"sam60\" >\n"
         "<export method=\"iSCSI\" id_type=\"LUN\" id_value=\"0\" />\n"
    "</volume>\n"
    "</logical>\n"
  "</diskgroup>\n"
"</Exanodes>"
);
    const string expected(
"VOLUMES:\n"
"dg_test:vl_test 10.0G SHARED\n"
"iSCSI | LUN 0\n"
"\n"
);
    test_display_info(expected, sample,
                      &__exa_clinfo::exa_display_volumes_status);
}

